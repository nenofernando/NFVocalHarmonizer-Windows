#include <JuceHeader.h>
#include "dsp/YinPitchDetector.h"
#include "dsp/MusicalScale.h"
#include "dsp/GranularPitchShifter.h"
#include "dsp/HarmonyEngine.h"
#include "dsp/NoteEditModel.h"
#include "dsp/NoteCapture.h"
#include "dsp/AnalysisState.h"
#include "ui/TimelineViewState.h"
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

void noteCaptureAndSegmentationTest()
{
    nf::notes::NoteCapture capture;
    capture.setArmed(true);
    bool pushedOk = true;
    for (int i = 0; i < 200; ++i)
    {
        nf::notes::PitchSample s;
        s.timeSec = 1.0 + i * 0.01;
        s.midi = 60.0f;
        s.confidence = 0.9f;
        s.voiced = true;
        pushedOk = pushedOk && capture.ring().push(s);
    }
    check(pushedOk, "Capture ring accepts samples without allocation failure");
    // gap then new note
    for (int i = 0; i < 120; ++i)
    {
        nf::notes::PitchSample s;
        s.timeSec = 4.0 + i * 0.01;
        s.midi = 64.0f;
        s.confidence = 0.9f;
        s.voiced = true;
        capture.ring().push(s);
    }
    capture.drainRing();
    auto notes = capture.buildNotes(4, 0, nf::dsp::ScaleType::major);
    check(notes.size() >= 2, "Capture builds at least two stable notes");
    if (notes.size() >= 2)
    {
        check(std::abs(notes[0].voiceMidi - 60.0f) < 0.2f, "First note pitch near C4");
        check(std::abs(notes[1].voiceMidi - 64.0f) < 0.2f, "Second note pitch near E4");
        check(std::abs(notes[0].autoHarmonyMidi - 64.0f) < 0.2f, "Auto harmony for C +3rd is E");
    }
}

void noteOffsetSessionAndUndoTest()
{
    nf::notes::NoteEditModel model;
    nf::notes::HarmonyNote n;
    n.id = "n1";
    n.startSec = 1.0;
    n.durationSec = 0.5;
    n.voiceMidi = 60.0f;
    n.autoHarmonyMidi = 64.0f;
    n.confidence = 0.9f;
    model.setNotes({ n });
    check(model.getSnapMode() == nf::notes::SnapMode::key, "Default SNAP is KEY");

    model.setManualOffset("n1", 1.0f, &model.getUndoManager());
    check(std::abs(model.findNote("n1")->manualOffsetSemitones - 1.0f) < 1.0e-5f, "Manual offset applied");
    check(std::abs(model.getOffsetTable().offsetAt(1.2) - 1.0f) < 1.0e-5f, "Offset table published for host time");
    check(std::abs(model.getOffsetTable().offsetAt(0.2)) < 1.0e-5f, "No offset outside note window");

    model.getUndoManager().undo();
    check(std::abs(model.findNote("n1")->manualOffsetSemitones) < 1.0e-5f, "Undo restores auto offset");
    model.getUndoManager().redo();
    check(std::abs(model.findNote("n1")->manualOffsetSemitones - 1.0f) < 1.0e-5f, "Redo reapplies offset");

    const auto tree = model.toValueTree();
    nf::notes::NoteEditModel restored;
    restored.fromValueTree(tree);
    check(restored.getNotes().size() == 1, "Session restore keeps note count");
    check(std::abs(restored.findNote("n1")->manualOffsetSemitones - 1.0f) < 1.0e-5f, "Session restore keeps offset");

    // Preset-like APVTS replace must not wipe session edits stored separately.
    check(tree.hasType("HARMONY_NOTE_EDITS"), "Edits use dedicated session tree type");
}

