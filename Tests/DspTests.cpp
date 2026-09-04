#include <JuceHeader.h>
#include "dsp/YinPitchDetector.h"
#include "dsp/MusicalScale.h"
#include "dsp/GranularPitchShifter.h"
#include <cmath>
#include <iostream>

namespace
{
int failures = 0;
void check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    if (! condition) ++failures;
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
}

int main()
{
    scaleTests(); detectorTest(); keyAnalysisTest(); shifterSafetyTest();
    std::cout << "Failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
