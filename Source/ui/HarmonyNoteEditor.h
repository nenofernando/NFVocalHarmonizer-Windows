#pragma once
#include <JuceHeader.h>
#include "../dsp/HarmonyNoteTypes.h"
#include "../dsp/MusicalScale.h"
#include "TimelineViewState.h"
#include <vector>

class NFVocalHarmonizerAudioProcessor;

/** Relative geometry for the VOICE/HARMONY panel (visual only). */
struct PitchEditorLayout
{
    juce::Rectangle<float> toolbar;
    juce::Rectangle<float> voiceHeader;
    juce::Rectangle<float> voiceNotes;
    juce::Rectangle<float> divider;
    juce::Rectangle<float> harmonyHeader;
    juce::Rectangle<float> harmonyNotes;

    juce::Rectangle<float> notesUnion() const noexcept
    {
        return voiceNotes.getUnion(harmonyNotes);
    }
};

class HarmonyNoteEditor final : public juce::Component,
                                private juce::Timer
{
public:
    explicit HarmonyNoteEditor(NFVocalHarmonizerAudioProcessor&);
    ~HarmonyNoteEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

    nf::notes::TimelineViewState& timeline() noexcept { return timelineView; }
    const nf::notes::TimelineViewState& timeline() const noexcept { return timelineView; }
    /** Combined note lanes (excludes toolbar/headers) — zoom/pan/playhead. */
    juce::Rectangle<float> getTimelineLaneBounds() const;
    const PitchEditorLayout& getLayout() const noexcept { return layout; }
    const std::vector<juce::String>& getSelectedIds() const noexcept { return selectedIds; }

    static constexpr float topToolbarHeight = 20.0f;
    static constexpr float laneHeaderHeight = 18.0f;
    static constexpr float laneContentHeight = 28.0f;
    static constexpr float dividerHeight = 1.0f;
    static constexpr float outerPadding = 5.0f;
    /** Preferred panel height in design pixels (toolbar + two equal lanes). */
    static constexpr int preferredPanelHeight = 123;

private:
    enum class DragMode { none, pitchEdit, marquee };

    void timerCallback() override;
    PitchEditorLayout computeLayout(juce::Rectangle<float> bounds) const;
    int hitTestNote(juce::Point<float> pos) const;
    juce::Rectangle<float> noteBounds(const nf::notes::HarmonyNote&, bool harmonyLane) const;
    float midiToY(float midi, bool harmonyLane) const;
    float yToMidi(float y, bool harmonyLane) const;
    void updateTooltip(float offsetSemitones);
    void commitPitchDrag(bool apply);
    void zoomTimelineAtMouse(float deltaY, float mouseX);
    void panTimelineFromWheel(float deltaY);
    void refreshContentRangeFromNotes();
    void showZoomHud();
    void clearSelection();
    void selectOnly(const juce::String& id);
    void toggleSelection(const juce::String& id);
    void addToSelection(const juce::String& id);
    bool isSelected(const juce::String& id) const;
    void selectAllVisibleNotes();
    void deleteSelectedNotesFromEditor();
    void applyMarqueeSelection(bool additive);
    std::pair<int, nf::dsp::ScaleType> keyScale() const;

    NFVocalHarmonizerAudioProcessor& processor;
    nf::notes::TimelineViewState timelineView;
    PitchEditorLayout layout;
    juce::ComboBox snapBox;
    juce::Label snapLabel;
    std::vector<juce::String> selectedIds;
    juce::String dragId;
    float dragStartOffset = 0.0f;
    float dragStartMidi = 0.0f;
    float dragStartY = 0.0f;
    DragMode dragMode = DragMode::none;
    bool fineDrag = false;
    bool userNavigatedTimeline = false;
    bool shiftMarqueeAdditive = false;
    juce::Point<float> marqueeOrigin;
    juce::Rectangle<float> marqueeRect;
    juce::String tooltipText;
    juce::uint32 zoomHudUntilMs = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonyNoteEditor)
};