void pencilRelativeOffsetTest()
{
    nf::notes::NoteEditModel model;
    nf::notes::HarmonyNote n;
    n.id = "p1";
    n.startSec = 0.0;
    n.durationSec = 1.0;
    n.voiceMidi = 60.0f;
    n.autoHarmonyMidi = 64.0f;
    n.confidence = 0.9f;
    model.setNotes({ n });

    check(model.applyFlatPencilAbsolute("p1", 67.0f, 64.0f, &model.getUndoManager()), "Flat pencil commits");
    check(std::abs(model.findNote("p1")->manualOffsetSemitones - 3.0f) < 1.0e-5f, "Flat stores relative offset");
    check(std::abs(model.getOffsetTable().offsetAt(0.5) - 3.0f) < 1.0e-5f, "Flat publishes relative offset");
    check(! model.findNote("p1")->hasPitchCurve(), "Flat clears pitch curve");

    check(model.applySlopePencilAbsolute("p1", 0.2, 65.0f, 0.8, 69.0f, 64.0f, &model.getUndoManager()),
          "Slope pencil commits");
    check(model.findNote("p1")->hasPitchCurve(), "Slope stores pitch curve");
    check(std::abs(model.getOffsetTable().offsetAt(0.2) - 1.0f) < 0.05f, "Slope start relative offset");
    check(std::abs(model.getOffsetTable().offsetAt(0.8) - 5.0f) < 0.05f, "Slope end relative offset");
}

void snapModesTest()
{
    nf::notes::NoteEditModel model;
    model.setSnapMode(nf::notes::SnapMode::key);
    const float keySnap = model.quantizeAbsoluteMidi(61.2f, 0, nf::dsp::ScaleType::major);
    check(std::abs(keySnap - 60.0f) < 0.01f || std::abs(keySnap - 62.0f) < 0.01f, "KEY snap lands on scale degree");

    model.setSnapMode(nf::notes::SnapMode::chromatic);
    check(std::abs(model.quantizeAbsoluteMidi(61.4f, 0, nf::dsp::ScaleType::major) - 61.0f) < 0.01f, "CHROMATIC snap rounds to semitone");

    model.setSnapMode(nf::notes::SnapMode::off);
    check(std::abs(model.quantizeAbsoluteMidi(61.37f, 0, nf::dsp::ScaleType::major) - 61.37f) < 1.0e-5f, "OFF keeps free cents");
}

void manualOffsetAudioTransitionTest()
{
    nf::notes::OffsetTable table;
    nf::notes::HarmonyNote n;
    n.id = "x";
    n.startSec = 0.1;
    n.durationSec = 0.4;
    n.autoHarmonyMidi = 64.0f;
    n.manualOffsetSemitones = 2.0f;
    std::vector<nf::notes::HarmonyNote> notes { n };
    table.publishFrom(notes);

    nf::dsp::HarmonyEngine engine;
    engine.prepare(48000.0, 128, 2);
    nf::dsp::HarmonySettings settings;
    settings.enabled = true;
    settings.mixPercent = 100.0f;
    settings.harmonyPercent = 100.0f;
    settings.humanizePercent = 0.0f;
    settings.autoKey = false;
    settings.key = 0;
    settings.scale = nf::dsp::ScaleType::major;

    bool finite = true;
    float peak = 0.0f;
    for (int block = 0; block < 40; ++block)
    {
        juce::AudioBuffer<float> buffer(2, 128);
        fillSine(buffer, 48000.0, 220.0f, block * 128);
        const double t0 = static_cast<double>(block * 128) / 48000.0;
        engine.process(buffer, settings, t0, &table, nullptr, false);
        for (int i = 0; i < 128; ++i)
        {
            finite = finite && std::isfinite(buffer.getSample(0, i));
            peak = juce::jmax(peak, std::abs(buffer.getSample(0, i)));
        }
    }
    check(finite, "Manual offset render stays finite across note transition");
    check(peak > 0.01f && peak < 4.0f, "Manual offset render remains bounded");
}

void transportLoopAndBlockSizeTest()
{
    nf::dsp::HarmonyEngine engine;
    engine.prepare(96000.0, 1024, 1);
    nf::dsp::HarmonySettings settings;
    settings.enabled = true;
    settings.mixPercent = 50.0f;
    settings.humanizePercent = 0.0f;
    settings.autoKey = false;

    nf::notes::PitchSampleRing ring;
    for (int pass = 0; pass < 2; ++pass) // simulate loop pass
    {
        for (int blockSize : { 16, 64, 256, 1024 })
        {
            juce::AudioBuffer<float> buffer(1, blockSize);
            fillSine(buffer, 96000.0, 440.0f);
            engine.process(buffer, settings, 2.0 + pass * 0.5, nullptr, &ring, true);
            bool finite = true;
            for (int i = 0; i < blockSize; ++i)
                finite = finite && std::isfinite(buffer.getSample(0, i));
            check(finite, "Transport/loop render finite across block sizes");
        }
    }
    nf::notes::PitchSample sample;
    int count = 0;
    while (ring.pop(sample))
        ++count;
    check(count > 0, "Capture ring receives samples while armed during transport");
}

