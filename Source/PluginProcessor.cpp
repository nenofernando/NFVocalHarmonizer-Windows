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
    transportSampleRate.store(sr > 0.0 ? sr : 44100.0, std::memory_order_relaxed);
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

void NFVocalHarmonizerAudioProcessor::captureHostTransportForEditor() noexcept
{
    transportSampleRate.store(getSampleRate() > 0.0 ? getSampleRate() : 44100.0,
                              std::memory_order_relaxed);

    auto* playHead = getPlayHead();
    if (playHead == nullptr)
    {
        transportPositionValid.store(false, std::memory_order_release);
        return;
    }

    const auto position = playHead->getPosition();
    if (! position.hasValue())
    {
        transportPositionValid.store(false, std::memory_order_release);
        return;
    }

    const bool playing = position->getIsPlaying();
    const auto optionalSample = position->getTimeInSamples();

    // Odd revision = write in progress; even = stable.
    transportRevision.fetch_add(1, std::memory_order_acq_rel);

    transportIsPlaying.store(playing, std::memory_order_relaxed);

    if (optionalSample.hasValue())
    {
        const int64_t sample = juce::jmax<int64_t>(0, *optionalSample);
        transportCurrentSample.store(sample, std::memory_order_relaxed);
        transportPositionValid.store(true, std::memory_order_relaxed);

        // CRITICAL: only update last valid point while playing.
        // If the DAW stops and returns zero, this field stays frozen.
        if (playing)
            transportLastPlayingSample.store(sample, std::memory_order_relaxed);
    }
    else
    {
        // Fallback: seconds → samples when host omits sample position.
        if (auto seconds = position->getTimeInSeconds())
        {
            const double sr = transportSampleRate.load(std::memory_order_relaxed);
            const int64_t sample = juce::jmax<int64_t>(
                0, static_cast<int64_t>(std::llround(*seconds * sr)));
            transportCurrentSample.store(sample, std::memory_order_relaxed);
            transportPositionValid.store(true, std::memory_order_relaxed);
            if (playing)
                transportLastPlayingSample.store(sample, std::memory_order_relaxed);
        }
        else
        {
            transportPositionValid.store(false, std::memory_order_relaxed);
        }
    }

    transportRevision.fetch_add(1, std::memory_order_release);
}

NFVocalHarmonizerAudioProcessor::TransportSnapshot
NFVocalHarmonizerAudioProcessor::getTransportSnapshot() const noexcept
{
    TransportSnapshot result;

    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const auto before = transportRevision.load(std::memory_order_acquire);
        if ((before & 1u) != 0u)
            continue;

        result.currentHostSample = transportCurrentSample.load(std::memory_order_relaxed);
        result.lastValidPlayingSample = transportLastPlayingSample.load(std::memory_order_relaxed);
        result.analysisStartHostSample = transportAnalysisStartSample.load(std::memory_order_relaxed);
        result.sampleRate = transportSampleRate.load(std::memory_order_relaxed);
        result.isPlaying = transportIsPlaying.load(std::memory_order_relaxed);
        result.hasValidHostPosition = transportPositionValid.load(std::memory_order_relaxed);

        const auto after = transportRevision.load(std::memory_order_acquire);
        if (before == after && (after & 1u) == 0u)
        {
            result.revision = after;
            return result;
        }
    }

    result.currentHostSample = transportCurrentSample.load(std::memory_order_relaxed);
    result.lastValidPlayingSample = transportLastPlayingSample.load(std::memory_order_relaxed);
    result.analysisStartHostSample = transportAnalysisStartSample.load(std::memory_order_relaxed);
    result.sampleRate = transportSampleRate.load(std::memory_order_relaxed);
    result.isPlaying = transportIsPlaying.load(std::memory_order_relaxed);
    result.hasValidHostPosition = transportPositionValid.load(std::memory_order_relaxed);
    result.revision = transportRevision.load(std::memory_order_acquire);
    return result;
}

void NFVocalHarmonizerAudioProcessor::markAnalysisStartFromCurrentTransport() noexcept
{
    const auto snapshot = getTransportSnapshot();

    int64_t start = snapshot.currentHostSample;
    if (! snapshot.hasValidHostPosition)
        start = snapshot.lastValidPlayingSample;

    transportAnalysisStartSample.store(juce::jmax<int64_t>(0, start),
                                       std::memory_order_release);
}

