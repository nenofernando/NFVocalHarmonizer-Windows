#pragma once

#include <JuceHeader.h>
#include <vector>

namespace nf::dsp
{
struct PitchEstimate
{
    float frequencyHz = 0.0f;
    float midiNote = 0.0f;
    float confidence = 0.0f;
    bool voiced = false;
};

class YinPitchDetector
{
public:
    void prepare(double newSampleRate);
    void reset();
    bool pushSample(float monoSample);
    PitchEstimate getEstimate() const noexcept { return estimate; }

private:
    void analyse();

    double sampleRate = 44100.0;
    double detectorRate = 11025.0;
    int decimation = 4;
    int decimationCounter = 0;
    float decimationAccumulator = 0.0f;
    int writeIndex = 0;
    int samplesSinceAnalysis = 0;
    static constexpr int windowSize = 512;
    static constexpr int hopSize = 96;
    std::vector<float> ring = std::vector<float>(static_cast<size_t>(windowSize), 0.0f);
    std::vector<float> frame = std::vector<float>(static_cast<size_t>(windowSize), 0.0f);
    std::vector<float> difference = std::vector<float>(static_cast<size_t>(windowSize / 2), 0.0f);
    PitchEstimate estimate;
};
}