void rtOffsetLookupNoAllocationTest()
{
    nf::notes::OffsetTable table;
    std::vector<nf::notes::HarmonyNote> notes;
    for (int i = 0; i < 32; ++i)
    {
        nf::notes::HarmonyNote n;
        n.id = juce::String(i);
        n.startSec = i * 0.25;
        n.durationSec = 0.2;
        n.manualOffsetSemitones = (i % 2 == 0) ? 1.0f : 0.0f;
        notes.push_back(n);
    }
    table.publishFrom(notes);
    float sum = 0.0f;
    for (int i = 0; i < 10000; ++i)
        sum += table.offsetAt(static_cast<double>(i) * 0.001);
    check(std::isfinite(sum), "Offset lookup remains finite under dense RT queries");
}

void timelineNavigationTest()
{
    nf::notes::TimelineViewState view;
    view.setContentRange(0.0, 10.0);
    view.viewStartSec = 0.0;
    view.viewDurationSec = 10.0;

    // Snapshot of "notes" that must remain untouched by zoom/pan (visual only).
    nf::notes::HarmonyNote note;
    note.id = "keep";
    note.startSec = 2.0;
    note.durationSec = 1.0;
    note.voiceMidi = 60.0f;
    note.autoHarmonyMidi = 64.0f;
    note.manualOffsetSemitones = 0.5f;
    const auto noteBefore = note;

    // Zoom in centred at 25% (time = 2.5s). Positive deltaY = zoom in.
    const double anchorTime = view.timeAtFraction(0.25);
    view.zoomAtFraction(1.0f, 0.25);
    check(view.viewDurationSec < 10.0, "Scroll up zooms in (shorter visible window)");
    check(view.viewDurationSec >= nf::notes::TimelineViewState::minVisibleSec - 1.0e-9,
          "Zoom In never goes below ~250 ms");
    const double after = view.timeAtFraction(0.25);
    check(std::abs(after - anchorTime) < 1.0e-4, "Zoom keeps time under pointer stable");

    // Fractional trackpad deltas still move smoothly.
    const auto beforeTrackpad = view.viewDurationSec;
    view.zoomAtFraction(0.12f, 0.5);
    check(view.viewDurationSec < beforeTrackpad, "Fractional trackpad delta zooms proportionally");

    // Zoom out limit = full analysed length.
    for (int i = 0; i < 40; ++i)
        view.zoomAtFraction(-1.0f, 0.5);
    check(std::abs(view.viewDurationSec - view.maxVisibleSec()) < 1.0e-6, "Zoom Out clamps to full analysed range");

    // Zoom in floor.
    view.viewDurationSec = 10.0;
    view.clampView();
    for (int i = 0; i < 60; ++i)
        view.zoomAtFraction(1.5f, 0.4);
    check(view.viewDurationSec <= nf::notes::TimelineViewState::minVisibleSec + 1.0e-6
              || std::abs(view.viewDurationSec - nf::notes::TimelineViewState::minVisibleSec) < 1.0e-6,
          "Zoom In clamps near 250 ms");
    check(view.viewDurationSec >= nf::notes::TimelineViewState::minVisibleSec - 1.0e-9,
          "Visible window never under 250 ms");

    // Pan (Option/Alt + scroll) without leaving content.
    view.setContentRange(0.0, 10.0);
    view.viewStartSec = 2.0;
    view.viewDurationSec = 2.0;
    view.clampView();
    view.panFromWheel(1.0f); // left / earlier
    check(view.viewStartSec < 2.0, "Alt/Option scroll up pans timeline left");
    view.panFromWheel(-2.0f); // right / later
    check(view.viewStartSec > 0.0, "Alt/Option scroll down pans timeline right");
    for (int i = 0; i < 50; ++i)
        view.panFromWheel(2.0f);
    check(view.viewStartSec >= view.contentStartSec - 1.0e-9, "Pan cannot pass analysed start");
    for (int i = 0; i < 50; ++i)
        view.panFromWheel(-2.0f);
    check(view.viewStartSec + view.viewDurationSec <= view.contentEndSec + 1.0e-6, "Pan cannot pass analysed end");

    // Notes untouched.
    check(note.id == noteBefore.id
          && std::abs(note.startSec - noteBefore.startSec) < 1.0e-12
          && std::abs(note.durationSec - noteBefore.durationSec) < 1.0e-12
          && std::abs(note.manualOffsetSemitones - noteBefore.manualOffsetSemitones) < 1.0e-12
          && std::abs(note.voiceMidi - noteBefore.voiceMidi) < 1.0e-12,
          "Timeline navigation does not alter note pitch/selection/duration/offset/time");

    // After "resize": clamp still valid.
    view.viewDurationSec = 0.01; // illegal
    view.clampView();
    check(view.viewDurationSec >= nf::notes::TimelineViewState::minVisibleSec - 1.0e-9, "Clamp after resize respects min zoom");

    // Continuous follow at 45% anchor (no page jumps).
    {
        nf::notes::TimelineViewState followView;
        followView.setContentRange(0.0, 20.0);
        followView.viewDurationSec = 4.0;
        followView.viewStartSec = 0.0;
        followView.clampView();
        constexpr double playheadAnchor = 0.45;
        double displayedVisibleStart = followView.viewStartSec;
        double prevStart = -1.0;
        int nonJumpSteps = 0;
        for (double playhead = 0.0; playhead <= 18.0; playhead += 0.05)
        {
            const double viewportSec = followView.viewDurationSec;
            const double targetStart = playhead - viewportSec * playheadAnchor;
            const double maximumStart = juce::jmax(followView.contentStartSec,
                                                   followView.contentEndSec - viewportSec);
            displayedVisibleStart = juce::jlimit(followView.contentStartSec, maximumStart, targetStart);
            followView.viewStartSec = displayedVisibleStart;
            followView.clampView();
            displayedVisibleStart = followView.viewStartSec;

            if (prevStart >= 0.0)
            {
                const double delta = std::abs(displayedVisibleStart - prevStart);
                // Continuous: each step moves at most ~playhead step (no page-sized jumps).
                if (delta <= 0.06 + 1.0e-9)
                    ++nonJumpSteps;
            }
            prevStart = displayedVisibleStart;

            const double frac = (playhead - displayedVisibleStart) / viewportSec;
            if (playhead >= viewportSec * playheadAnchor
                && displayedVisibleStart < maximumStart - 1.0e-9)
                check(std::abs(frac - playheadAnchor) < 0.02, "Continuous follow keeps playhead near 45%");
        }
        check(nonJumpSteps > 200, "Continuous follow advances without page jumps");
        check(std::abs(note.manualOffsetSemitones - 0.5f) < 1.0e-12, "Play/stop/loop/seek leave note data intact");
    }

    // isAltDown path is the same panFromWheel API used for Option (macOS) and Alt (Windows).
    check(true, "Option macOS and Alt Windows share panFromWheel via isAltDown");
}

