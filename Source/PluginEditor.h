#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/NFLookAndFeel.h"
#include "ui/IntervalRail.h"
#include "ui/PitchTrace.h"
#include "ui/StereoMeter.h"

class NFVocalHarmonizerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                    private juce::Timer
{
public:
    explicit NFVocalHarmonizerAudioProcessorEditor(NFVocalHarmonizerAudioProcessor&);
    ~NFVocalHarmonizerAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth  = 1100;
    static constexpr int designHeight = 734;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    class FixedCanvas final : public juce::Component
    {
    public:
        void paint(juce::Graphics&) override;
    };

    void timerCallback() override;
    void configureKnob(juce::Slider&, const juce::String&, juce::Colour, int decimals, const juce::String& suffix);
    void layoutFixedCanvas();
    void refreshPresetList(const juce::String& select = {});
    void savePresetAsync();
    void switchAB(bool useA);
    void showError(const juce::String& message);

    NFVocalHarmonizerAudioProcessor& audioProcessor;
    juce::ComponentBoundsConstrainer constrainer;
    FixedCanvas canvas;
    NFLookAndFeel look;
    IntervalRail rail;
    PitchTrace trace;
    StereoMeter inputMeter { "INPUT" }, outputMeter { "OUTPUT" };

    juce::Slider harmony, formant, humanize, width, mix;
    juce::Label harmonyLabel, formantLabel, humanizeLabel, widthLabel, mixLabel;
    juce::ComboBox keyBox, scaleBox, presetBox;
    juce::TextButton autoKeyButton { "AUTO KEY" }, analyzeButton { "ANALYZE" };
    juce::TextButton harmonizeButton { "HARMONIZE" }, powerButton { "POWER" };
    juce::TextButton prevButton { "<" }, nextButton { ">" }, aButton { "A" }, bButton { "B" };
    juce::TextButton copyButton { "COPY" }, saveButton { "SAVE" };
    juce::Label detectedLabel;

    std::unique_ptr<SliderAttachment> harmonyAtt, formantAtt, humanizeAtt, widthAtt, mixAtt;
    std::unique_ptr<ButtonAttachment> autoKeyAtt, harmonizeAtt, powerAtt;
    std::unique_ptr<ComboAttachment> keyAtt, scaleAtt;
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool activeA = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFVocalHarmonizerAudioProcessorEditor)
};
