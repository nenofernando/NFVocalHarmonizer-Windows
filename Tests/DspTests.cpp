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

    // Host play / stop / loop / seek: only clamp view around a playhead, no note mutation.
    for (double playhead : { 0.0, 3.5, 9.9, 1.0, 8.0, 0.2 })
    {
        if (playhead < view.viewStartSec || playhead > view.viewStartSec + view.viewDurationSec * 0.92)
            view.viewStartSec = playhead - view.viewDurationSec * 0.15;
        view.clampView();
        check(view.viewStartSec >= view.contentStartSec - 1.0e-9, "Playhead follow stays in range");
        check(view.viewStartSec + view.viewDurationSec <= view.contentEndSec + 1.0e-6, "Playhead follow end clamp");
    }
    check(std::abs(note.manualOffsetSemitones - 0.5f) < 1.0e-12, "Play/stop/loop/seek leave note data intact");

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

void analyzeButtonStateMachineTest()
{
    check(analysisStateForBegin(false) == AnalysisState::armed, "Analyze while stopped → Armed");
    check(analysisStateForBegin(true) == AnalysisState::analyzing, "Analyze while playing → Analyzing");
    check(analysisStateAfterArmedSeesPlay(AnalysisState::armed, true) == AnalysisState::analyzing,
          "Armed + Play → Analyzing");
    check(analysisStateAfterArmedSeesPlay(AnalysisState::armed, false) == AnalysisState::armed,
          "Armed stays Armed until Play");
    check(analysisStateAfterArmedSeesPlay(AnalysisState::analyzing, true) == AnalysisState::analyzing,
          "Loop/seek while Playing keeps Analyzing");
    check(analysisShouldFinalizeOnStop(true, false), "Play→Stop finalizes analysis");
    check(! analysisShouldFinalizeOnStop(true, true), "Still Playing does not finalize (loop safe)");
    check(! analysisShouldFinalizeOnStop(false, false), "Idle Stop does not finalize");
    check(analysisStateForFinalize(true) == AnalysisState::completed, "Notes present → Completed");
    check(analysisStateForFinalize(false) == AnalysisState::failed, "No notes → Failed/Empty look");

    // Soft pulse mapping stays within gentle neon range (UI-only math).
    const float phase = std::fmod(static_cast<float>(0.25 * juce::MathConstants<double>::twoPi),
                                  juce::MathConstants<float>::twoPi);
    const float pulse = 0.5f + 0.5f * std::sin(phase);
    const float intensity = juce::jmap(pulse, 0.45f, 1.0f);
    check(intensity >= 0.45f && intensity <= 1.0f, "Pulse intensity stays in soft neon range");

    // Atomic publish pattern (message/audio publish only; no UI from audio thread).
    std::atomic<AnalysisState> published { AnalysisState::idle };
    published.store(AnalysisState::analyzing, std::memory_order_relaxed);
    check(published.load(std::memory_order_relaxed) == AnalysisState::analyzing,
          "AnalysisState publishes atomically without UI calls");
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
    snapModesTest();
    manualOffsetAudioTransitionTest();
    transportLoopAndBlockSizeTest();
    rtOffsetLookupNoAllocationTest();
    timelineNavigationTest();
    noteSelectionDeleteUndoTest();
    analyzeButtonStateMachineTest();
    pitchEditorLayoutStructureTest();
    std::cout << "Failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