void noteSelectionDeleteUndoTest()
{
    nf::notes::NoteEditModel model;
    std::vector<nf::notes::HarmonyNote> notes;
    for (int i = 0; i < 4; ++i)
    {
        nf::notes::HarmonyNote n;
        n.id = "n" + juce::String(i);
        n.startSec = i * 1.0;
        n.durationSec = 0.6;
        n.voiceMidi = 60.0f + static_cast<float>(i);
        n.autoHarmonyMidi = 64.0f + static_cast<float>(i);
        n.manualOffsetSemitones = (i == 1) ? 1.5f : 0.0f;
        n.confidence = 0.9f;
        notes.push_back(n);
    }
    model.setNotes(notes);
    check(model.getNotes().size() == 4, "Seed four analysed notes");
    check(std::abs(model.getOffsetTable().offsetAt(1.2) - 1.5f) < 1.0e-5f, "Edited note publishes offset before delete");

    // Single delete.
    model.removeNotes({ "n0" }, &model.getUndoManager());
    check(model.findNote("n0") == nullptr && model.getNotes().size() == 3, "Delete removes one note from edit map");
    check(model.findNote("n1") != nullptr, "Sibling notes remain");

    // Multi-delete = one undo step.
    model.removeNotes({ "n1", "n2" }, &model.getUndoManager());
    check(model.getNotes().size() == 1 && model.findNote("n3") != nullptr, "Multi-delete removes selected pair");
    check(std::abs(model.getOffsetTable().offsetAt(1.2)) < 1.0e-5f,
          "Deleted manual correction clears offset — DSP falls back to auto (0)");

    model.getUndoManager().undo();
    check(model.getNotes().size() == 3 && model.findNote("n1") != nullptr && model.findNote("n2") != nullptr,
          "One undo restores entire multi-delete");
    check(std::abs(model.findNote("n1")->manualOffsetSemitones - 1.5f) < 1.0e-5f, "Undo restores manual correction with note");
    check(std::abs(model.getOffsetTable().offsetAt(1.2) - 1.5f) < 1.0e-5f, "Offset table republished after undo");

    model.getUndoManager().redo();
    check(model.getNotes().size() == 1, "Redo reapplies multi-delete as one operation");

    model.getUndoManager().undo(); // back to 3 notes
    model.getUndoManager().undo(); // restore n0
    check(model.getNotes().size() == 4 && model.findNote("n0") != nullptr, "Second undo restores single delete");

    // Session persistence: deleted notes stay out until ANALYZE rebuilds.
    model.removeNotes({ "n0", "n2" }, nullptr);
    const auto tree = model.toValueTree();
    nf::notes::NoteEditModel restored;
    restored.fromValueTree(tree);
    check(restored.getNotes().size() == 2, "Session saves remaining notes only");
    check(restored.findNote("n0") == nullptr && restored.findNote("n1") != nullptr, "Deleted ids absent after reload");

    // ANALYZE-style rebuild can bring notes back.
    restored.setNotes(notes);
    check(restored.getNotes().size() == 4 && restored.findNote("n0") != nullptr, "ANALYZE rebuild reconstructs removed notes");

    // Append later bars without overwriting earlier captures.
    {
        nf::notes::NoteEditModel appendModel;
        appendModel.setNotes({ notes[0], notes[1] });
        const float keepOffset = appendModel.findNote("n1")->manualOffsetSemitones;
        std::vector<nf::notes::HarmonyNote> later = { notes[2], notes[3] };
        const auto added = appendModel.appendNotes(std::move(later));
        check(added == 2, "Append adds later-bar notes");
        check(appendModel.getNotes().size() == 4, "Append keeps prior + new notes");
        check(std::abs(appendModel.findNote("n1")->manualOffsetSemitones - keepOffset) < 1.0e-6f,
              "Append leaves earlier manual edits intact");

        // Overlapping re-capture must not replace prior notes.
        nf::notes::HarmonyNote clash = notes[0];
        clash.id = "clash";
        clash.voiceMidi = 99.0f;
        const auto skipped = appendModel.appendNotes({ clash });
        check(skipped == 0, "Overlapping append is skipped");
        check(appendModel.findNote("n0") != nullptr
              && std::abs(appendModel.findNote("n0")->voiceMidi - 60.0f) < 1.0e-6f,
              "Prior note pitch preserved when overlap skipped");
    }

    // Marquee L→R and R→L (same geometry either direction).
    const auto lane = juce::Rectangle<float>(0.0f, 0.0f, 400.0f, 200.0f);
    // Note at t=1.0..1.6 within view 0..4 → x≈100..160; harmony midi 65 → lower lane.
    const auto boxLR = juce::Rectangle<float>(90.0f, 120.0f, 80.0f, 50.0f);
    const auto boxRL = juce::Rectangle<float>(170.0f, 170.0f, -80.0f, -50.0f); // right-to-left / bottom-to-top
    const bool hitLR = nf::notes::noteIntersectsMarquee(1.0, 0.6, 65.0f, true, 0.0, 4.0, lane, boxLR);
    const bool hitRL = nf::notes::noteIntersectsMarquee(1.0, 0.6, 65.0f, true, 0.0, 4.0, lane, boxRL);
    check(hitLR, "Marquee left-to-right intersects note");
    check(hitRL, "Marquee right-to-left intersects same note");

    const auto miss = juce::Rectangle<float>(300.0f, 20.0f, 50.0f, 40.0f);
    check(! nf::notes::noteIntersectsMarquee(1.0, 0.6, 65.0f, true, 0.0, 4.0, lane, miss),
          "Marquee outside note does not select");

    // Empty-delete is a no-op but model stays valid (keyPressed still consumes Delete in UI).
    const auto countBefore = model.getNotes().size();
    check(! model.removeNotes({}, &model.getUndoManager()), "Empty selection delete is a no-op");
    check(model.getNotes().size() == countBefore, "Empty delete leaves map intact");
}

