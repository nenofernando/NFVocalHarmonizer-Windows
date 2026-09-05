#include "MusicalScale.h"
#include <cmath>

namespace nf::dsp
{
void KeyAnalyzer::reset()
{
    histogram.fill(0.0f);
    observationCount = 0;
}

void KeyAnalyzer::addMidiNote(float midiNote, float confidence)
{
    if (! std::isfinite(midiNote) || confidence < 0.55f)
        return;

    for (auto& v : histogram)
        v *= 0.9985f;

    const auto pitchClass = ((juce::roundToInt(midiNote) % 12) + 12) % 12;
    histogram[static_cast<size_t>(pitchClass)] += juce::jlimit(0.0f, 1.0f, confidence);
    ++observationCount;
}

ScaleResult KeyAnalyzer::analyse() const
{
    static constexpr std::array<float, 12> majorProfile {
        6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static constexpr std::array<float, 12> minorProfile {
        6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

    auto isRelativePair = [](int rootA, ScaleType typeA, int rootB, ScaleType typeB) noexcept
    {
        if (typeA == ScaleType::major && typeB == ScaleType::naturalMinor)
            return ((rootA + 9) % 12) == rootB;
        if (typeA == ScaleType::naturalMinor && typeB == ScaleType::major)
            return ((rootA + 3) % 12) == rootB;
        return false;
    };

    ScaleResult result;
    if (observationCount < 8)
    {
        result.confidence = -1.0f; // not enough data — callers must keep previous key
        return result;
    }

    float best = -1.0f;
    float second = -1.0f;
    for (int root = 0; root < 12; ++root)
    {
        for (int mode = 0; mode < 2; ++mode)
        {
            const auto& profile = mode == 0 ? majorProfile : minorProfile;
            float score = 0.0f;
            float normA = 0.0f;
            float normB = 0.0f;
            for (int pc = 0; pc < 12; ++pc)
            {
                const auto h = histogram[static_cast<size_t>((pc + root) % 12)];
                const auto p = profile[static_cast<size_t>(pc)];
                score += h * p;
                normA += h * h;
                normB += p * p;
            }
            score /= std::sqrt(normA * normB + 1.0e-12f);
            const auto type = mode == 0 ? ScaleType::major : ScaleType::naturalMinor;
            if (score > best)
            {
                // Previous best becomes second only if it isn't the relative twin.
                if (best >= 0.0f
                    && ! isRelativePair(result.root, result.type, root, type))
                    second = best;
                best = score;
                result.root = root;
                result.type = type;
            }
            else if (score > second
                     && ! isRelativePair(result.root, result.type, root, type))
            {
                second = score;
            }
        }
    }

    result.confidence = juce::jlimit(0.0f, 1.0f, (best - juce::jmax(0.0f, second)) * 8.0f);
    return result;
}

std::vector<int> MusicalScale::degrees(ScaleType type)
{
    switch (type)
    {
        case ScaleType::major:         return { 0, 2, 4, 5, 7, 9, 11 };
        case ScaleType::naturalMinor:  return { 0, 2, 3, 5, 7, 8, 10 };
        case ScaleType::harmonicMinor: return { 0, 2, 3, 5, 7, 8, 11 };
        case ScaleType::melodicMinor:  return { 0, 2, 3, 5, 7, 9, 11 };
        case ScaleType::chromatic:     return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    }
    return { 0, 2, 3, 5, 7, 8, 10 };
}

int MusicalScale::intervalChoiceToDegreeOffset(int choiceIndex)
{
    // -8ve -6th -5th -3rd VOICE/tônica +3rd +5th +6th +8ve
    static constexpr std::array<int, 9> offsets { -7, -5, -4, -2, 0, 2, 4, 5, 7 };
    return offsets[static_cast<size_t>(juce::jlimit(0, 8, choiceIndex))];
}

float MusicalScale::targetMidi(float inputMidi, int intervalChoice, int root, ScaleType type)
{
    if (! std::isfinite(inputMidi))
        return inputMidi;

    if (type == ScaleType::chromatic)
    {
        static constexpr std::array<int, 9> semitones { -12, -9, -7, -4, 0, 4, 7, 9, 12 };
        return inputMidi + static_cast<float>(semitones[static_cast<size_t>(juce::jlimit(0, 8, intervalChoice))]);
    }

    const auto scale = degrees(type);
    const auto rounded = juce::roundToInt(inputMidi);
    const auto deviation = inputMidi - static_cast<float>(rounded);

    int bestDegree = 0;
    int bestScaleMidi = rounded;
    int bestDistance = 999;
    const int approxOctave = static_cast<int>(std::floor((rounded - root) / 12.0));

    for (int octave = approxOctave - 1; octave <= approxOctave + 1; ++octave)
    {
        for (int degree = 0; degree < static_cast<int>(scale.size()); ++degree)
        {
            const int candidate = root + octave * 12 + scale[static_cast<size_t>(degree)];
            const int distance = std::abs(candidate - rounded);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestDegree = octave * static_cast<int>(scale.size()) + degree;
                bestScaleMidi = candidate;
            }
        }
    }

    const int targetDegree = bestDegree + intervalChoiceToDegreeOffset(intervalChoice);
    const int scaleSize = static_cast<int>(scale.size());
    int octave = static_cast<int>(std::floor(targetDegree / static_cast<double>(scaleSize)));
    int degree = targetDegree - octave * scaleSize;
    if (degree < 0)
    {
        degree += scaleSize;
        --octave;
    }
    const int target = root + octave * 12 + scale[static_cast<size_t>(degree)];
    juce::ignoreUnused(bestScaleMidi);
    return static_cast<float>(target) + deviation;
}

bool MusicalScale::isInScale(int midiNote, int root, ScaleType type)
{
    const auto pc = ((midiNote - root) % 12 + 12) % 12;
    const auto scale = degrees(type);
    return std::find(scale.begin(), scale.end(), pc) != scale.end();
}
}

