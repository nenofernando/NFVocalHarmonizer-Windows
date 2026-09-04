#pragma once

#include <JuceHeader.h>
#include "YinPitchDetector.h"
#include "MusicalScale.h"
#include "GranularPitchShifter.h"
#include <atomic>

namespace nf::dsp
{
struct HarmonySettings
{
    int intervalChoice = 4;
    float harmonyPercent = 70.0f;
    float formantSemitones = 0.0f;
    float humanizePercent = 35.0f;
    float widthPercent = 100.0f;
    float mixPercent = 50.0f;
    int key = 7;
    ScaleType scale = ScaleType::naturalMinor;
    bool autoKey = true;
    bool enabled = false;
};

struct MeterSnapshot
{
    float inputPeakL = 0.0f, inputPeakR = 0.0f;
    float outputPeakL = 0.0f, outputPeakR = 0.0f;
};

class HarmonyEngine
{
public:
    void prepare(double sampleRate, int maximumBlockSize, int channels);
    void reset();
    void resetKeyAnalysis() { resetKeyRequested.store(true, std::memory_order_release); }
    void process(juce::AudioBuffer<float>& buffer, const HarmonySettings& settings);

    PitchEstimate getPitchEstimate() const noexcept;
    ScaleResult getDetectedScale() const noexcept;
    MeterSnapshot getMeters() const noexcept;
    int getLatencySamples() const noexcept { return latencySamples; }

private:
    float nextRandomBipolar() noexcept;
    float processFormantColour(int channel, float sample, float formantSemitones) noexcept;
    float readDryDelay(int channel, float input) noexcept;

    double currentSampleRate = 44100.0;
    int maxBlock = 512;
    int numChannels = 2;
    int latencySamples = 0;
    YinPitchDetector pitchDetector;
    KeyAnalyzer keyAnalyzer;
    GranularPitchShifter leftShifter, rightShifter;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> ratioSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> voicedSmooth;

    std::array<std::vector<float>, 2> dryDelay;
    std::array<int, 2> dryWrite { 0, 0 };
    std::array<float, 2> formantLowpass { 0.0f, 0.0f };
    uint32_t randomState = 0x75A3E19Du;
    float driftL = 0.0f, driftR = 0.0f;

    std::atomic<float> pitchHz { 0.0f }, pitchMidi { 0.0f }, pitchConfidence { 0.0f };
    std::atomic<bool> pitchVoiced { false };
    std::atomic<int> detectedRoot { 7 }, detectedType { static_cast<int>(ScaleType::naturalMinor) };
    std::atomic<float> detectedConfidence { 0.0f };
    std::atomic<bool> resetKeyRequested { false };
    std::atomic<float> inPeakL { 0.0f }, inPeakR { 0.0f }, outPeakL { 0.0f }, outPeakR { 0.0f };
};
}
