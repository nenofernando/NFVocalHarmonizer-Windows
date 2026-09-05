#include "PluginProcessor.h"
#include "PluginEditor.h"

NFVocalHarmonizerAudioProcessor::NFVocalHarmonizerAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "NF_VOCAL_HARMONIZER_STATE", nf::params::createLayout()), presets(apvts)
{
    slotA.addChild(apvts.copyState(), -1, nullptr);
    slotB.addChild(apvts.copyState(), -1, nullptr);
    startTimerHz(20);
}

NFVocalHarmonizerAudioProcessor::~NFVocalHarmonizerAudioProcessor()
{
    stopTimer();
}

void NFVocalHarmonizerAudioProcessor::prepareToPlay(double sr, int block)
{
    engine.prepare(sr, block, getTotalNumOutputChannels());
    setLatencySamples(engine.getLatencySamples());
    capture.ring().reset();
}

void NFVocalHarmonizerAudioProcessor::releaseResources() { engine.reset(); }

bool NFVocalHarmonizerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo()) && in == out;
}

nf::dsp::HarmonySettings NFVocalHarmonizerAudioProcessor::readSettings() const
{
    const auto value = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    nf::dsp::HarmonySettings s;
    s.intervalChoice = juce::roundToInt(value(nf::params::interval));
    s.harmonyPercent = value(nf::params::harmony);
    s.formantSemitones = value(nf::params::formant);
    s.humanizePercent = value(nf::params::humanize);
    s.widthPercent = value(nf::params::width);
    s.mixPercent = value(nf::params::mix);
    s.key = juce::roundToInt(value(nf::params::key));
    s.scale = static_cast<nf::dsp::ScaleType>(juce::roundToInt(value(nf::params::scale)));
    s.autoKey = value(nf::params::autoKey) > 0.5f;
    s.enabled = value(nf::params::enabled) > 0.5f;
    return s;
}

std::pair<int, nf::dsp::ScaleType> NFVocalHarmonizerAudioProcessor::activeKeyScale() const
{
    auto root = juce::roundToInt(apvts.getRawParameterValue(nf::params::key)->load());
    auto scale = static_cast<nf::dsp::ScaleType>(juce::roundToInt(apvts.getRawParameterValue(nf::params::scale)->load()));
    if (apvts.getRawParameterValue(nf::params::autoKey)->load() > 0.5f)
    {
        const auto detected = engine.getDetectedScale();
        if (detected.confidence > 0.20f)
        {
            root = detected.root;
            scale = detected.type;
        }
    }
    return { root, scale };
}

void NFVocalHarmonizerAudioProcessor::setParkedPlayheadSeconds(double timeSec) noexcept
{
    const double t = juce::jmax(0.0, timeSec);
    parkedPlayheadSec.store(t, std::memory_order_relaxed);
    hasParkedPlayhead.store(true, std::memory_order_relaxed);
    if (! hostPlaying.load(std::memory_order_relaxed))
        hostTimeSec.store(t, std::memory_order_relaxed);
}

void NFVocalHarmonizerAudioProcessor::beginAnalyzeCapture()
{
    noteModel.clear();
    capture.clearRaw();
    capture.ring().reset();
    capture.setArmed(true);
    engine.resetKeyAnalysis();

    // Clear any previous completed/failed visual before arming again.
    const bool playing = hostPlaying.load(std::memory_order_relaxed);
    analysisState.store(analysisStateForBegin(playing), std::memory_order_relaxed);
    wasPlaying = playing;
}

void NFVocalHarmonizerAudioProcessor::finalizeAnalyzeCapture()
{
    capture.setArmed(false);
    capture.drainRing();
    const auto settings = readSettings();
    const auto key = activeKeyScale();
    auto notes = capture.buildNotes(settings.intervalChoice, key.first, key.second);
    const bool hasNotes = ! notes.empty();
    noteModel.setNotes(std::move(notes));
    analysisState.store(analysisStateForFinalize(hasNotes), std::memory_order_relaxed);
}

void NFVocalHarmonizerAudioProcessor::timerCallback()
{
    const bool playing = hostPlaying.load(std::memory_order_relaxed);
    const auto state = analysisState.load(std::memory_order_relaxed);

    if (capture.isArmed())
    {
        capture.drainRing();

        // Armed + host starts playing → analyzing (spacebar / transport button).
        const auto next = analysisStateAfterArmedSeesPlay(state, playing);
        if (next != state)
            analysisState.store(next, std::memory_order_relaxed);

        // Play → Stop finalizes. Loop/seek while still playing must not finalize.
        if (analysisShouldFinalizeOnStop(wasPlaying, playing))
            finalizeAnalyzeCapture();
    }

    wasPlaying = playing;
}

void NFVocalHarmonizerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals guard;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    double timeSec = hostTimeSec.load(std::memory_order_relaxed);
    bool playing = false;
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto samples = pos->getTimeInSamples())
            {
                const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
                timeSec = static_cast<double>(*samples) / sr;
            }
            else if (auto seconds = pos->getTimeInSeconds())
            {
                timeSec = *seconds;
            }
        }
    }

    // Many hosts jump the playhead back to 0 (or cycle start) on stop.
    // Keep the last playing position / user scrub so the editor cursor stays put.
    if (playing)
    {
        lastPlayingTimeSec.store(timeSec, std::memory_order_relaxed);
    }
    else if (audioWasPlaying)
    {
        const double park = lastPlayingTimeSec.load(std::memory_order_relaxed);
        parkedPlayheadSec.store(park, std::memory_order_relaxed);
        hasParkedPlayhead.store(true, std::memory_order_relaxed);
        timeSec = park;
    }
    else if (hasParkedPlayhead.load(std::memory_order_relaxed))
    {
        timeSec = parkedPlayheadSec.load(std::memory_order_relaxed);
    }

    audioWasPlaying = playing;
    hostPlaying.store(playing, std::memory_order_relaxed);
    hostTimeSec.store(timeSec, std::memory_order_relaxed);

    if (apvts.getRawParameterValue(nf::params::power)->load() < 0.5f)
    {
        processBlockBypassed(buffer, midi);
        return;
    }

    engine.process(buffer, readSettings(), timeSec, &noteModel.getOffsetTable(),
                   &capture.ring(), capture.isArmed() && playing);
}

void NFVocalHarmonizerAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto s = readSettings(); s.enabled = false;
    engine.process(buffer, s, hostTimeSec.load(std::memory_order_relaxed), &noteModel.getOffsetTable(),
                   nullptr, false);
}

juce::AudioProcessorEditor* NFVocalHarmonizerAudioProcessor::createEditor()
{ return new NFVocalHarmonizerAudioProcessorEditor(*this); }

void NFVocalHarmonizerAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto root = apvts.copyState();
    root.setProperty("schemaVersion", 3, nullptr);
    root.addChild(slotA.createCopy(), -1, nullptr);
    root.addChild(slotB.createCopy(), -1, nullptr);
    // Session-only note edits: stored with the instance, not with user presets.
    root.addChild(noteModel.toValueTree(), -1, nullptr);
    // Completed visual may persist only together with a saved note map.
    if (analysisState.load(std::memory_order_relaxed) == AnalysisState::completed
        && ! noteModel.getNotes().empty())
        root.setProperty("analysisCompleted", true, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary(*xml, dest);
}

void NFVocalHarmonizerAudioProcessor::setStateInformation(const void* data, int bytes)
{
    const auto xml = getXmlFromBinary(data, bytes);
    if (xml == nullptr || ! xml->hasTagName(apvts.state.getType())) return;
    auto root = juce::ValueTree::fromXml(*xml);
    if (! root.isValid()) return;
    const auto a = root.getChildWithName("SLOT_A");
    const auto b = root.getChildWithName("SLOT_B");
    const auto edits = root.getChildWithName("HARMONY_NOTE_EDITS");
    if (a.isValid()) { slotA = a.createCopy(); root.removeChild(a, nullptr); }
    if (b.isValid()) { slotB = b.createCopy(); root.removeChild(b, nullptr); }
    if (edits.isValid())
    {
        noteModel.fromValueTree(edits);
        root.removeChild(edits, nullptr);
    }
    // Restore completed indicator only when notes are present; never restore live arm/pulse.
    if (static_cast<bool>(root.getProperty("analysisCompleted", false))
        && ! noteModel.getNotes().empty())
        analysisState.store(AnalysisState::completed, std::memory_order_relaxed);
    else if (noteModel.getNotes().empty())
        analysisState.store(AnalysisState::idle, std::memory_order_relaxed);
    root.removeProperty("analysisCompleted", nullptr);
    apvts.replaceState(root);
}

void NFVocalHarmonizerAudioProcessor::captureSlot(bool a)
{
    auto& slot = a ? slotA : slotB;
    slot.removeAllChildren(nullptr);
    slot.addChild(apvts.copyState(), -1, nullptr);
}
void NFVocalHarmonizerAudioProcessor::restoreSlot(bool a)
{
    const auto& slot = a ? slotA : slotB;
    if (slot.getNumChildren() > 0)
        apvts.replaceState(slot.getChild(0).createCopy());
}
void NFVocalHarmonizerAudioProcessor::copySlot(bool fromA)
{
    auto& destination = fromA ? slotB : slotA;
    const auto& source = fromA ? slotA : slotB;
    destination.removeAllChildren(nullptr);
    if (source.getNumChildren() > 0)
        destination.addChild(source.getChild(0).createCopy(), -1, nullptr);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{ return new NFVocalHarmonizerAudioProcessor(); }
