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
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    void parameterChanged(const juce::String&, float) override;
    int choiceForRow(int row) const;
    /** Returns interval choice index, or -1 if the click is outside any selectable circle. */
    int hitTestIntervalCircle(juce::Point<float> pos) const;

    juce::AudioProcessorValueTreeState& state;
    std::atomic<int> selected { 4 };
};
