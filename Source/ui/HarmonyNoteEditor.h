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
    void mouseMove(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

    nf::notes::TimelineViewState& timeline() noexcept { return timelineView; }
    const nf::notes::TimelineViewState& timeline() const noexcept { return timelineView; }
    juce::Rectangle<float> getTimelineLaneBounds() const;
    const PitchEditorLayout& getLayout() const noexcept { return layout; }
    const std::vector<juce::String>& getSelectedIds() const noexcept { return selectedIds; }

    /** Button zoom around the centre of the visible timeline. */
    void zoomTimelineIn();
    void zoomTimelineOut();

    void resetCursorForNewAnalysis() noexcept;
    void restoreCursorFromState(double seconds) noexcept;
    double getCursorSecondsForState() const noexcept;

    static constexpr float topToolbarHeight = 20.0f;
    static constexpr float laneHeaderHeight = 18.0f;
    static constexpr float laneContentHeight = 48.0f;
    static constexpr float dividerHeight = 1.0f;
    static constexpr float outerPadding = 5.0f;
    // Taller panel so VOICE/HARMONY lanes have more vertical pitch room.
    static constexpr int preferredPanelHeight = 188;

private:
    enum class DragMode { none, pitchEdit, marquee, pan, pitchPan, scrub };

    void timerCallback() override;
    PitchEditorLayout computeLayout(juce::Rectangle<float> bounds) const;
    int hitTestNote(juce::Point<float> pos) const;
    juce::Rectangle<float> noteBounds(const nf::notes::HarmonyNote&, bool harmonyLane) const;
    float midiToY(float midi, bool harmonyLane) const;
    float yToMidi(float y, bool harmonyLane) const;
    void drawPitchGrid(juce::Graphics& g, bool harmonyLane) const;
    void updateTooltip(float offsetSemitones);
    void commitPitchDrag(bool apply);
    void zoomTimelineAtMouse(float deltaY, float mouseX);
    void zoomPitchAtMouse(float deltaY, float mouseY);
    void panTimelineFromWheel(float deltaY);
    void panPitchFromWheel(float deltaY);
    void refreshContentRangeFromNotes();
    void fitViewsToNewNotesIfNeeded(bool notesIdentityChanged);
    void followPlayheadPage(double playSec);
    void showZoomHud();
    void clearSelection();
    void selectOnly(const juce::String& id);
    void toggleSelection(const juce::String& id);
    void addToSelection(const juce::String& id);
    bool isSelected(const juce::String& id) const;
    void selectAllVisibleNotes();
    void deleteSelectedNotesFromEditor();
    void applyMarqueeSelection(bool additive);
    void beginTimelinePan(float mouseX);
    void beginPitchPan(float mouseY);
    void beginMarquee(juce::Point<float> origin, bool additive);
    void capturePitchDragSnapshot();
    void applyPitchDragToSnapshot(float primaryProposedMidi);
    void updateTimelineCursor(juce::Point<float> pos, bool altDown, bool dragging);
    void setPlayheadFromX(float mouseX);
    double timeAtMouseX(float mouseX) const;
    double getDisplayPlayheadSeconds() const;
    bool isNearPlayhead(float mouseX) const;
    void updateCursorFromTransport();
    int64_t getAnalysisLengthSamples() const noexcept;
    void nudgeSelectedPitch(int direction, bool fineCents);
    std::pair<int, nf::dsp::ScaleType> keyScale() const;

    NFVocalHarmonizerAudioProcessor& processor;
    nf::notes::TimelineViewState timelineView;
    nf::notes::PitchViewState pitchView;
    PitchEditorLayout layout;
    juce::ComboBox snapBox;
    juce::Label snapLabel;
    std::vector<juce::String> selectedIds;
    juce::String dragId;
    float dragStartOffset = 0.0f;
    float dragStartMidi = 0.0f;
    float dragStartY = 0.0f;
    struct PitchDragSnapshot
    {
        juce::String id;
        float startEditedMidi = 0.0f;
        float startOffset = 0.0f;
    };
    std::vector<PitchDragSnapshot> pitchDragNotes;
    float panDragStartX = 0.0f;
    float panDragStartY = 0.0f;
    double panDragStartVisibleTime = 0.0;
    float panDragStartTopMidi = 84.0f;

    // Persistent cursor relative to analysisStartHostSample (never cleared on host stop).
    int64_t editorCursorSample = 0;
    int64_t lastDisplayedPlayingSample = 0;

    bool pendingEmptyGesture = false;
    juce::Point<float> emptyGestureOrigin;
    float emptyGestureStartX = 0.0f;
    float emptyGestureStartY = 0.0f;
    double emptyGestureStartView = 0.0;
    float emptyGestureStartTopMidi = 84.0f;
    DragMode dragMode = DragMode::none;
    bool fineDrag = false;
    bool userNavigatedTimeline = false;
    bool userNavigatedPitch = false;
    bool followPlayhead = true;
    bool wasPlaying = false;
    size_t lastFittedNoteCount = 0;
    double lastFittedContentStart = 0.0;
    double lastFittedContentEnd = 0.0;
    float lastFittedMidiMin = 0.0f;
    float lastFittedMidiMax = 0.0f;
    bool shiftMarqueeAdditive = false;
    juce::Point<float> marqueeOrigin;
    juce::Rectangle<float> marqueeRect;
    juce::String tooltipText;
    juce::uint32 zoomHudUntilMs = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonyNoteEditor)
};
