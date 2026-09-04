#include "PluginProcessor.h"
#include "PluginEditor.h"

NFVocalHarmonizerAudioProcessor::NFVocalHarmonizerAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "NF_VOCAL_HARMONIZER_STATE", nf::params::createLayout()), presets(apvts)
{
    slotA.addChild(apvts.copyState(), -1, nullptr);
    slotB.addChild(apvts.copyState(), -1, nullptr);
}

void NFVocalHarmonizerAudioProcessor::prepareToPlay(double sr, int block)
{
    engine.prepare(sr, block, getTotalNumOutputChannels());
    setLatencySamples(engine.getLatencySamples());
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

void NFVocalHarmonizerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals guard;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());
    engine.process(buffer, readSettings());
}

void NFVocalHarmonizerAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto s = readSettings(); s.enabled = false; engine.process(buffer, s);
}

juce::AudioProcessorEditor* NFVocalHarmonizerAudioProcessor::createEditor()
{ return new NFVocalHarmonizerAudioProcessorEditor(*this); }

void NFVocalHarmonizerAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto root = apvts.copyState();
    root.setProperty("schemaVersion", 1, nullptr);
    root.addChild(slotA.createCopy(), -1, nullptr);
    root.addChild(slotB.createCopy(), -1, nullptr);
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
    if (a.isValid()) { slotA = a.createCopy(); root.removeChild(a, nullptr); }
    if (b.isValid()) { slotB = b.createCopy(); root.removeChild(b, nullptr); }
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