void NFVocalHarmonizerAudioProcessor::clearAnalysisTransportState() noexcept
{
    transportCurrentSample.store(0, std::memory_order_relaxed);
    transportLastPlayingSample.store(0, std::memory_order_relaxed);
    transportAnalysisStartSample.store(0, std::memory_order_relaxed);
    transportIsPlaying.store(false, std::memory_order_relaxed);
    transportPositionValid.store(false, std::memory_order_relaxed);
    transportRevision.fetch_add(2, std::memory_order_release);
}

double NFVocalHarmonizerAudioProcessor::getHostTimeSeconds() const noexcept
{
    const auto snap = getTransportSnapshot();
    const double sr = juce::jmax(1.0, snap.sampleRate);
    return static_cast<double>(snap.currentHostSample) / sr;
}

bool NFVocalHarmonizerAudioProcessor::isHostPlaying() const noexcept
{
    return transportIsPlaying.load(std::memory_order_relaxed);
}

void NFVocalHarmonizerAudioProcessor::setEditorCursorSecondsForState(double seconds) noexcept
{
    editorCursorSecondsForState.store(juce::jmax(0.0, seconds), std::memory_order_relaxed);
}

double NFVocalHarmonizerAudioProcessor::getEditorCursorSecondsForState() const noexcept
{
    return editorCursorSecondsForState.load(std::memory_order_relaxed);
}

double NFVocalHarmonizerAudioProcessor::takePendingEditorCursorSeconds() noexcept
{
    return pendingEditorCursorSeconds.exchange(-1.0, std::memory_order_acq_rel);
}

void NFVocalHarmonizerAudioProcessor::startAnalysisCapture()
{
    if (analysisState.load(std::memory_order_acquire) == AnalysisState::capturing)
        return;

    // Scratch only — never clear the published note map or frozen viz.
    capture.clearRaw();
    capture.ring().reset();
    capture.setArmed(true);
    engine.resetKeyAnalysis();

    // Anchor timeline only on the first capture; later ANALYZE passes append forward.
    if (! hasPublishedAnalysis())
        markAnalysisStartFromCurrentTransport();

    finishAnalysisRequested.store(false, std::memory_order_release);
    analysisState.store(AnalysisState::capturing, std::memory_order_release);
    wasPlaying = isHostPlaying();
}

void NFVocalHarmonizerAudioProcessor::requestFinishAnalysis() noexcept
{
    if (analysisState.load(std::memory_order_acquire) != AnalysisState::capturing)
        return;
    finishAnalysisRequested.store(true, std::memory_order_release);
}

void NFVocalHarmonizerAudioProcessor::finishAnalysisCapture()
{
    if (analysisState.load(std::memory_order_acquire) != AnalysisState::capturing)
        return;

    capture.setArmed(false);
    capture.drainRing();
    finishAnalysisRequested.store(false, std::memory_order_relaxed);

    const auto settings = readSettings();
    const auto key = activeKeyScale();
    auto notes = capture.buildNotes(settings.intervalChoice, key.first, key.second);
    const bool hasNotes = ! notes.empty();
    const bool hadPublished = hasPublishedAnalysis();

    if (hasNotes)
    {
        const bool appending = hadPublished && ! noteModel.getNotes().empty();
        size_t added = 0;
        if (appending)
            added = noteModel.appendNotes(std::move(notes));
        else
        {
            noteModel.setNotes(std::move(notes));
            added = noteModel.getNotes().size();
        }

        // Merge viz hops forward — never erase earlier capture waveforms.
        const auto& incoming = capture.getRawSamples();
        if (publishedVizSamples.empty())
        {
            publishedVizSamples = incoming;
        }
        else if (! incoming.empty())
        {
            const double protectEnd = publishedVizSamples.back().timeSec;
            for (const auto& s : incoming)
            {
                if (s.timeSec > protectEnd + 0.01)
                    publishedVizSamples.push_back(s);
            }
        }

        if (added > 0 || ! appending)
            publishedAnalysisRevision.fetch_add(1, std::memory_order_release);

        analysisState.store(AnalysisState::ready, std::memory_order_release);
    }
    else
    {
        // Keep previous publishedVizSamples + map.
        analysisState.store(analysisStateAfterFinish(false, hadPublished),
                            std::memory_order_release);
    }
}

void NFVocalHarmonizerAudioProcessor::timerCallback()
{
    wasPlaying = isHostPlaying();

    if (finishAnalysisRequested.load(std::memory_order_acquire)
        && analysisState.load(std::memory_order_acquire) == AnalysisState::capturing)
    {
        finishAnalysisCapture();
    }

    if (analysisState.load(std::memory_order_acquire) == AnalysisState::capturing)
        capture.drainRing();
}

void NFVocalHarmonizerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals guard;

    captureHostTransportForEditor();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    const auto transport = getTransportSnapshot();
    const double sr = juce::jmax(1.0, transport.sampleRate);
    const double timeSec = static_cast<double>(transport.currentHostSample) / sr;
    const bool playing = transport.isPlaying;

    // Play→Stop while capturing: signal finish only (no finalize on audio thread).
    {
        const bool was = wasHostPlayingAudio.exchange(playing, std::memory_order_acq_rel);
        if (was && ! playing
            && analysisState.load(std::memory_order_acquire) == AnalysisState::capturing)
        {
            finishAnalysisRequested.store(true, std::memory_order_release);
        }
    }

    const auto state = analysisState.load(std::memory_order_acquire);
    const bool shouldCapture = (state == AnalysisState::capturing);
    if (shouldCapture)
        analysisCaptureWriteCount.fetch_add(1, std::memory_order_relaxed);

    if (apvts.getRawParameterValue(nf::params::power)->load() < 0.5f)
    {
        processBlockBypassed(buffer, midi);
        return;
    }

    engine.process(buffer, readSettings(), timeSec, &noteModel.getOffsetTable(),
                   shouldCapture ? &capture.ring() : nullptr,
                   shouldCapture);
}

void NFVocalHarmonizerAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto s = readSettings(); s.enabled = false;
    const auto transport = getTransportSnapshot();
    const double sr = juce::jmax(1.0, transport.sampleRate);
    const double timeSec = static_cast<double>(transport.currentHostSample) / sr;
    engine.process(buffer, s, timeSec, &noteModel.getOffsetTable(),
                   nullptr, false);
}

juce::AudioProcessorEditor* NFVocalHarmonizerAudioProcessor::createEditor()
{ return new NFVocalHarmonizerAudioProcessorEditor(*this); }

void NFVocalHarmonizerAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto root = apvts.copyState();
    root.setProperty("schemaVersion", 5, nullptr);
    root.addChild(slotA.createCopy(), -1, nullptr);
    root.addChild(slotB.createCopy(), -1, nullptr);
    root.addChild(noteModel.toValueTree(), -1, nullptr);
    if (hasPublishedAnalysis() && ! noteModel.getNotes().empty())
    {
        root.setProperty("analysisCompleted", true, nullptr);
        root.setProperty("publishedAnalysisRevision",
                         static_cast<juce::int64>(publishedAnalysisRevision.load(std::memory_order_relaxed)),
                         nullptr);
    }
    root.setProperty("editorCursorSeconds", getEditorCursorSecondsForState(), nullptr);
    root.setProperty("analysisStartHostSample",
                     static_cast<juce::int64>(transportAnalysisStartSample.load(std::memory_order_relaxed)),
                     nullptr);
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

    finishAnalysisRequested.store(false, std::memory_order_relaxed);
    capture.setArmed(false);
    if ((static_cast<bool>(root.getProperty("analysisCompleted", false))
         || static_cast<juce::int64>(root.getProperty("publishedAnalysisRevision", 0)) > 0)
        && ! noteModel.getNotes().empty())
    {
        const auto rev = static_cast<uint64_t>(
            static_cast<juce::int64>(root.getProperty("publishedAnalysisRevision", 1)));
        publishedAnalysisRevision.store(juce::jmax<uint64_t>(1, rev), std::memory_order_relaxed);
        analysisState.store(AnalysisState::ready, std::memory_order_release);
    }
    else if (! noteModel.getNotes().empty())
    {
        publishedAnalysisRevision.store(1, std::memory_order_relaxed);
        analysisState.store(AnalysisState::ready, std::memory_order_release);
    }
    else
    {
        publishedAnalysisRevision.store(0, std::memory_order_relaxed);
        analysisState.store(AnalysisState::empty, std::memory_order_release);
    }
    root.removeProperty("analysisCompleted", nullptr);
    root.removeProperty("publishedAnalysisRevision", nullptr);

    const double cursorSeconds = static_cast<double>(root.getProperty("editorCursorSeconds", 0.0));
    editorCursorSecondsForState.store(cursorSeconds, std::memory_order_relaxed);
    pendingEditorCursorSeconds.store(cursorSeconds, std::memory_order_release);
    root.removeProperty("editorCursorSeconds", nullptr);

    if (root.hasProperty("analysisStartHostSample"))
    {
        const auto start = static_cast<int64_t>(
            static_cast<juce::int64>(root.getProperty("analysisStartHostSample")));
        transportAnalysisStartSample.store(juce::jmax<int64_t>(0, start), std::memory_order_relaxed);
        root.removeProperty("analysisStartHostSample", nullptr);
    }

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
