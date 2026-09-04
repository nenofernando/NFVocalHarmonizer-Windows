#pragma once

#include <JuceHeader.h>
#include <vector>

namespace nf::dsp
{
class GranularPitchShifter
{
public:
    void prepare(double sampleRate, int maximumBlockSize);
    void reset();
    float processSample(float input, float ratio);
    int getLatencySamples() const noexcept { return minDelay + grainSamples / 2; }

private:
    float readDelay(float delaySamples) const;
    static float hann(float phase) noexcept;

    std::vector<float> delay;
    int writeIndex = 0;
    int grainSamples = 1024;
    int minDelay = 96;
    float phase = 0.0f;
};
}