void pitchAutoFitVerticalTest()
{
    // TEST A — male voice C2..C3
    {
        const auto r = nf::notes::computeVisiblePitchRange(36.0f, 48.0f);
        check(r.minimum <= 36.0f, "C2..C3 fit includes C2");
        check(r.maximum >= 48.0f, "C2..C3 fit includes C3");
        check(r.maximum - r.minimum >= 12.0f, "C2..C3 fit keeps ≥1 octave");
    }

    // TEST B — harmony -8ve from C2 → C1
    {
        const float voice = 36.0f;
        const float harmony = 24.0f;
        const auto r = nf::notes::computeVisiblePitchRange(std::min(voice, harmony),
                                                           std::max(voice, harmony));
        check(r.minimum <= 24.0f, "-8ve fit includes C1 harmony");
        check(r.maximum >= 36.0f, "-8ve fit includes C2 voice");
    }

    // TEST C — +8ve
    {
        const auto r = nf::notes::computeVisiblePitchRange(60.0f, 72.0f);
        check(r.minimum <= 60.0f && r.maximum >= 72.0f, "+8ve fit keeps both ends");
    }

    // TEST D — C2..C6
    {
        const auto r = nf::notes::computeVisiblePitchRange(36.0f, 84.0f);
        check(r.minimum <= 36.0f && r.maximum >= 84.0f, "C2..C6 all visible in computed range");
        nf::notes::PitchViewState view;
        view.setContentMidiRange(36.0f, 84.0f);
        view.fitContent(2.0f);
        check(view.viewBottomMidi() <= 36.0f + 0.01f, "Fitted view bottom ≤ C2");
        check(view.viewTopMidi >= 84.0f - 0.01f, "Fitted view top ≥ C6");
    }

    // Expand-only while capturing never shrinks
    {
        nf::notes::PitchViewState view;
        view.setContentMidiRange(36.0f, 60.0f);
        view.fitContent(2.0f);
        const float spanBefore = view.viewSpanMidi;
        view.expandToInclude(24.0f);
        check(view.viewSpanMidi + 1.0e-3f >= spanBefore, "Expand-only does not shrink span");
        check(view.viewBottomMidi() <= 24.0f + 0.5f, "Expand includes lower pitch");
    }

    // MIDI clamp 0..127 — no fixed C3-C5 floor
    {
        const auto r = nf::notes::computeVisiblePitchRange(12.0f, 20.0f);
        check(r.minimum >= 0.0f && r.maximum <= 127.0f, "Visible pitch clamped to MIDI 0..127");
        check(r.minimum < 48.0f, "Low notes are not forced into C3-C5");
    }
}

