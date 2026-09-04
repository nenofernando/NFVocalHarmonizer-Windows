#include "YinPitchDetector.h"
#include <cmath>

namespace nf::dsp
{
void YinPitchDetector::prepare(double newSampleRate)
{
    sampleRate = juce::jmax(8000.0, newSampleRate);
    decimation = juce::jlimit(1, 16, juce::roundToInt(sampleRate / 12000.0));
    detectorRate = sampleRate / static_cast<double>(decimation);
    reset();
}

void YinPitchDetector::reset()
{
    std::fill(ring.begin(), ring.end(), 0.0f);
    std::fill(frame.begin(), frame.end(), 0.0f);
    std::fill(difference.begin(), difference.end(), 0.0f);
    writeIndex = 0;
    samplesSinceAnalysis = 0;
    decimationCounter = 0;
    decimationAccumulator = 0.0f;
    estimate = {};
}

bool YinPitchDetector::pushSample(float monoSample)
{
    decimationAccumulator += monoSample;
    if (++decimationCounter < decimation)
        return false;

    const auto downsampled = decimationAccumulator / static_cast<float>(decimation);
    decimationCounter = 0;
    decimationAccumulator = 0.0f;

    ring[static_cast<size_t>(writeIndex)] = downsampled;
    writeIndex = (writeIndex + 1) % windowSize;
    if (++samplesSinceAnalysis >= hopSize)
    {
        samplesSinceAnalysis = 0;
        analyse();
        return true;
    }
    return false;
}

void YinPitchDetector::analyse()
{
    float rms = 0.0f;
    for (int i = 0; i < windowSize; ++i)
    {
        frame[static_cast<size_t>(i)] = ring[static_cast<size_t>((writeIndex + i) % windowSize)];
        rms += frame[static_cast<size_t>(i)] * frame[static_cast<size_t>(i)];
    }
    rms = std::sqrt(rms / static_cast<float>(windowSize));
    if (rms < 0.0025f)
    {
        estimate = {};
        return;
    }

    const int minTau = juce::jmax(2, static_cast<int>(detectorRate / 1000.0));
    const int maxTau = juce::jmin(windowSize / 2 - 2, static_cast<int>(detectorRate / 65.0));
    std::fill(difference.begin(), difference.end(), 0.0f);

    for (int tau = 1; tau <= maxTau; ++tau)
    {
        float sum = 0.0f;
        const int count = windowSize - tau;
        for (int i = 0; i < count; ++i)
        {
            const auto d = frame[static_cast<size_t>(i)] - frame[static_cast<size_t>(i + tau)];
            sum += d * d;
        }
        difference[static_cast<size_t>(tau)] = sum;
    }

    float running = 0.0f;
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        running += difference[static_cast<size_t>(tau)];
        difference[static_cast<size_t>(tau)] = difference[static_cast<size_t>(tau)]
                                               * static_cast<float>(tau)
                                               / juce::jmax(running, 1.0e-12f);
    }

    int bestTau = minTau;
    float bestValue = difference[static_cast<size_t>(minTau)];
    for (int tau = minTau; tau <= maxTau; ++tau)
    {
        const auto cmndf = difference[static_cast<size_t>(tau)];
        if (cmndf < bestValue) { bestValue = cmndf; bestTau = tau; }
        if (cmndf < 0.14f)
        {
            bestTau = tau;
            while (bestTau + 1 <= maxTau
                   && difference[static_cast<size_t>(bestTau + 1)] < difference[static_cast<size_t>(bestTau)])
                ++bestTau;
            bestValue = difference[static_cast<size_t>(bestTau)];
            break;
        }
    }

    if (bestTau < 0 || bestValue > 0.42f)
    {
        estimate = {};
        return;
    }

    float refinedTau = static_cast<float>(bestTau);
    if (bestTau > minTau && bestTau < maxTau)
    {
        const auto a = difference[static_cast<size_t>(bestTau - 1)];
        const auto b = difference[static_cast<size_t>(bestTau)];
        const auto c = difference[static_cast<size_t>(bestTau + 1)];
        const auto denom = a - 2.0f * b + c;
        if (std::abs(denom) > 1.0e-8f)
            refinedTau += 0.5f * (a - c) / denom;
    }

    const auto hz = static_cast<float>(detectorRate) / juce::jmax(1.0f, refinedTau);
    const auto midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
    estimate.frequencyHz = hz;
    estimate.midiNote = midi;
    estimate.confidence = juce::jlimit(0.0f, 1.0f, 1.0f - bestValue);
    estimate.voiced = std::isfinite(midi) && hz >= 65.0f && hz <= 1000.0f;
}
}
