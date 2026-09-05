#pragma once
#include <JuceHeader.h>
#include "../dsp/AnalysisState.h"
#include "../dsp/HarmonyNoteTypes.h"
#include "../dsp/MusicalScale.h"
#include "TimelineViewState.h"
#include <vector>
#include <cstdint>

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

    /** Recalculate vertical pitch scale so every VOICE/HARMONY note is visible. */
    void fitPitchToAllNotes();

    void resetCursorForNewAnalysis() noexcept;
    void restoreCursorFromState(double seconds) noexcept;
    double getCursorSecondsForState() const noexcept;

    static constexpr float topToolbarHeight = 20.0f;
    static constexpr float laneHeaderHeight = 18.0f;
    static constexpr float laneContentHeight = 48.0f;
    static constexpr float dividerHeight = 1.0f;
    static constexpr float outerPadding = 5.0f;
    // Taller panel so VOICE/HARMONY lanes have more vertical pitch room.
    // Keep modest so the interval rail always fits +8 … -8.
    static constexpr int preferredPanelHeight = 160;

private:
    enum class EditTool { select = 0, pencilFlat, pencilSlope, scissors };
    enum class DragMode { none, pitchEdit, marquee, pan, pitchPan, scrub, pencil, scissors };

    void timerCallback() override;
    void setEditTool(EditTool tool);
    void refreshToolButtonStyles();
    void drawPencilPreview(juce::Graphics& g) const;
    void beginPencilGesture(const nf::notes::HarmonyNote& note, juce::Point<float> pos);
    void updatePencilGesture(juce::Point<float> pos, bool fine);
    void commitPencilGesture();
    void cancelPencilGesture();
    float proposedMidiFromY(float y) const;
    float offsetFromProposedMidi(const nf::notes::HarmonyNote& note, float proposedMidi) const;
    PitchEditorLayout computeLayout(juce::Rectangle<float> bounds) const;
    int hitTestNote(juce::Point<float> pos) const;
    juce::Rectangle<float> noteBounds(const nf::notes::HarmonyNote&, bool harmonyLane) const;
    float midiToY(float midi, bool harmonyLane) const;
    float yToMidi(float y, bool harmonyLane) const;
    void drawPitchGrid(juce::Graphics& g, bool harmonyLane) const;
    void drawWaveformBackground(juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawMidiScaleLabels(juce::Graphics& g) const;
    void drawVocalBlob(juce::Graphics& g, const nf::notes::HarmonyNote& note, bool harmonyLane, bool selected) const;
    void drawSelectionHud(juce::Graphics& g) const;
    const std::vector<nf::notes::PitchSample>& activeVizSamples() const noexcept;
    float amplitudeAtTime(double timeSec, const nf::notes::HarmonyNote& note) const;
    float pitchMidiAtTime(double timeSec, const nf::notes::HarmonyNote& note, bool harmonyLane) const;
    void updateTooltip(float offsetSemitones);
    void commitPitchDrag(bool apply);
    void zoomTimelineAtMouse(float deltaY, float mouseX);
    void zoomPitchAtMouse(float deltaY, float mouseY);
    void panTimelineFromWheel(float deltaY);
    void panPitchFromWheel(float deltaY);
    void refreshContentRangeFromNotes();
    void fitViewsToNewNotesIfNeeded(bool notesIdentityChanged);
    void applyAutoFitPitchVertical(bool allowShrink);
    float harmonyDisplayMidi(const nf::notes::HarmonyNote& note) const;
    float liveAutoHarmonyMidi(const nf::notes::HarmonyNote& note) const;
    void updateContinuousFollow();
    void showZoomHud();
    float timeToX(double timeSec, juce::Rectangle<float> lane) const noexcept;
    void syncDisplayedVisibleStartFromView() noexcept;
    void applyDisplayedVisibleStartToView() noexcept;
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
    void updatePlayhead();
    int64_t getAnalysisLengthSamples() const noexcept;
    void nudgeSelectedPitch(int direction, bool fineCents);
    std::pair<int, nf::dsp::ScaleType> keyScale() const;

    NFVocalHarmonizerAudioProcessor& processor;
    nf::notes::TimelineViewState timelineView;
    nf::notes::PitchViewState pitchView;
    PitchEditorLayout layout;
    juce::ComboBox snapBox;
    juce::Label snapLabel;
    juce::TextButton fitPitchButton { "FIT" };
    juce::TextButton selectToolButton { "SEL" };
    juce::TextButton pencilFlatButton { "FLAT" };
    juce::TextButton pencilSlopeButton { "LINE" };
    juce::TextButton scissorsButton { "CUT" };
    EditTool editTool = EditTool::select;
    std::vector<juce::String> selectedIds;
    juce::String pencilNoteId;
    double pencilT0 = 0.0;
    double pencilT1 = 0.0;
    float pencilOffset0 = 0.0f;
    float pencilOffset1 = 0.0f;
    float pencilMidi0 = 0.0f;
    float pencilMidi1 = 0.0f;
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

    // Persistent playhead (relative to analysisStart). Never copy host position while stopped.
    int64_t displayedPlayheadSample = 0;
    int64_t lastValidPlayingSample = 0;
    int64_t frozenStopSample = 0;

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
    /** Continuous-follow viewport origin (double). Notes + playhead share this via timeToX. */
    double displayedVisibleStart = 0.0;
    size_t lastFittedNoteCount = 0;
    double lastFittedContentStart = 0.0;
    double lastFittedContentEnd = 0.0;
    float lastFittedMidiMin = 0.0f;
    float lastFittedMidiMax = 0.0f;
    int lastIntervalChoice = -1;
    AnalysisState lastPitchFitAnalysisState = AnalysisState::empty;
    uint64_t lastPitchFitRevision = 0;
    bool shiftMarqueeAdditive = false;
    juce::Point<float> marqueeOrigin;
    juce::Rectangle<float> marqueeRect;
    juce::String tooltipText;
    juce::uint32 zoomHudUntilMs = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonyNoteEditor)
};