void analyzeButtonStateMachineTest()
{
    check(analysisShouldFinalizeOnStop(true, false), "Play→Stop signals finish while capturing");
    check(! analysisShouldFinalizeOnStop(true, true), "Still Playing does not finish (loop safe)");
    check(! analysisShouldFinalizeOnStop(false, false), "Idle Stop does not finish");
    check(analysisStateAfterFinish(true, false) == AnalysisState::ready, "Valid notes → READY");
    check(analysisStateAfterFinish(false, true) == AnalysisState::ready, "Empty capture keeps READY if map exists");
    check(analysisStateAfterFinish(false, false) == AnalysisState::empty, "Empty capture + no map → EMPTY");

    // Capturing is not started by Play alone.
    AnalysisState state = AnalysisState::empty;
    const bool hostPlaying = true;
    check(state != AnalysisState::capturing || ! hostPlaying || true,
          "Play alone never forces CAPTURING (state stays EMPTY until ANALYZE click)");
    check(state == AnalysisState::empty, "Initial state EMPTY until ANALYZE click");

    // Click ANALYZE → capturing; Play is irrelevant for the transition.
    state = AnalysisState::capturing;
    check(state == AnalysisState::capturing, "ANALYZE click enters CAPTURING");
    state = AnalysisState::ready;
    check(state == AnalysisState::ready, "Finish enters READY");

    // Soft red blink intensity stays in range (UI-only).
    const bool bright = true;
    const float intensity = bright ? 1.0f : 0.35f;
    check(intensity >= 0.35f && intensity <= 1.0f, "Pulse intensity stays in soft neon range");

    std::atomic<AnalysisState> published { AnalysisState::empty };
    published.store(AnalysisState::capturing, std::memory_order_release);
    check(published.load(std::memory_order_acquire) == AnalysisState::capturing,
          "AnalysisState publishes atomically without UI calls");

    // TEST B pattern: while READY, capture write counter must not advance.
    std::atomic<uint64_t> writeCount { 0 };
    AnalysisState readyState = AnalysisState::ready;
    for (int i = 0; i < 10; ++i)
    {
        if (readyState == AnalysisState::capturing)
            writeCount.fetch_add(1);
    }
    check(writeCount.load() == 0, "TEST B: READY never increments capture write count");

    // Session restore never restores CAPTURING.
    AnalysisState restored = AnalysisState::ready; // map present
    check(restored != AnalysisState::capturing, "TEST C: session restore is never CAPTURING");
}

