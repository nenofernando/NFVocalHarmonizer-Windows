#pragma once
#include <JuceHeader.h>
#include "../Parameters.h"
#include <atomic>

class IntervalRail final : public juce::Component,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit IntervalRail(juce::AudioProcessorValueTreeState&);
    ~IntervalRail() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void parameterChanged(const juce::String&, float) override;
    int choiceForRow(int row) const;
    juce::AudioProcessorValueTreeState& state;
    std::atomic<int> selected { 4 };
};

