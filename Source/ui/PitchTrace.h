#pragma once
#include <JuceHeader.h>
#include <array>

class PitchTrace final : public juce::Component
{
public:
    void add(float voiceMidi, float harmonyMidi, bool voiced);
    void paint(juce::Graphics&) override;
private:
    std::array<float, 160> voice {}, harmony {};
    std::array<bool, 160> valid {};
};

