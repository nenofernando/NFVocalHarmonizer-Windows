#include <JuceHeader.h>
#include "dsp/YinPitchDetector.h"
#include "dsp/MusicalScale.h"
#include "dsp/GranularPitchShifter.h"
#include "dsp/HarmonyEngine.h"
#include "Parameters.h"
#include "PresetManager.h"
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
int failures = 0;
void check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    if (! condition) ++failures;
}

class DummyProcessor final : public juce::AudioProcessor
{
public:
    DummyProcessor()
        : AudioProcessor(BusesProperties()
                             .withInput("Input", juce::AudioChannelSet::stereo(), true)
                             .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          apvts(*this, nullptr, "NF_VOCAL_HARMONIZER_STATE", nf::params::createLayout()),
          presets(apvts)
    {
    }

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "Dummy"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& dest) override
    {
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary(*xml, dest);
    }
    void setStateInformation(const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary(data, size))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
};

void fillSine(juce::AudioBuffer<float>& buffer, double sampleRate, float frequency, int startSample = 0)
{
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        const auto sample = 0.5f * std::sin(juce::MathConstants<float>::twoPi
                                            * frequency * static_cast<float>(startSample + n) / static_cast<float>(sampleRate));
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, n, sample);
    }
}

void scaleTests()
{
    using namespace nf::dsp;
    check(std::abs(MusicalScale::targetMidi(60.0f, 4, 0, ScaleType::major) - 64.0f) < 0.01f,
          "C major +3rd: C to E");
    check(std::abs(MusicalScale::targetMidi(62.0f, 4, 0, ScaleType::major) - 65.0f) < 0.01f,
          "C major +3rd: D to F");
    check(std::abs(MusicalScale::targetMidi(60.0f, 7, 0, ScaleType::major) - 72.0f) < 0.01f,
          "C major +8ve");
    check(std::abs(MusicalScale::targetMidi(60.0f, 0, 0, ScaleType::major) - 48.0f) < 0.01f,
          "C major -8ve");
    check(std::abs(MusicalScale::targetMidi(69.17f, 7, 9, ScaleType::naturalMinor) - 81.17f) < 0.02f,
          "Vibrato deviation preserved");
}

void detectorTest()
{
    for (const float frequency : { 90.0f, 150.0f, 220.0f, 440.0f, 700.0f })
    {
        nf::dsp::YinPitchDetector detector;
        detector.prepare(48000.0);
        for (int n = 0; n < 48000; ++n)
            detector.pushSample(0.5f * std::sin(juce::MathConstants<float>::twoPi * frequency * n / 48000.0f));
        const auto result = detector.getEstimate();
        std::cout << "Expected " << frequency << " Hz, detected " << result.frequencyHz
                  << " Hz, confidence " << result.confidence << '\n';
        check(result.voiced, "YIN detects voiced input");
        check(std::abs(result.frequencyHz - frequency) < juce::jmax(1.0f, frequency * 0.012f),
              "YIN frequency accuracy");
        check(result.confidence > 0.7f, "YIN confidence");
    }
}

void keyAnalysisTest()
{
    nf::dsp::KeyAnalyzer analyzer;
    analyzer.reset();
    for (int repeat = 0; repeat < 30; ++repeat)
        for (float midi : { 60.0f, 60.0f, 60.0f, 67.0f, 67.0f, 64.0f, 65.0f, 69.0f, 71.0f, 62.0f })
            analyzer.addMidiNote(midi, 0.95f);
    const auto major = analyzer.analyse();
    std::cout << "Key result root=" << major.root << " mode=" << static_cast<int>(major.type)
              << " confidence=" << major.confidence << '\n';
    check(major.root == 0 && major.type == nf::dsp::ScaleType::major, "Key analyzer identifies C major");
}

void shifterSafetyTest()
{
    nf::dsp::GranularPitchShifter shifter;
    shifter.prepare(48000.0, 512);
    float peak = 0.0f;
    bool finite = true;
    for (int n = 0; n < 96000; ++n)
    {
        const auto in = 0.5f * std::sin(juce::MathConstants<float>::twoPi * 220.0f * n / 48000.0f);
        const auto out = shifter.processSample(in, 1.5f);
        finite = finite && std::isfinite(out);
        peak = juce::jmax(peak, std::abs(out));
    }
    check(finite, "Pitch shifter has no NaN/Inf");
    check(peak > 0.05f && peak < 1.5f, "Pitch shifter bounded output");
    shifter.reset();
    check(shifter.getLatencySamples() > 0, "Pitch shifter reports latency");
}

