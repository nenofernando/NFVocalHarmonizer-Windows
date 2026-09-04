#include "HarmonyEngine.h"
#include <cmath>

namespace nf::dsp
{
void HarmonyEngine::prepare(double sampleRate, int maximumBlockSize, int channels)
{
    currentSampleRate = sampleRate;
    maxBlock = maximumBlockSize;
    numChannels = juce::jlimit(1, 2, channels);
    pitchDetector.prepare(sampleRate);
    leftShifter.prepare(sampleRate, maximumBlockSize);
    rightShifter.prepare(sampleRate, maximumBlockSize);
    latencySamples = leftShifter.getLatencySamples();

    const int delaySize = juce::nextPowerOfTwo(latencySamples + maximumBlockSize + 16);
    for (auto& line : dryDelay)
        line.assign(static_cast<size_t>(delaySize), 0.0f);

    ratioSmooth.reset(sampleRate, 0.035);
    ratioSmooth.setCurrentAndTargetValue(1.0f);
    voicedSmooth.reset(sampleRate, 0.025);
    voicedSmooth.setCurrentAndTargetValue(0.0f);
    reset();
}

void HarmonyEngine::reset()
{
    pitchDetector.reset();
    keyAnalyzer.reset();
    leftShifter.reset();
    rightShifter.reset();
    for (auto& line : dryDelay)
        std::fill(line.begin(), line.end(), 0.0f);
    dryWrite = { 0, 0 };
    formantLowpass = { 0.0f, 0.0f };
    driftL = driftR = 0.0f;
    randomState = 0x75A3E19Du;
    ratioSmooth.setCurrentAndTargetValue(1.0f);
    voicedSmooth.setCurrentAndTargetValue(0.0f);
    pitchHz.store(0.0f); pitchMidi.store(0.0f); pitchConfidence.store(0.0f); pitchVoiced.store(false);
    detectedRoot.store(7); detectedType.store(static_cast<int>(ScaleType::naturalMinor)); detectedConfidence.store(0.0f);
    resetKeyRequested.store(false);
    inPeakL.store(0.0f); inPeakR.store(0.0f); outPeakL.store(0.0f); outPeakR.store(0.0f);
}

float HarmonyEngine::nextRandomBipolar() noexcept
{
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return static_cast<float>(randomState & 0x00ffffffu) / 8388607.5f - 1.0f;
}

float HarmonyEngine::readDryDelay(int channel, float input) noexcept
{
    auto& line = dryDelay[static_cast<size_t>(juce::jlimit(0, 1, channel))];
    auto& write = dryWrite[static_cast<size_t>(juce::jlimit(0, 1, channel))];
    if (line.empty())
        return input;
    line[static_cast<size_t>(write)] = input;
    int read = write - latencySamples;
    if (read < 0)
        read += static_cast<int>(line.size());
    const auto out = line[static_cast<size_t>(read)];
    write = (write + 1) % static_cast<int>(line.size());
    return out;
}

float HarmonyEngine::processFormantColour(int channel, float sample, float formantSemitones) noexcept
{
    const auto norm = juce::jlimit(-1.0f, 1.0f, formantSemitones / 12.0f);
    const auto cutoff = 1800.0f * std::pow(2.0f, formantSemitones / 24.0f);
    const auto a = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * cutoff
                                   / static_cast<float>(currentSampleRate));
    auto& lp = formantLowpass[static_cast<size_t>(juce::jlimit(0, 1, channel))];
    lp += a * (sample - lp);
    const auto high = sample - lp;
    return sample + norm * (norm >= 0.0f ? 0.55f * high : 0.45f * lp);
}

