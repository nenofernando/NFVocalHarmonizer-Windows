#pragma once
#include <JuceHeader.h>

class StereoMeter final : public juce::Component
{
public:
    explicit StereoMeter(juce::String titleText) : title(std::move(titleText)) {}
    void setLevels(float l, float r) { left = l; right = r; repaint(); }
    void paint(juce::Graphics&) override;
private:
    juce::String title;
    float left = 0.0f, right = 0.0f;
};