void latencyTest()
{
    nf::dsp::HarmonyEngine engine;
    engine.prepare(48000.0, 512, 2);
    const int latency = engine.getLatencySamples();
    std::cout << "Reported latency samples: " << latency << '\n';
    check(latency > 0, "HarmonyEngine reports positive latency");

    nf::dsp::HarmonySettings settings;
    settings.enabled = false;
    settings.humanizePercent = 0.0f;
    settings.mixPercent = 0.0f;

    const int total = latency + 256;
    juce::AudioBuffer<float> buffer(2, total);
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);
    engine.process(buffer, settings);

    int peakIndex = 0;
    float peak = 0.0f;
    for (int n = 0; n < total; ++n)
    {
        const auto value = std::abs(buffer.getSample(0, n));
        if (value > peak)
        {
            peak = value;
            peakIndex = n;
        }
    }
    std::cout << "Impulse peak at sample " << peakIndex << " amplitude " << peak << '\n';
    check(std::abs(peakIndex - latency) <= 1, "Dry delay matches reported latency");
    check(peak > 0.5f, "Impulse remains audible after delay");
}

void blockInvarianceTest()
{
    constexpr int total = 4096;
    constexpr double sr = 48000.0;
    nf::dsp::HarmonySettings settings;
    settings.enabled = true;
    settings.autoKey = false;
    settings.key = 0;
    settings.scale = nf::dsp::ScaleType::major;
    settings.humanizePercent = 0.0f;
    settings.widthPercent = 0.0f;
    settings.mixPercent = 50.0f;
    settings.harmonyPercent = 70.0f;
    settings.intervalChoice = 4;

    auto render = [&](int blockSize)
    {
        nf::dsp::HarmonyEngine engine;
        engine.prepare(sr, 1024, 2);
        juce::AudioBuffer<float> out(2, total);
        out.clear();
        int rendered = 0;
        while (rendered < total)
        {
            const int n = juce::jmin(blockSize, total - rendered);
            juce::AudioBuffer<float> block(2, n);
            for (int i = 0; i < n; ++i)
            {
                const auto sample = 0.4f * std::sin(juce::MathConstants<float>::twoPi
                                                    * 220.0f * static_cast<float>(rendered + i) / static_cast<float>(sr));
                block.setSample(0, i, sample);
                block.setSample(1, i, sample);
            }
            engine.process(block, settings);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom(ch, rendered, block, ch, 0, n);
            rendered += n;
        }
        return out;
    };

    const auto large = render(512);
    const auto tiny = render(32);
    double error = 0.0;
    bool finite = true;
    for (int n = 256; n < total; ++n)
    {
        const auto a = large.getSample(0, n);
        const auto b = tiny.getSample(0, n);
        finite = finite && std::isfinite(a) && std::isfinite(b);
        error += static_cast<double>(std::abs(a - b));
    }
    error /= static_cast<double>(total - 256);
    std::cout << "Block invariance mean abs error: " << error << '\n';
    check(finite, "Block renders stay finite");
    check(error < 1.0e-5, "Output invariant across 512 vs 32 sample blocks");
}

void silenceDoesNotInventNotesTest()
{
    nf::dsp::HarmonyEngine engine;
    engine.prepare(48000.0, 256, 2);
    nf::dsp::HarmonySettings settings;
    settings.enabled = true;
    settings.mixPercent = 100.0f;
    settings.harmonyPercent = 100.0f;
    settings.humanizePercent = 0.0f;

    juce::AudioBuffer<float> buffer(2, 48000);
    buffer.clear();
    engine.process(buffer, settings);
    const auto meters = engine.getMeters();
    const auto pitch = engine.getPitchEstimate();
    check(! pitch.voiced, "Silence is not classified as voiced");
    check(meters.outputPeakL < 1.0e-4f && meters.outputPeakR < 1.0e-4f, "Silence does not invent harmonic notes");
}

void parameterIdTest()
{
    DummyProcessor processor;
    const char* ids[] { nf::params::interval, nf::params::harmony, nf::params::formant, nf::params::humanize,
                        nf::params::width, nf::params::mix, nf::params::key, nf::params::scale,
                        nf::params::autoKey, nf::params::enabled };
    bool allPresent = true;
    for (auto* id : ids)
        allPresent = allPresent && processor.apvts.getParameter(id) != nullptr;
    check(allPresent, "All ten stable parameter IDs exist");
    check(processor.getParameters().size() >= 10, "Host sees at least ten automatable parameters");
    check(processor.apvts.getParameter(nf::params::power) != nullptr, "Power is a separate parameter from Harmonize");
}

