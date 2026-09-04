#pragma once

#include <JuceHeader.h>
#include <array>

namespace nf::dsp
{
enum class ScaleType { major = 0, naturalMinor, harmonicMinor, melodicMinor, chromatic };

struct ScaleResult
{
    int root = 0;
    ScaleType type = ScaleType::naturalMinor;
    float confidence = 0.0f;
};

class KeyAnalyzer
{
public:
    void reset();
    void addMidiNote(float midiNote, float confidence);
    ScaleResult analyse() const;

private:
    std::array<float, 12> histogram {};
    int observationCount = 0;
};

class MusicalScale
{
public:
    static int intervalChoiceToDegreeOffset(int choiceIndex);
    static float targetMidi(float inputMidi, int intervalChoice, int root, ScaleType type);
    static bool isInScale(int midiNote, int root, ScaleType type);

private:
    static std::vector<int> degrees(ScaleType type);
};
}