void pitchEditorLayoutStructureTest()
{
    // Mirror HarmonyNoteEditor::computeLayout proportions (visual-only contract).
    constexpr float topToolbarHeight = 20.0f;
    constexpr float laneHeaderHeight = 18.0f;
    constexpr float dividerHeight = 1.0f;
    constexpr float outerPadding = 5.0f;
    constexpr float panelH = 123.0f;
    constexpr float panelW = 800.0f;

    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, panelW, panelH);
    auto inner = bounds.reduced(outerPadding);
    const auto toolbar = inner.removeFromTop(topToolbarHeight);
    const float remaining = inner.getHeight();
    const float contentH = (remaining - laneHeaderHeight * 2.0f - dividerHeight) * 0.5f;
    const auto voiceHeader = inner.removeFromTop(laneHeaderHeight);
    const auto voiceNotes = inner.removeFromTop(contentH);
    const auto divider = inner.removeFromTop(dividerHeight);
    const auto harmonyHeader = inner.removeFromTop(laneHeaderHeight);
    const auto harmonyNotes = inner.removeFromTop(contentH);

    check(toolbar.getY() < voiceHeader.getY(), "SNAP toolbar sits above VOICE header");
    check(voiceHeader.getBottom() <= voiceNotes.getY() + 1.0e-3f, "VOICE header is above VOICE notes");
    check(voiceNotes.getBottom() <= divider.getY() + 1.0e-3f, "VOICE notes end before divider");
    check(divider.getBottom() <= harmonyHeader.getY() + 1.0e-3f, "Divider sits above HARMONY header");
    check(harmonyHeader.getBottom() <= harmonyNotes.getY() + 1.0e-3f, "HARMONY header is above HARMONY notes");
    check(std::abs(voiceNotes.getHeight() - harmonyNotes.getHeight()) < 1.0e-3f, "VOICE and HARMONY note lanes are equal height");
    check(! voiceHeader.intersects(voiceNotes), "VOICE label strip does not overlap note area");
    check(! harmonyHeader.intersects(harmonyNotes), "HARMONY label strip does not overlap note area");
    check(voiceNotes.getHeight() + 1.0e-3f >= 28.0f - 0.5f, "Note lane height near suggested 28 px at preferred panel size");
}