void recallAndAutomationTest()
{
    DummyProcessor a;
    a.apvts.getParameter(nf::params::harmony)->setValueNotifyingHost(
        a.apvts.getParameter(nf::params::harmony)->convertTo0to1(23.0f));
    a.apvts.getParameter(nf::params::mix)->setValueNotifyingHost(
        a.apvts.getParameter(nf::params::mix)->convertTo0to1(81.0f));
    a.apvts.getParameter(nf::params::enabled)->setValueNotifyingHost(1.0f);
    a.apvts.getParameter(nf::params::interval)->setValueNotifyingHost(
        a.apvts.getParameter(nf::params::interval)->convertTo0to1(7.0f));

    juce::MemoryBlock blob;
    a.getStateInformation(blob);
    check(blob.getSize() > 0, "State blob is non-empty");

    DummyProcessor b;
    b.setStateInformation(blob.getData(), static_cast<int>(blob.getSize()));
    const auto harmonyA = a.apvts.getRawParameterValue(nf::params::harmony)->load();
    const auto harmonyB = b.apvts.getRawParameterValue(nf::params::harmony)->load();
    const auto mixB = b.apvts.getRawParameterValue(nf::params::mix)->load();
    const auto enabledB = b.apvts.getRawParameterValue(nf::params::enabled)->load();
    const auto intervalB = b.apvts.getRawParameterValue(nf::params::interval)->load();
    check(std::abs(harmonyA - harmonyB) < 0.01f, "Recall restores harmony");
    check(std::abs(mixB - 81.0f) < 0.01f, "Recall restores mix");
    check(enabledB > 0.5f, "Recall restores Harmonize");
    check(std::abs(intervalB - 7.0f) < 0.01f, "Recall restores interval");

    auto* mix = a.apvts.getParameter(nf::params::mix);
    mix->beginChangeGesture();
    mix->setValueNotifyingHost(mix->convertTo0to1(10.0f));
    mix->endChangeGesture();
    check(std::abs(a.apvts.getRawParameterValue(nf::params::mix)->load() - 10.0f) < 0.01f,
          "Automation gesture writes mix");
}

void presetRoundTripTest()
{
    DummyProcessor processor;
    processor.apvts.getParameter(nf::params::formant)->setValueNotifyingHost(
        processor.apvts.getParameter(nf::params::formant)->convertTo0to1(-4.5f));
    processor.apvts.getParameter(nf::params::width)->setValueNotifyingHost(
        processor.apvts.getParameter(nf::params::width)->convertTo0to1(42.0f));

    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("nf_vocal_harmonizer_test.nfhpreset");
    file.deleteFile();
    juce::String error;
    check(processor.presets.savePresetToFile(file, error), "Preset save succeeds");
    processor.apvts.getParameter(nf::params::formant)->setValueNotifyingHost(
        processor.apvts.getParameter(nf::params::formant)->convertTo0to1(0.0f));
    processor.apvts.getParameter(nf::params::width)->setValueNotifyingHost(
        processor.apvts.getParameter(nf::params::width)->convertTo0to1(100.0f));
    check(processor.presets.loadPresetFromFile(file, error), "Preset load succeeds");
    check(std::abs(processor.apvts.getRawParameterValue(nf::params::formant)->load() + 4.5f) < 0.05f,
          "Preset restores formant");
    check(std::abs(processor.apvts.getRawParameterValue(nf::params::width)->load() - 42.0f) < 0.05f,
          "Preset restores width");
    file.deleteFile();
}

void engineAutomationAudibleTest()
{
    nf::dsp::HarmonyEngine dryEngine;
    nf::dsp::HarmonyEngine wetEngine;
    dryEngine.prepare(48000.0, 256, 2);
    wetEngine.prepare(48000.0, 256, 2);

    nf::dsp::HarmonySettings dry;
    dry.enabled = false;
    dry.humanizePercent = 0.0f;
    nf::dsp::HarmonySettings wet = dry;
    wet.enabled = true;
    wet.mixPercent = 100.0f;
    wet.harmonyPercent = 100.0f;
    wet.autoKey = false;
    wet.key = 0;
    wet.scale = nf::dsp::ScaleType::major;

    juce::AudioBuffer<float> dryBuffer(2, 24000);
    juce::AudioBuffer<float> wetBuffer(2, 24000);
    fillSine(dryBuffer, 48000.0, 220.0f);
    fillSine(wetBuffer, 48000.0, 220.0f);
    dryEngine.process(dryBuffer, dry);
    wetEngine.process(wetBuffer, wet);

    double difference = 0.0;
    bool finite = true;
    for (int n = 8000; n < 24000; ++n)
    {
        finite = finite && std::isfinite(wetBuffer.getSample(0, n));
        difference += static_cast<double>(std::abs(wetBuffer.getSample(0, n) - dryBuffer.getSample(0, n)));
    }
    difference /= 16000.0;
    std::cout << "Enabled vs bypassed mean difference: " << difference << '\n';
    check(finite, "Enabled render is finite");
    check(difference > 0.01, "Harmonize automation changes the audio");
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    scaleTests();
    detectorTest();
    keyAnalysisTest();
    shifterSafetyTest();
    latencyTest();
    blockInvarianceTest();
    silenceDoesNotInventNotesTest();
    parameterIdTest();
    recallAndAutomationTest();
    presetRoundTripTest();
    engineAutomationAudibleTest();
    std::cout << "Failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
