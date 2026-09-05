#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/NFLookAndFeel.h"
#include "ui/IntervalRail.h"
#include "ui/HarmonyNoteEditor.h"
#include "ui/StereoMeter.h"

class NFVocalHarmonizerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                    private juce::Timer
{
public:
    explicit NFVocalHarmonizerAudioProcessorEditor(NFVocalHarmonizerAudioProcessor&);
    ~NFVocalHarmonizerAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth   = 1100;
    // Header grows to 84 design px; editor panel taller for pitch visibility.
    static constexpr int designHeight  = 872;
    static constexpr int defaultWidth  = 900;
    static constexpr int defaultHeight = 713;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    class FixedCanvas final : public juce::Component
    {
    public:
        void paint(juce::Graphics&) override;
    };

    class TitleHitZone final : public juce::Component
    {
    public:
        explicit TitleHitZone(NFVocalHarmonizerAudioProcessorEditor& owner) : editor(owner)
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            if (e.mouseWasClicked())
                editor.resetToDefaultSize();
        }

    private:
        NFVocalHarmonizerAudioProcessorEditor& editor;
    };

    class ArrowButton final : public juce::Button
    {
    public:
        explicit ArrowButton(bool left) : juce::Button({}), pointsLeft(left)
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }

        void paint(juce::Graphics& g) override
        {
            paintButton(g, isMouseOverOrDragging(), isMouseButtonDown());
        }

        void paintButton(juce::Graphics& g, bool highlighted, bool down) override
        {
            auto area = getLocalBounds().toFloat().reduced(8.0f);
            if (highlighted) area = area.translated(0.0f, down ? 0.5f : -0.5f);

            juce::Path arrow;
            if (pointsLeft)
            {
                arrow.startNewSubPath(area.getRight(), area.getY());
                arrow.lineTo(area.getX(), area.getCentreY());
                arrow.lineTo(area.getRight(), area.getBottom());
            }
            else
            {
                arrow.startNewSubPath(area.getX(), area.getY());
                arrow.lineTo(area.getRight(), area.getCentreY());
                arrow.lineTo(area.getX(), area.getBottom());
            }

            g.setColour(juce::Colours::lightgrey.withAlpha(highlighted ? 1.0f : 0.85f));
            g.strokePath(arrow, juce::PathStrokeType(1.6f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }

    private:
        bool pointsLeft = true;
    };

    class MenuButton final : public juce::Button
    {
    public:
        MenuButton() : juce::Button({})
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }

        void paint(juce::Graphics& g) override
        {
            paintButton(g, isMouseOverOrDragging(), isMouseButtonDown());
        }

        void paintButton(juce::Graphics& g, bool highlighted, bool) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(1.0f);
            g.setColour(juce::Colour::fromRGB(12, 17, 24).brighter(highlighted ? 0.08f : 0.0f));
            g.fillRoundedRectangle(bounds, 5.0f);
            g.setColour(juce::Colour::fromRGB(55, 68, 82));
            g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

            g.setColour(juce::Colours::lightgrey.withAlpha(0.85f));
            const float centreX = bounds.getCentreX();
            const float centreY = bounds.getCentreY();
            for (float offset : { -5.0f, 0.0f, 5.0f })
                g.drawLine(centreX - 6.0f, centreY + offset, centreX + 6.0f, centreY + offset, 1.4f);
        }
    };

    void timerCallback() override;
    void configureKnob(juce::Slider&, const juce::String&, juce::Colour, int decimals, const juce::String& suffix);
    void layoutFixedCanvas();
    void resetToDefaultSize();
    void refreshPresetList(const juce::String& select = {});
    void refreshAnalyzeButtonVisual();
    void savePresetAsync();
    void switchAB(bool useA);
    void showError(const juce::String& message);
    void showHeaderMenu();
    void openEmbeddedManual(bool portuguese);

    NFVocalHarmonizerAudioProcessor& audioProcessor;
    juce::ComponentBoundsConstrainer constrainer;
    FixedCanvas canvas;
    TitleHitZone titleHitZone;
    NFLookAndFeel look;
    IntervalRail rail;
    HarmonyNoteEditor noteEditor;
    StereoMeter inputMeter { "INPUT" }, outputMeter { "OUTPUT" };

    juce::TextButton zoomOutButton { "+" }; // + = zoom out (as requested)
    juce::TextButton zoomInButton { "-" };  // - = zoom in (as requested)

    juce::Label titleLabel, subtitleLabel;
    juce::Slider harmony, formant, humanize, width, mix;
    juce::Label harmonyLabel, formantLabel, humanizeLabel, widthLabel, mixLabel;
    juce::ComboBox keyBox, scaleBox, presetBox;
    juce::TextButton autoKeyButton { "AUTO KEY" }, analyzeButton { "ANALYZE" };
    juce::TextButton harmonizeButton { "HARMONIZE" }, powerButton { "POWER" };
    ArrowButton prevButton { true }, nextButton { false };
    juce::TextButton aButton { "A" }, bButton { "B" };
    juce::TextButton copyButton { "COPY" }, saveButton { "SAVE" };
    MenuButton menuButton;
    juce::Label detectedLabel;

    std::unique_ptr<SliderAttachment> harmonyAtt, formantAtt, humanizeAtt, widthAtt, mixAtt;
    std::unique_ptr<ButtonAttachment> autoKeyAtt, harmonizeAtt, powerAtt;
    std::unique_ptr<ComboAttachment> keyAtt, scaleAtt;
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool activeA = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFVocalHarmonizerAudioProcessorEditor)
};