void HarmonyEngine::process(juce::AudioBuffer<float>& buffer, const HarmonySettings& s)
{
    if (resetKeyRequested.exchange(false, std::memory_order_acq_rel))
    {
        keyAnalyzer.reset();
        detectedConfidence.store(0.0f, std::memory_order_relaxed);
    }
    const int channels = juce::jmin(buffer.getNumChannels(), 2);
    const int samples = buffer.getNumSamples();
    if (channels == 0 || samples == 0)
        return;

    float localInL = 0.0f, localInR = 0.0f, localOutL = 0.0f, localOutR = 0.0f;
    int activeRoot = s.key;
    auto activeScale = s.scale;

    if (s.autoKey)
    {
        const auto detected = getDetectedScale();
        if (detected.confidence > 0.04f)
        {
            activeRoot = detected.root;
            activeScale = detected.type;
        }
    }

    const auto mix = juce::jlimit(0.0f, 1.0f, s.mixPercent * 0.01f);
    const auto dryGain = s.enabled ? std::cos(mix * juce::MathConstants<float>::halfPi) : 1.0f;
    const auto wetGain = std::sin(mix * juce::MathConstants<float>::halfPi)
                       * juce::jlimit(0.0f, 1.25f, s.harmonyPercent * 0.01f);
    const auto width = juce::jlimit(0.0f, 1.0f, s.widthPercent * 0.01f);
    const auto human = juce::jlimit(0.0f, 1.0f, s.humanizePercent * 0.01f);

    auto* left = buffer.getWritePointer(0);
    auto* right = channels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < samples; ++i)
    {
        const float inputL = left[i];
        const float inputR = right != nullptr ? right[i] : inputL;
        const float mono = 0.5f * (inputL + inputR);
        localInL = juce::jmax(localInL, std::abs(inputL));
        localInR = juce::jmax(localInR, std::abs(inputR));

        if (pitchDetector.pushSample(mono))
        {
            const auto estimate = pitchDetector.getEstimate();
            pitchHz.store(estimate.frequencyHz, std::memory_order_relaxed);
            pitchMidi.store(estimate.midiNote, std::memory_order_relaxed);
            pitchConfidence.store(estimate.confidence, std::memory_order_relaxed);
            pitchVoiced.store(estimate.voiced, std::memory_order_relaxed);

            if (estimate.voiced)
            {
                keyAnalyzer.addMidiNote(estimate.midiNote, estimate.confidence);
                if (s.autoKey)
                {
                    const auto result = keyAnalyzer.analyse();
                    detectedRoot.store(result.root, std::memory_order_relaxed);
                    detectedType.store(static_cast<int>(result.type), std::memory_order_relaxed);
                    detectedConfidence.store(result.confidence, std::memory_order_relaxed);
                    if (result.confidence > 0.04f)
                    {
                        activeRoot = result.root;
                        activeScale = result.type;
                    }
                }

                const auto target = MusicalScale::targetMidi(estimate.midiNote, s.intervalChoice,
                                                              activeRoot, activeScale);
                ratioSmooth.setTargetValue(std::pow(2.0f, (target - estimate.midiNote) / 12.0f));
                voicedSmooth.setTargetValue(1.0f);
            }
            else
            {
                voicedSmooth.setTargetValue(0.0f);
            }
        }

        driftL += 0.00035f * (nextRandomBipolar() - driftL);
        driftR += 0.00029f * (nextRandomBipolar() - driftR);
        const auto ratio = ratioSmooth.getNextValue();
        const auto centsAmount = 7.0f * human * width;
        const auto ratioL = ratio * std::pow(2.0f, driftL * centsAmount / 1200.0f);
        const auto ratioR = ratio * std::pow(2.0f, driftR * centsAmount / 1200.0f);
        const auto voiced = voicedSmooth.getNextValue();

        auto harmonyL = processFormantColour(0, leftShifter.processSample(mono, ratioL), s.formantSemitones);
        auto harmonyR = processFormantColour(1, rightShifter.processSample(mono, ratioR), s.formantSemitones);
        const auto harmonyMono = 0.5f * (harmonyL + harmonyR);
        harmonyL = juce::jmap(width, harmonyMono, harmonyL);
        harmonyR = juce::jmap(width, harmonyMono, harmonyR);

        const auto dryL = readDryDelay(0, inputL);
        const auto dryR = readDryDelay(1, inputR);
        const auto activeWet = s.enabled ? wetGain * voiced : 0.0f;
        const auto adaptiveDry = s.enabled ? 1.0f + (dryGain - 1.0f) * voiced : 1.0f;
        left[i] = adaptiveDry * dryL + activeWet * harmonyL;
        if (right != nullptr)
            right[i] = adaptiveDry * dryR + activeWet * harmonyR;

        localOutL = juce::jmax(localOutL, std::abs(left[i]));
        localOutR = juce::jmax(localOutR, std::abs(right != nullptr ? right[i] : left[i]));
    }

    inPeakL.store(localInL, std::memory_order_relaxed);
    inPeakR.store(localInR, std::memory_order_relaxed);
    outPeakL.store(localOutL, std::memory_order_relaxed);
    outPeakR.store(localOutR, std::memory_order_relaxed);
}

PitchEstimate HarmonyEngine::getPitchEstimate() const noexcept
{
    return { pitchHz.load(std::memory_order_relaxed), pitchMidi.load(std::memory_order_relaxed),
             pitchConfidence.load(std::memory_order_relaxed), pitchVoiced.load(std::memory_order_relaxed) };
}

ScaleResult HarmonyEngine::getDetectedScale() const noexcept
{
    return { detectedRoot.load(std::memory_order_relaxed),
             static_cast<ScaleType>(detectedType.load(std::memory_order_relaxed)),
             detectedConfidence.load(std::memory_order_relaxed) };
}

MeterSnapshot HarmonyEngine::getMeters() const noexcept
{
    return { inPeakL.load(std::memory_order_relaxed), inPeakR.load(std::memory_order_relaxed),
             outPeakL.load(std::memory_order_relaxed), outPeakR.load(std::memory_order_relaxed) };
}
}