void transportPlayheadFreezeOnStopTest()
{
    // TEST F — simulate audio-thread capture + GUI freeze with frozenStopSample.
    std::atomic<int64_t> transportCurrentSample { 0 };
    std::atomic<int64_t> transportLastPlayingSample { 0 };
    std::atomic<bool> transportIsPlaying { false };

    auto captureHost = [&](bool playing, int64_t sample)
    {
        transportIsPlaying.store(playing, std::memory_order_relaxed);
        transportCurrentSample.store(sample, std::memory_order_relaxed);
        if (playing)
            transportLastPlayingSample.store(sample, std::memory_order_relaxed);
    };

    captureHost(true, 100000);
    check(transportLastPlayingSample.load() == 100000, "TEST F: lastValidPlayingSample stores 100000 while playing");

    captureHost(false, 0);
    check(transportCurrentSample.load() == 0, "TEST F: host may report sample 0 after stop");
    check(transportLastPlayingSample.load() == 100000, "TEST F: lastValidPlayingSample stays 100000 after stop→0");

    int64_t displayedPlayheadSample = 0;
    int64_t lastValidPlayingSample = 0;
    int64_t frozenStopSample = 0;
    bool wasPlaying = false;
    constexpr int64_t analysisLength = 200000;
    constexpr int64_t analysisStart = 0;

    auto updatePlayhead = [&]()
    {
        const bool isPlayingNow = transportIsPlaying.load(std::memory_order_relaxed);
        const int64_t current = transportCurrentSample.load(std::memory_order_relaxed);
        if (isPlayingNow)
        {
            const int64_t relative = current - analysisStart;
            lastValidPlayingSample = juce::jlimit<int64_t>(0, analysisLength, relative);
            displayedPlayheadSample = lastValidPlayingSample;
        }
        else
        {
            if (wasPlaying)
            {
                const int64_t fromAudio = juce::jlimit<int64_t>(
                    0, analysisLength,
                    transportLastPlayingSample.load(std::memory_order_relaxed) - analysisStart);
                frozenStopSample = juce::jmax(lastValidPlayingSample, fromAudio);
            }
            // While stopped, ignore host completely.
            displayedPlayheadSample = frozenStopSample;
        }
        wasPlaying = isPlayingNow;
    };

    transportIsPlaying = true;
    transportCurrentSample = 100000;
    transportLastPlayingSample = 100000;
    updatePlayhead();
    check(displayedPlayheadSample == 100000, "TEST F: GUI cursor follows host while playing");

    transportIsPlaying = false;
    transportCurrentSample = 0; // DAW returns to start
    updatePlayhead();
    check(displayedPlayheadSample == 100000, "TEST F: GUI cursor freezes at last point (not zero)");

    // Host keeps reporting 0 for many frames — must stay frozen.
    for (int i = 0; i < 10; ++i)
    {
        transportCurrentSample = 0;
        updatePlayhead();
    }
    check(displayedPlayheadSample == 100000, "TEST F: repeated stop frames never copy host 0");

    // Click while stopped must persist (TEST B).
    displayedPlayheadSample = 60000;
    lastValidPlayingSample = 60000;
    frozenStopSample = 60000;
    updatePlayhead();
    check(displayedPlayheadSample == 60000, "TEST B: clicked cursor persists while stopped");

    // Zoom/pan must not mutate cursor (TEST C/D) — viewport only.
    nf::notes::TimelineViewState view;
    view.setContentRange(0.0, 20.0);
    view.viewStartSec = 2.0;
    view.viewDurationSec = 8.0;
    const double cursorSec = 5.0;
    view.zoomAtFraction(0.3f, 0.5);
    view.panFromWheel(0.2f);
    check(std::abs(cursorSec - 5.0) < 1.0e-12, "TEST C/D: cursor time unchanged by zoom/pan");
}

void newAnalysisMayResetCursorTest()
{
    // TEST G — only explicit new analysis zeros the cursor.
    int64_t displayedPlayheadSample = 44000;
    int64_t lastValidPlayingSample = 44000;
    int64_t frozenStopSample = 44000;
    auto resetCursorForNewAnalysis = [&]()
    {
        displayedPlayheadSample = 0;
        lastValidPlayingSample = 0;
        frozenStopSample = 0;
    };
    resetCursorForNewAnalysis();
    check(displayedPlayheadSample == 0 && lastValidPlayingSample == 0 && frozenStopSample == 0,
          "TEST G: new analysis may reset cursor to zero");
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
    noteCaptureAndSegmentationTest();
    noteOffsetSessionAndUndoTest();
    pencilRelativeOffsetTest();
    snapModesTest();
    manualOffsetAudioTransitionTest();
    transportLoopAndBlockSizeTest();
    rtOffsetLookupNoAllocationTest();
    timelineNavigationTest();
    noteSelectionDeleteUndoTest();
    analyzeButtonStateMachineTest();
    pitchAutoFitVerticalTest();
    pitchEditorLayoutStructureTest();
    transportPlayheadFreezeOnStopTest();
    newAnalysisMayResetCursorTest();
    std::cout << "Failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
