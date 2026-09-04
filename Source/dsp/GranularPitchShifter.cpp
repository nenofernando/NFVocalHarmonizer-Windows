#include "GranularPitchShifter.h"
#include <cmath>

namespace nf::dsp
{
void GranularPitchShifter::prepare(double sampleRate, int maximumBlockSize)
{
    grainSamples = juce::jlimit(512, 4096, juce::roundToInt(sampleRate * 0.024));
    minDelay = juce::jmax(32, juce::roundToInt(sampleRate * 0.002));
    const int required = grainSamples + minDelay + maximumBlockSize + 8;
    delay.assign(static_cast<size_t>(juce::nextPowerOfTwo(required)), 0.0f);
    reset();
}

void GranularPitchShifter::reset()
{
    std::fill(delay.begin(), delay.end(), 0.0f);
    writeIndex = 0;
    phase = 0.0f;
}

float GranularPitchShifter::hann(float p) noexcept
{
    return 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * p);
}

float GranularPitchShifter::readDelay(float delaySamples) const
{
    float position = static_cast<float>(writeIndex) - delaySamples;
    const auto size = static_cast<int>(delay.size());
    while (position < 0.0f)
        position += static_cast<float>(size);
    while (position >= static_cast<float>(size))
        position -= static_cast<float>(size);

    const int i0 = static_cast<int>(position);
    const int i1 = (i0 + 1) % size;
    const float frac = position - static_cast<float>(i0);
    return delay[static_cast<size_t>(i0)]
         + frac * (delay[static_cast<size_t>(i1)] - delay[static_cast<size_t>(i0)]);
}

float GranularPitchShifter::processSample(float input, float ratio)
{
    if (delay.empty())
        return input;

    delay[static_cast<size_t>(writeIndex)] = input;
    ratio = juce::jlimit(0.45f, 2.10f, ratio);
    const auto speed = std::abs(1.0f - ratio) / static_cast<float>(grainSamples);
    phase += speed;
    phase -= std::floor(phase);

    const auto p1 = phase;
    const auto p2 = std::fmod(phase + 0.5f, 1.0f);
    const auto range = static_cast<float>(grainSamples);
    const auto d1 = static_cast<float>(minDelay) + (ratio >= 1.0f ? (1.0f - p1) : p1) * range;
    const auto d2 = static_cast<float>(minDelay) + (ratio >= 1.0f ? (1.0f - p2) : p2) * range;
    const auto w1 = hann(p1);
    const auto w2 = hann(p2);
    const auto wet = (readDelay(d1) * w1 + readDelay(d2) * w2) / juce::jmax(0.001f, w1 + w2);

    writeIndex = (writeIndex + 1) % static_cast<int>(delay.size());
    return wet;
}
}

