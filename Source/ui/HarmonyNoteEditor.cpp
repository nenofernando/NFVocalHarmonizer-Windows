#include "HarmonyNoteEditor.h"
#include "../PluginProcessor.h"
#include <cmath>
#include <algorithm>

namespace
{
juce::String formatOffsetTooltip(float offsetSemitones)
{
    const int semis = juce::roundToInt(std::floor(offsetSemitones));
    const int cents = juce::roundToInt((offsetSemitones - static_cast<float>(semis)) * 100.0f);
    juce::String text;
    if (semis != 0)
        text << (semis > 0 ? "+" : "") << semis << " st";
    if (cents != 0 || semis == 0)
    {
        if (text.isNotEmpty())
            text << " ";
        text << (cents > 0 ? "+" : "") << cents << " ct";
    }
    return text;
}
}

HarmonyNoteEditor::HarmonyNoteEditor(NFVocalHarmonizerAudioProcessor& p)
    : processor(p)
{
    setWantsKeyboardFocus(true);
    snapLabel.setText("SNAP", juce::dontSendNotification);
    snapLabel.setJustificationType(juce::Justification::centredRight);
    snapLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc9d0d8));
    snapLabel.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    addAndMakeVisible(snapLabel);

    snapBox.addItem("KEY", 1);
    snapBox.addItem("CHROMATIC", 2);
    snapBox.addItem("OFF", 3);
    snapBox.setSelectedItemIndex(static_cast<int>(processor.noteModel.getSnapMode()), juce::dontSendNotification);
    snapBox.setWantsKeyboardFocus(false); // keep Delete/Backspace on the editor, not the DAW
    snapBox.onChange = [this]
    {
        processor.noteModel.setSnapMode(static_cast<nf::notes::SnapMode>(snapBox.getSelectedItemIndex()));
    };
    addAndMakeVisible(snapBox);
    startTimerHz(60);
}

HarmonyNoteEditor::~HarmonyNoteEditor()
{
    stopTimer();
}

void HarmonyNoteEditor::resized()
{
    layout = computeLayout(getLocalBounds().toFloat());
    auto bar = layout.toolbar.toNearestInt();
    snapBox.setBounds(bar.removeFromRight(110).reduced(0, 1));
    snapLabel.setBounds(bar.removeFromRight(48).reduced(0, 1));
    timelineView.clampView();
}

PitchEditorLayout HarmonyNoteEditor::computeLayout(juce::Rectangle<float> bounds) const
{
    PitchEditorLayout L;
    auto inner = bounds.reduced(outerPadding);
    L.toolbar = inner.removeFromTop(topToolbarHeight);

    // Two symmetric lanes: header + notes each, with a 1px divider between.
    const float remaining = juce::jmax(1.0f, inner.getHeight());
    const float fixed = laneHeaderHeight * 2.0f + dividerHeight;
    float contentH = (remaining - fixed) * 0.5f;
    // Prefer ~28px when space allows; never collapse below a usable note row.
    contentH = juce::jmax(18.0f, contentH);

    L.voiceHeader = inner.removeFromTop(laneHeaderHeight);
    L.voiceNotes = inner.removeFromTop(contentH);
    L.divider = inner.removeFromTop(dividerHeight);
    L.harmonyHeader = inner.removeFromTop(laneHeaderHeight);
    L.harmonyNotes = inner.removeFromTop(contentH);
    // Any leftover (rounding) stays unused below — keeps lanes equal.
    return L;
}

juce::Rectangle<float> HarmonyNoteEditor::getTimelineLaneBounds() const
{
    return layout.notesUnion();
}

std::pair<int, nf::dsp::ScaleType> HarmonyNoteEditor::keyScale() const
{
    auto root = juce::roundToInt(processor.apvts.getRawParameterValue(nf::params::key)->load());
    auto scale = static_cast<nf::dsp::ScaleType>(juce::roundToInt(processor.apvts.getRawParameterValue(nf::params::scale)->load()));
    if (processor.apvts.getRawParameterValue(nf::params::autoKey)->load() > 0.5f)
    {
        const auto detected = processor.getDetectedScale();
        if (detected.confidence > 0.20f)
        {
            root = detected.root;
            scale = detected.type;
        }
    }
    return { root, scale };
}

float HarmonyNoteEditor::midiToY(float midi, bool harmonyLane) const
{
    // Notes map exclusively into voiceNotes / harmonyNotes — never into headers.
    auto r = harmonyLane ? layout.harmonyNotes : layout.voiceNotes;
    // ≥5 px clearance under the lane header text (header is a separate rect).
    r = r.withTrimmedTop(5.0f).withTrimmedBottom(2.0f);
    const float span = juce::jmax(1.0f, pitchView.viewSpanMidi);
    // Do not clamp: notes outside the visible pitch band fall off-screen until the user pans.
    const float frac = (pitchView.viewTopMidi - midi) / span;
    return r.getY() + frac * juce::jmax(1.0f, r.getHeight());
}

float HarmonyNoteEditor::yToMidi(float y, bool harmonyLane) const
{
    auto r = harmonyLane ? layout.harmonyNotes : layout.voiceNotes;
    r = r.withTrimmedTop(5.0f).withTrimmedBottom(2.0f);
    const float frac = juce::jlimit(0.0f, 1.0f, (y - r.getY()) / juce::jmax(1.0f, r.getHeight()));
    return pitchView.viewTopMidi - frac * pitchView.viewSpanMidi;
}

void HarmonyNoteEditor::drawPitchGrid(juce::Graphics& g, bool harmonyLane) const
{
    auto r = harmonyLane ? layout.harmonyNotes : layout.voiceNotes;
    r = r.withTrimmedTop(5.0f).withTrimmedBottom(2.0f);
    if (r.getHeight() < 8.0f || r.getWidth() < 8.0f)
        return;

    const float midiTop = pitchView.viewTopMidi;
    const float midiBottom = pitchView.viewBottomMidi();
    const float midiSpan = juce::jmax(1.0f, pitchView.viewSpanMidi);
    const float pxPerSemi = r.getHeight() / midiSpan;

    int step = 1;
    if (pxPerSemi < 2.2f)
        step = 2;
    if (pxPerSemi < 1.2f)
        step = 3;

    const int midiLo = static_cast<int>(std::floor(midiBottom));
    const int midiHi = static_cast<int>(std::ceil(midiTop));
    for (int midi = midiLo; midi <= midiHi; ++midi)
    {
        const bool isOctaveC = (midi % 12) == 0;
        const bool isWholeTone = (midi % 2) == 0;
        if (! isOctaveC && (midi % step) != 0)
            continue;

        const float y = midiToY(static_cast<float>(midi), harmonyLane);
        if (y < r.getY() - 0.5f || y > r.getBottom() + 0.5f)
            continue;

        if (isOctaveC)
        {
            g.setColour(juce::Colour(0xff3a4654));
            g.drawLine(r.getX(), y, r.getRight(), y, 1.0f);
        }
        else if (step == 1 && ! isWholeTone)
        {
            g.setColour(juce::Colour(0xff161c24));
            g.drawLine(r.getX(), y, r.getRight(), y, 0.6f);
        }
        else
        {
            g.setColour(juce::Colour(0xff1e2630));
            g.drawLine(r.getX(), y, r.getRight(), y, 0.8f);
        }
    }
}

juce::Rectangle<float> HarmonyNoteEditor::noteBounds(const nf::notes::HarmonyNote& note, bool harmonyLane) const
{
    // Horizontal span uses the active notes lane width (same X for both lanes).
    const auto lane = harmonyLane ? layout.harmonyNotes : layout.voiceNotes;
    const auto dur = juce::jmax(1.0e-9, timelineView.viewDurationSec);
    const float x0 = lane.getX() + static_cast<float>((note.startSec - timelineView.viewStartSec) / dur) * lane.getWidth();
    const float x1 = lane.getX() + static_cast<float>((note.startSec + note.durationSec - timelineView.viewStartSec) / dur) * lane.getWidth();
    const float midi = harmonyLane ? note.editedHarmonyMidi() : note.voiceMidi;
    const float y = midiToY(midi, harmonyLane);
    const float noteH = 12.0f;
    auto bounds = juce::Rectangle<float>(x0, y - noteH * 0.5f, juce::jmax(4.0f, x1 - x0), noteH);
    // Clip to lane — notes outside the visible pitch/time window stay off-screen (no fake centre fallback).
    return bounds.getIntersection(lane);
}

int HarmonyNoteEditor::hitTestNote(juce::Point<float> pos) const
{
    const auto& notes = processor.noteModel.getNotes();
    for (int i = static_cast<int>(notes.size()) - 1; i >= 0; --i)
    {
        const auto& n = notes[static_cast<size_t>(i)];
        // Voice + Harmony are the same musical event — either hit selects the pair.
        if (noteBounds(n, true).contains(pos) || noteBounds(n, false).contains(pos))
            return i;
    }
    return -1;
}

bool HarmonyNoteEditor::isSelected(const juce::String& id) const
{
    return std::find(selectedIds.begin(), selectedIds.end(), id) != selectedIds.end();
}

void HarmonyNoteEditor::clearSelection()
{
    selectedIds.clear();
}

void HarmonyNoteEditor::selectOnly(const juce::String& id)
{
    selectedIds = { id };
}

void HarmonyNoteEditor::addToSelection(const juce::String& id)
{
    if (! isSelected(id))
        selectedIds.push_back(id);
}

void HarmonyNoteEditor::toggleSelection(const juce::String& id)
{
    if (isSelected(id))
        selectedIds.erase(std::remove(selectedIds.begin(), selectedIds.end(), id), selectedIds.end());
    else
        selectedIds.push_back(id);
}

void HarmonyNoteEditor::selectAllVisibleNotes()
{
    selectedIds.clear();
    const auto view = layout.notesUnion();
    for (const auto& n : processor.noteModel.getNotes())
    {
        if (noteBounds(n, true).intersects(view) || noteBounds(n, false).intersects(view))
            selectedIds.push_back(n.id);
    }
}

void HarmonyNoteEditor::deleteSelectedNotesFromEditor()
{
    if (selectedIds.empty())
        return; // still consume Delete in keyPressed even when empty

    processor.noteModel.removeNotes(selectedIds, &processor.noteModel.getUndoManager());
    clearSelection();
    dragMode = DragMode::none;
    dragId = {};
    tooltipText.clear();
    repaint();
}

static juce::Rectangle<float> normalisedRect(juce::Rectangle<float> box)
{
    if (box.getWidth() < 0.0f)
    {
        box.setX(box.getRight());
        box.setWidth(std::abs(box.getWidth()));
    }
    if (box.getHeight() < 0.0f)
    {
        box.setY(box.getBottom());
        box.setHeight(std::abs(box.getHeight()));
    }
    return box;
}

void HarmonyNoteEditor::applyMarqueeSelection(bool additive)
{
    if (! additive)
        clearSelection();

    const auto box = normalisedRect(marqueeRect);
    for (const auto& n : processor.noteModel.getNotes())
    {
        const bool hit = noteBounds(n, true).intersects(box) || noteBounds(n, false).intersects(box);
        if (hit)
            addToSelection(n.id);
    }
}

void HarmonyNoteEditor::beginMarquee(juce::Point<float> origin, bool additive)
{
    dragMode = DragMode::marquee;
    shiftMarqueeAdditive = additive;
    marqueeOrigin = origin;
    marqueeRect = juce::Rectangle<float>(origin, origin);
    pendingEmptyGesture = false;
    dragId = {};
    pitchDragNotes.clear();
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

void HarmonyNoteEditor::capturePitchDragSnapshot()
{
    pitchDragNotes.clear();
    for (const auto& id : selectedIds)
    {
        auto* note = processor.noteModel.findNote(id);
        if (note == nullptr)
            continue;
        pitchDragNotes.push_back({ id, note->editedHarmonyMidi(), note->manualOffsetSemitones });
    }

    // Fallback: primary drag note only (should already be in selection).
    if (pitchDragNotes.empty() && ! dragId.isEmpty())
    {
        if (auto* note = processor.noteModel.findNote(dragId))
            pitchDragNotes.push_back({ dragId, note->editedHarmonyMidi(), note->manualOffsetSemitones });
    }
}

void HarmonyNoteEditor::applyPitchDragToSnapshot(float primaryProposedMidi)
{
    if (pitchDragNotes.empty())
        return;

    float primaryStart = dragStartMidi;
    for (const auto& snap : pitchDragNotes)
    {
        if (snap.id == dragId)
        {
            primaryStart = snap.startEditedMidi;
            break;
        }
    }
    const float deltaMidi = primaryProposedMidi - primaryStart;

    for (const auto& snap : pitchDragNotes)
    {
        auto* note = processor.noteModel.findNote(snap.id);
        if (note == nullptr)
            continue;
        const float newAbs = snap.startEditedMidi + deltaMidi;
        note->manualOffsetSemitones = newAbs - note->autoHarmonyMidi;
    }
    processor.noteModel.republishOffsets();
}

void HarmonyNoteEditor::refreshContentRangeFromNotes()
{
    const auto& notes = processor.noteModel.getNotes();
    if (notes.empty())
    {
        // Avoid clamp churn every timer tick when nothing changed.
        if (lastFittedNoteCount != 0
            || std::abs(timelineView.contentStartSec) > 1.0e-9
            || std::abs(timelineView.contentEndSec - 8.0) > 1.0e-9)
        {
            timelineView.setContentRange(0.0, 8.0);
            pitchView.setContentMidiRange(48.0f, 84.0f);
        }
        const bool identityChanged = lastFittedNoteCount != 0;
        lastFittedNoteCount = 0;
        lastFittedContentStart = 0.0;
        lastFittedContentEnd = 8.0;
        lastFittedMidiMin = 48.0f;
        lastFittedMidiMax = 84.0f;
        if (identityChanged)
        {
            userNavigatedTimeline = false;
            userNavigatedPitch = false;
            fitViewsToNewNotesIfNeeded(true);
        }
        return;
    }

    double start = notes.front().startSec;
    double end = notes.front().startSec + notes.front().durationSec;
    float midiMin = juce::jmin(notes.front().voiceMidi, notes.front().editedHarmonyMidi());
    float midiMax = juce::jmax(notes.front().voiceMidi, notes.front().editedHarmonyMidi());
    for (const auto& n : notes)
    {
        start = juce::jmin(start, n.startSec);
        end = juce::jmax(end, n.startSec + n.durationSec);
        midiMin = juce::jmin(midiMin, n.voiceMidi, n.editedHarmonyMidi());
        midiMax = juce::jmax(midiMax, n.voiceMidi, n.editedHarmonyMidi());
    }
    start = juce::jmax(0.0, start - 0.05);
    end = end + 0.25;

    // Only ANALYZE / note-set timing changes reset the camera.
    // Pitch edits change midiMin/Max every drag frame — never treat that as a new song.
    const bool noteSetChanged = notes.size() != lastFittedNoteCount
                                || std::abs(start - lastFittedContentStart) > 1.0e-3
                                || std::abs(end - lastFittedContentEnd) > 1.0e-3;

    const bool timeRangeChanged = std::abs(start - timelineView.contentStartSec) > 1.0e-6
                                  || std::abs(end - timelineView.contentEndSec) > 1.0e-6;
    const bool midiRangeChanged = std::abs(midiMin - pitchView.contentMinMidi) > 0.02f
                                  || std::abs(midiMax - pitchView.contentMaxMidi) > 0.02f;

    if (timeRangeChanged)
        timelineView.setContentRange(start, end);
    if (midiRangeChanged)
        pitchView.setContentMidiRange(midiMin, midiMax);

    if (noteSetChanged)
    {
        lastFittedNoteCount = notes.size();
        lastFittedContentStart = start;
        lastFittedContentEnd = end;
        lastFittedMidiMin = midiMin;
        lastFittedMidiMax = midiMax;
        userNavigatedTimeline = false;
        userNavigatedPitch = false;
        followPlayhead = true;
        fitViewsToNewNotesIfNeeded(true);
    }
}

void HarmonyNoteEditor::fitViewsToNewNotesIfNeeded(bool notesIdentityChanged)
{
    if (! notesIdentityChanged)
        return;

    if (! userNavigatedTimeline)
    {
        // First page (~8 s), not the whole song — otherwise pan/follow cannot work.
        timelineView.viewStartSec = timelineView.contentStartSec;
        timelineView.viewDurationSec = juce::jmin(8.0, timelineView.maxVisibleSec());
        timelineView.clampView();
    }

    if (! userNavigatedPitch)
        pitchView.fitContent(4.0f);
}

void HarmonyNoteEditor::followPlayheadPage(double playSec)
{
    const double viewStart = timelineView.viewStartSec;
    const double viewDur = juce::jmax(1.0e-6, timelineView.viewDurationSec);
    const double viewEnd = viewStart + viewDur;
    const double rightMargin = viewDur * 0.08; // turn page near the right edge
    const double leftLead = viewDur * 0.08;    // after page turn, playhead sits near the left

    if (playSec > viewEnd - rightMargin)
    {
        // Page forward so the cursor keeps walking through the material.
        timelineView.viewStartSec = playSec - leftLead;
        timelineView.clampView();
    }
    else if (playSec < viewStart)
    {
        timelineView.viewStartSec = playSec - leftLead;
        timelineView.clampView();
    }
}

void HarmonyNoteEditor::showZoomHud()
{
    zoomHudUntilMs = juce::Time::getMillisecondCounter() + 700;
}

void HarmonyNoteEditor::zoomTimelineAtMouse(float deltaY, float mouseX)
{
    const auto lane = getTimelineLaneBounds();
    if (lane.getWidth() <= 1.0f)
        return;
    const double fraction = juce::jlimit(0.0, 1.0, static_cast<double>((mouseX - lane.getX()) / lane.getWidth()));
    timelineView.zoomAtFraction(deltaY, fraction);
    userNavigatedTimeline = true;
    followPlayhead = false;
    showZoomHud();
}

void HarmonyNoteEditor::zoomTimelineIn()
{
    const auto lane = getTimelineLaneBounds();
    zoomTimelineAtMouse(0.45f, lane.getCentreX());
    repaint();
}

void HarmonyNoteEditor::zoomTimelineOut()
{
    const auto lane = getTimelineLaneBounds();
    zoomTimelineAtMouse(-0.45f, lane.getCentreX());
    repaint();
}

void HarmonyNoteEditor::zoomPitchAtMouse(float deltaY, float mouseY)
{
    const auto lane = layout.notesUnion();
    if (lane.getHeight() <= 1.0f)
        return;
    auto r = lane.withTrimmedTop(5.0f).withTrimmedBottom(2.0f);
    const float frac = juce::jlimit(0.0f, 1.0f, (mouseY - r.getY()) / juce::jmax(1.0f, r.getHeight()));
    pitchView.zoomAtFraction(deltaY, frac);
    userNavigatedPitch = true;
    showZoomHud();
}

void HarmonyNoteEditor::panTimelineFromWheel(float deltaY)
{
    timelineView.panFromWheel(deltaY);
    userNavigatedTimeline = true;
    followPlayhead = false;
}

void HarmonyNoteEditor::panPitchFromWheel(float deltaY)
{
    pitchView.panFromWheel(deltaY);
    userNavigatedPitch = true;
}

void HarmonyNoteEditor::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (! layout.notesUnion().contains(event.position))
    {
        juce::Component::mouseWheelMove(event, wheel);
        return;
    }

    // macOS Shift+scroll often remaps into deltaX; prefer the dominant axis.
    float delta = wheel.deltaY;
    if (std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
        delta = wheel.deltaX;
    if (! std::isfinite(delta) || std::abs(delta) < 0.008f)
        return;

    // Inertial coasting after lift makes zoom/pan feel like it fights the user.
    if (wheel.isInertial)
        return;

    // Cmd/Ctrl + scroll = pitch zoom (zoom into low/high notes).
    // Shift + scroll = pitch pan.
    // Alt/Option + scroll = time pan.
    // Plain scroll = time zoom.
    if (event.mods.isCommandDown() || event.mods.isCtrlDown())
        zoomPitchAtMouse(delta, event.position.y);
    else if (event.mods.isShiftDown())
        panPitchFromWheel(delta);
    else if (event.mods.isAltDown())
        panTimelineFromWheel(delta);
    else
        zoomTimelineAtMouse(delta, event.position.x);

    repaint();
}

void HarmonyNoteEditor::paint(juce::Graphics& g)
{
    layout = computeLayout(getLocalBounds().toFloat());

    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff080c11));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colour(0xff2a333e));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    // Fixed lane headers — never share vertical space with note blocks.
    g.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff29e1f2));
    g.drawFittedText("VOICE",
                     layout.voiceHeader.toNearestInt().withTrimmedLeft(2),
                     juce::Justification::centredLeft, 1);
    g.setColour(juce::Colour(0xffa34bf2));
    g.drawFittedText("HARMONY",
                     layout.harmonyHeader.toNearestInt().withTrimmedLeft(2),
                     juce::Justification::centredLeft, 1);

    // Discrete horizontal divider between VOICE and HARMONY.
    g.setColour(juce::Colour(0xff1d2630));
    g.fillRect(layout.divider);

    // Semitone / pitch grid behind note blocks (meio tom when the lane is tall enough).
    drawPitchGrid(g, false);
    drawPitchGrid(g, true);

    const auto notesArea = layout.notesUnion();
    if (processor.noteModel.getNotes().empty())
    {
        g.setColour(juce::Colour(0xff8b949e));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawFittedText("Press ANALYZE, play the DAW, then stop - purple HARMONY blocks become editable",
                         notesArea.reduced(24.0f, 4.0f).toNearestInt(), juce::Justification::centred, 2);
    }

    for (const auto& note : processor.noteModel.getNotes())
    {
        const bool selected = isSelected(note.id);

        auto voice = noteBounds(note, false);
        if (! voice.isEmpty())
        {
            g.setColour(juce::Colour(0xff29e1f2).withAlpha(selected ? 0.75f : 0.55f));
            g.fillRoundedRectangle(voice, 3.0f);
            if (selected)
            {
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.drawRoundedRectangle(voice, 3.0f, 1.2f);
            }
        }

        auto harm = noteBounds(note, true);
        if (! harm.isEmpty())
        {
            g.setColour(selected ? juce::Colour(0xffc56bff) : juce::Colour(0xffa34bf2).withAlpha(note.isEdited() ? 0.95f : 0.7f));
            g.fillRoundedRectangle(harm, 3.0f);
            if (selected)
            {
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.drawRoundedRectangle(harm, 3.0f, 1.2f);
            }
        }
    }

    if (dragMode == DragMode::marquee)
    {
        const auto box = normalisedRect(marqueeRect);
        g.setColour(juce::Colour(0x552fe0ee));
        g.fillRect(box);
        g.setColour(juce::Colour(0xcc2fe0ee));
        g.drawRect(box, 1.0f);
    }

    const double play = getDisplayPlayheadSeconds();
    const auto dur = juce::jmax(1.0e-9, timelineView.viewDurationSec);
    const float px = notesArea.getX()
                     + static_cast<float>((play - timelineView.viewStartSec) / dur) * notesArea.getWidth();
    if (px >= notesArea.getX() && px <= notesArea.getRight())
    {
        const bool frozen = ! processor.isHostPlaying();
        g.setColour(juce::Colour(0xffedf1f4).withAlpha(frozen ? 1.0f : 0.8f));
        g.drawLine(px, notesArea.getY(), px, notesArea.getBottom(), 1.6f);

        // Persistent cursor marker (spec): small cyan triangle at the top.
        g.setColour(juce::Colour::fromRGB(40, 224, 238));
        juce::Path triangle;
        triangle.addTriangle(px - 4.0f, notesArea.getY(),
                             px + 4.0f, notesArea.getY(),
                             px, notesArea.getY() + 6.0f);
        g.fillPath(triangle);
    }

    if (tooltipText.isNotEmpty()
        && (dragMode == DragMode::pitchEdit || ! selectedIds.empty()))
    {
        const auto font = juce::Font(juce::FontOptions(20.0f, juce::Font::bold));
        g.setFont(font);
        const float textW = juce::jmax(72.0f, juce::GlyphArrangement::getStringWidth(font, tooltipText) + 24.0f);
        const float textH = 30.0f;
        const float tipX = bounds.getCentreX() - textW * 0.5f;
        const float tipY = layout.toolbar.getBottom() + 2.0f;
        g.setColour(juce::Colour(0xee11161f));
        g.fillRoundedRectangle(tipX, tipY, textW, textH, 6.0f);
        g.setColour(juce::Colours::white);
        g.drawFittedText(tooltipText,
                         juce::roundToInt(tipX),
                         juce::roundToInt(tipY),
                         juce::roundToInt(textW),
                         juce::roundToInt(textH),
                         juce::Justification::centred, 1);
    }

    if (juce::Time::getMillisecondCounter() < zoomHudUntilMs)
    {
        const int pct = juce::roundToInt(timelineView.zoomPercent());
        const juce::String hud = juce::String(pct) + "%";
        g.setColour(juce::Colour(0xaa0b1016));
        g.fillRoundedRectangle(bounds.getRight() - 64.0f, bounds.getBottom() - 22.0f, 54.0f, 16.0f, 3.0f);
        g.setColour(juce::Colour(0xffc9d0d8));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawFittedText(hud, juce::roundToInt(bounds.getRight() - 64.0f), juce::roundToInt(bounds.getBottom() - 22.0f),
                         54, 16, juce::Justification::centred, 1);
    }
}

void HarmonyNoteEditor::timerCallback()
{
    // Don't fight the camera while the user is actively dragging the view/notes.
    if (dragMode != DragMode::pan && dragMode != DragMode::pitchPan
        && dragMode != DragMode::pitchEdit)
        refreshContentRangeFromNotes();

    // Drop selection ids that no longer exist (after delete / ANALYZE rebuild).
    selectedIds.erase(std::remove_if(selectedIds.begin(), selectedIds.end(),
                                     [this](const juce::String& id) { return processor.noteModel.findNote(id) == nullptr; }),
                      selectedIds.end());

    const double pending = processor.takePendingEditorCursorSeconds();
    if (pending >= 0.0)
        restoreCursorFromState(pending);

    updateCursorFromTransport();
}

int64_t HarmonyNoteEditor::getAnalysisLengthSamples() const noexcept
{
    const auto transport = processor.getTransportSnapshot();
    const double sr = juce::jmax(1.0, transport.sampleRate);
    const double endSec = juce::jmax(timelineView.contentEndSec, timelineView.contentStartSec + 0.25);
    const int64_t endSample = static_cast<int64_t>(std::llround(endSec * sr));
    const int64_t relativeEnd = endSample - transport.analysisStartHostSample;
    return juce::jmax<int64_t>(1, relativeEnd);
}

void HarmonyNoteEditor::updateCursorFromTransport()
{
    const auto transport = processor.getTransportSnapshot();
    const int64_t analysisLength = getAnalysisLengthSamples();

    if (transport.isPlaying)
    {
        if (transport.hasValidHostPosition)
        {
            const int64_t relative = transport.currentHostSample
                                   - transport.analysisStartHostSample;
            editorCursorSample = juce::jlimit<int64_t>(0, analysisLength, relative);
            lastDisplayedPlayingSample = editorCursorSample;
        }

        if (! wasPlaying)
            followPlayhead = true; // re-enable page-follow on each transport start

        if (followPlayhead)
            followPlayheadPage(getDisplayPlayheadSeconds());
    }
    else if (wasPlaying)
    {
        // PLAYING → STOPPED: freeze last drawn point; ignore host return to zero.
        editorCursorSample = juce::jlimit<int64_t>(0, analysisLength, lastDisplayedPlayingSample);
    }

    wasPlaying = transport.isPlaying;
    processor.setEditorCursorSecondsForState(getCursorSecondsForState());
    repaint();
}

void HarmonyNoteEditor::resetCursorForNewAnalysis() noexcept
{
    editorCursorSample = 0;
    lastDisplayedPlayingSample = 0;
    wasPlaying = false;
    followPlayhead = true;
    processor.setEditorCursorSecondsForState(0.0);
    repaint();
}

void HarmonyNoteEditor::restoreCursorFromState(double seconds) noexcept
{
    const auto transport = processor.getTransportSnapshot();
    const double sr = juce::jmax(1.0, transport.sampleRate);
    const int64_t absolute = static_cast<int64_t>(std::llround(seconds * sr));
    const int64_t relative = absolute - transport.analysisStartHostSample;
    editorCursorSample = juce::jlimit<int64_t>(0, getAnalysisLengthSamples(), relative);
    lastDisplayedPlayingSample = editorCursorSample;
    processor.setEditorCursorSecondsForState(getCursorSecondsForState());
    repaint();
}

double HarmonyNoteEditor::getCursorSecondsForState() const noexcept
{
    const auto transport = processor.getTransportSnapshot();
    const double sr = juce::jmax(1.0, transport.sampleRate);
    return static_cast<double>(transport.analysisStartHostSample + editorCursorSample) / sr;
}

void HarmonyNoteEditor::setPlayheadFromX(float mouseX)
{
    const double t = timeAtMouseX(mouseX);
    const auto transport = processor.getTransportSnapshot();
    const double sr = juce::jmax(1.0, transport.sampleRate);
    const int64_t absolute = static_cast<int64_t>(std::llround(t * sr));
    const int64_t relative = absolute - transport.analysisStartHostSample;
    editorCursorSample = juce::jlimit<int64_t>(0, getAnalysisLengthSamples(), relative);
    lastDisplayedPlayingSample = editorCursorSample;
    processor.setEditorCursorSecondsForState(getCursorSecondsForState());
    userNavigatedTimeline = true;
    if (! processor.isHostPlaying())
        followPlayheadPage(getDisplayPlayheadSeconds());
}

double HarmonyNoteEditor::getDisplayPlayheadSeconds() const
{
    return getCursorSecondsForState();
}

namespace
{
/** Closed / grabbing fist — JUCE only exposes openHand via DraggingHandCursor on macOS. */
juce::MouseCursor makeClosedHandCursor()
{
    juce::Image img(juce::Image::ARGB, 32, 32, true);
    {
        juce::Graphics g(img);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.addTransform(juce::AffineTransform::scale(2.0f));

        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillEllipse(3.5f, 8.5f, 10.0f, 7.0f);

        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(3.0f, 6.5f, 10.0f, 7.5f, 2.2f);
        g.fillRoundedRectangle(1.5f, 8.0f, 3.2f, 5.0f, 1.4f);
        for (int i = 0; i < 4; ++i)
            g.fillRoundedRectangle(3.5f + static_cast<float>(i) * 2.2f, 5.0f, 1.9f, 3.0f, 0.8f);
    }
    return juce::MouseCursor(juce::ScaledImage(img, 2.0), { 8, 8 });
}
}

void HarmonyNoteEditor::beginTimelinePan(float mouseX)
{
    dragMode = DragMode::pan;
    panDragStartX = mouseX;
    panDragStartVisibleTime = timelineView.viewStartSec;
    userNavigatedTimeline = true;
    followPlayhead = false;
    pendingEmptyGesture = false;
}

void HarmonyNoteEditor::beginPitchPan(float mouseY)
{
    dragMode = DragMode::pitchPan;
    panDragStartY = mouseY;
    panDragStartTopMidi = pitchView.viewTopMidi;
    userNavigatedPitch = true;
    pendingEmptyGesture = false;
}

double HarmonyNoteEditor::timeAtMouseX(float mouseX) const
{
    const auto lane = layout.notesUnion();
    const double width = juce::jmax(1.0, static_cast<double>(lane.getWidth()));
    const double frac = juce::jlimit(0.0, 1.0, static_cast<double>((mouseX - lane.getX()) / width));
    return timelineView.timeAtFraction(frac);
}

bool HarmonyNoteEditor::isNearPlayhead(float mouseX) const
{
    const auto lane = layout.notesUnion();
    if (lane.getWidth() <= 1.0f)
        return false;
    const double play = getDisplayPlayheadSeconds();
    const double dur = juce::jmax(1.0e-9, timelineView.viewDurationSec);
    const float px = lane.getX()
                     + static_cast<float>((play - timelineView.viewStartSec) / dur) * lane.getWidth();
    return std::abs(mouseX - px) <= 5.0f;
}

void HarmonyNoteEditor::updateTimelineCursor(juce::Point<float> pos, bool /*altDown*/, bool dragging)
{
    const bool overNotes = layout.notesUnion().contains(pos);
    const bool panning = dragging || dragMode == DragMode::pan || dragMode == DragMode::pitchPan;
    const bool emptyForPan = overNotes && hitTestNote(pos) < 0;

    if (overNotes && (panning || emptyForPan))
    {
        static const juce::MouseCursor closedHand = makeClosedHandCursor();
        setMouseCursor(panning ? closedHand : juce::MouseCursor::DraggingHandCursor);
        return;
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void HarmonyNoteEditor::mouseMove(const juce::MouseEvent& e)
{
    updateTimelineCursor(e.position, e.mods.isAltDown(),
                          dragMode == DragMode::pan || dragMode == DragMode::pitchPan);
}

void HarmonyNoteEditor::mouseEnter(const juce::MouseEvent& e)
{
    updateTimelineCursor(e.position, e.mods.isAltDown(),
                          dragMode == DragMode::pan || dragMode == DragMode::pitchPan);
}

void HarmonyNoteEditor::mouseExit(const juce::MouseEvent&)
{
    if (dragMode != DragMode::pan && dragMode != DragMode::pitchPan)
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void HarmonyNoteEditor::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    pendingEmptyGesture = false;

    if (layout.toolbar.contains(e.position))
    {
        if (! e.mods.isShiftDown())
            clearSelection();
        dragMode = DragMode::none;
        repaint();
        return;
    }

    // Click / drag on lane headers places (and can drag) the playhead cursor.
    if (layout.voiceHeader.contains(e.position)
        || layout.harmonyHeader.contains(e.position)
        || layout.divider.contains(e.position))
    {
        if (! e.mods.isShiftDown())
            clearSelection();
        setPlayheadFromX(e.position.x);
        dragMode = DragMode::scrub;
        repaint();
        return;
    }

    if (! layout.notesUnion().contains(e.position))
    {
        clearSelection();
        repaint();
        return;
    }

    const auto mods = e.mods;
    const bool altHeld = mods.isAltDown()
                         || juce::ModifierKeys::getCurrentModifiersRealtime().isAltDown();
    const bool middle = mods.isMiddleButtonDown();
    const bool shift = mods.isShiftDown();

    // Middle-click → time pan.
    if (middle)
    {
        beginTimelinePan(e.position.x);
        updateTimelineCursor(e.position, false, true);
        repaint();
        return;
    }

    // Option/Alt + left drag = marquee multi-select ("lençol").
    // Shift+Option keeps previous selection (additive).
    if (altHeld)
    {
        beginMarquee(e.position, shift);
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
        repaint();
        return;
    }

    // Drag near the playhead line to reposition it.
    if (! shift && isNearPlayhead(e.position.x) && hitTestNote(e.position) < 0)
    {
        setPlayheadFromX(e.position.x);
        dragMode = DragMode::scrub;
        repaint();
        return;
    }

    const int hit = hitTestNote(e.position);

    if (hit >= 0)
    {
        const auto& note = processor.noteModel.getNotes()[static_cast<size_t>(hit)];
        if (shift)
        {
            toggleSelection(note.id);
            dragMode = DragMode::none;
            dragId = {};
            pitchDragNotes.clear();
            repaint();
            return;
        }

        // Clicking an unselected note selects only it; clicking inside a multi-selection keeps all.
        if (! isSelected(note.id))
            selectOnly(note.id);

        dragMode = DragMode::pitchEdit;
        dragId = note.id;
        dragStartOffset = note.manualOffsetSemitones;
        dragStartMidi = note.editedHarmonyMidi();
        dragStartY = e.position.y;
        fineDrag = false;
        capturePitchDragSnapshot();
        updateTooltip(0.0f);
        repaint();
        return;
    }

    // Shift + empty drag = additive marquee.
    if (shift)
    {
        beginMarquee(e.position, true);
        repaint();
        return;
    }

    // Empty click → place playhead; empty drag → pan time (horizontal) or pitch (vertical).
    clearSelection();
    pitchDragNotes.clear();
    pendingEmptyGesture = true;
    emptyGestureOrigin = e.position;
    emptyGestureStartX = e.position.x;
    emptyGestureStartY = e.position.y;
    emptyGestureStartView = timelineView.viewStartSec;
    emptyGestureStartTopMidi = pitchView.viewTopMidi;
    dragMode = DragMode::none;
    repaint();
}

void HarmonyNoteEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (pendingEmptyGesture)
    {
        const float dx = e.position.x - emptyGestureOrigin.x;
        const float dy = e.position.y - emptyGestureOrigin.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 5.0f)
        {
            pendingEmptyGesture = false;
            if (std::abs(dy) > std::abs(dx))
            {
                beginPitchPan(emptyGestureStartY);
            }
            else
            {
                beginTimelinePan(emptyGestureStartX);
            }
        }
        else
        {
            return;
        }
    }

    if (dragMode == DragMode::scrub)
    {
        setPlayheadFromX(e.position.x);
        repaint();
        return;
    }

    if (dragMode == DragMode::pitchPan)
    {
        const float laneH = juce::jmax(1.0f, layout.notesUnion().getHeight());
        const float dragDistanceY = e.position.y - panDragStartY;
        pitchView.panByPixelDrag(panDragStartTopMidi, dragDistanceY, laneH);
        userNavigatedPitch = true;
        updateTimelineCursor(e.position, false, true);
        repaint();
        return;
    }

    if (dragMode == DragMode::pan)
    {
        const double timelineWidth = juce::jmax(1.0, static_cast<double>(layout.notesUnion().getWidth()));
        const double dragDistanceX = static_cast<double>(e.position.x - panDragStartX);
        timelineView.panByPixelDrag(panDragStartVisibleTime, dragDistanceX, timelineWidth);
        userNavigatedTimeline = true;
        updateTimelineCursor(e.position, true, true);
        repaint();
        return;
    }

    if (dragMode == DragMode::marquee)
    {
        marqueeRect = juce::Rectangle<float>(marqueeOrigin, e.position);
        repaint();
        return;
    }

    if (dragMode != DragMode::pitchEdit)
        return;

    fineDrag = e.mods.isShiftDown();
    const float deltaY = dragStartY - e.position.y;
    const auto ks = keyScale();
    float proposedAbsolute = dragStartMidi;
    if (fineDrag)
    {
        proposedAbsolute = dragStartMidi + deltaY * 0.02f;
        if (processor.noteModel.getSnapMode() != nf::notes::SnapMode::off)
            proposedAbsolute = processor.noteModel.quantizeAbsoluteMidi(proposedAbsolute, ks.first, ks.second);
        else
            proposedAbsolute = std::round(proposedAbsolute * 100.0f) / 100.0f;
    }
    else
    {
        const int steps = juce::roundToInt(deltaY / 10.0f);
        proposedAbsolute = dragStartMidi + static_cast<float>(steps);
        proposedAbsolute = processor.noteModel.quantizeAbsoluteMidi(proposedAbsolute, ks.first, ks.second);
    }

    applyPitchDragToSnapshot(proposedAbsolute);
    if (auto* note = processor.noteModel.findNote(dragId))
        tooltipText = formatOffsetTooltip(note->manualOffsetSemitones);
    repaint();
}

void HarmonyNoteEditor::mouseUp(const juce::MouseEvent& e)
{
    if (pendingEmptyGesture)
    {
        pendingEmptyGesture = false;
        setPlayheadFromX(e.position.x);
        dragMode = DragMode::none;
        updateTimelineCursor(e.position, e.mods.isAltDown(), false);
        repaint();
        return;
    }

    if (dragMode == DragMode::scrub)
    {
        setPlayheadFromX(e.position.x);
        dragMode = DragMode::none;
        repaint();
        return;
    }

    if (dragMode == DragMode::pan || dragMode == DragMode::pitchPan)
    {
        dragMode = DragMode::none;
        updateTimelineCursor(e.position, e.mods.isAltDown(), false);
        repaint();
        return;
    }

    if (dragMode == DragMode::marquee)
    {
        applyMarqueeSelection(shiftMarqueeAdditive);
        dragMode = DragMode::none;
        marqueeRect = {};
        repaint();
        return;
    }

    if (dragMode == DragMode::pitchEdit)
        commitPitchDrag(true);
}

void HarmonyNoteEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.mods.isAltDown())
        return;

    const int hit = hitTestNote(e.position);
    if (hit < 0)
        return;
    const auto id = processor.noteModel.getNotes()[static_cast<size_t>(hit)].id;
    processor.noteModel.resetManualOffset(id, &processor.noteModel.getUndoManager());
    selectOnly(id);
    dragMode = DragMode::none;
    dragId = {};
    tooltipText.clear();
    repaint();
}

void HarmonyNoteEditor::updateTooltip(float)
{
    if (auto* note = processor.noteModel.findNote(dragId))
        tooltipText = formatOffsetTooltip(note->manualOffsetSemitones);
}

void HarmonyNoteEditor::commitPitchDrag(bool apply)
{
    if (! pitchDragNotes.empty())
    {
        if (apply)
        {
            auto& um = processor.noteModel.getUndoManager();
            um.beginNewTransaction(pitchDragNotes.size() > 1 ? "Edit harmony notes" : "Edit harmony note");
            for (const auto& snap : pitchDragNotes)
            {
                auto* note = processor.noteModel.findNote(snap.id);
                if (note == nullptr)
                    continue;
                const float finalOffset = note->manualOffsetSemitones;
                note->manualOffsetSemitones = snap.startOffset;
                processor.noteModel.setManualOffset(snap.id, finalOffset, &um);
            }
        }
        else
        {
            for (const auto& snap : pitchDragNotes)
            {
                if (auto* note = processor.noteModel.findNote(snap.id))
                    note->manualOffsetSemitones = snap.startOffset;
            }
            processor.noteModel.republishOffsets();
        }
    }
    else if (! dragId.isEmpty())
    {
        if (auto* note = processor.noteModel.findNote(dragId))
        {
            if (apply)
            {
                const float finalOffset = note->manualOffsetSemitones;
                note->manualOffsetSemitones = dragStartOffset;
                processor.noteModel.setManualOffset(dragId, finalOffset, &processor.noteModel.getUndoManager());
            }
            else
            {
                note->manualOffsetSemitones = dragStartOffset;
                processor.noteModel.republishOffsets();
            }
        }
    }
    dragMode = DragMode::none;
    dragId = {};
    pitchDragNotes.clear();
    tooltipText.clear();
    repaint();
}

void HarmonyNoteEditor::nudgeSelectedPitch(int direction, bool fineCents)
{
    if (selectedIds.empty() || direction == 0)
        return;
    if (dragMode == DragMode::pitchEdit || dragMode == DragMode::marquee
        || dragMode == DragMode::pan || dragMode == DragMode::pitchPan
        || dragMode == DragMode::scrub)
        return;

    const auto ks = keyScale();
    bool changed = false;

    for (const auto& id : selectedIds)
    {
        auto* note = processor.noteModel.findNote(id);
        if (note == nullptr)
            continue;

        float proposed = note->editedHarmonyMidi();
        if (fineCents)
        {
            proposed += static_cast<float>(direction) * 0.01f;
            if (processor.noteModel.getSnapMode() != nf::notes::SnapMode::off)
                proposed = processor.noteModel.quantizeAbsoluteMidi(proposed, ks.first, ks.second);
            else
                proposed = std::round(proposed * 100.0f) / 100.0f;
        }
        else
        {
            proposed += static_cast<float>(direction);
            if (processor.noteModel.getSnapMode() != nf::notes::SnapMode::off)
                proposed = processor.noteModel.quantizeAbsoluteMidi(proposed, ks.first, ks.second);
            else
                proposed = std::round(proposed);
        }

        const float newOffset = juce::jlimit(-24.0f, 24.0f, proposed - note->autoHarmonyMidi);
        if (processor.noteModel.setManualOffset(id, newOffset, &processor.noteModel.getUndoManager()))
        {
            changed = true;
            tooltipText = formatOffsetTooltip(newOffset);
            dragId = id; // so updateTooltip path stays consistent if needed
        }
    }

    if (changed)
        repaint();
}

bool HarmonyNoteEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        // Do not consume Space — the DAW owns Play/Stop; timer freezes the cursor.
        return false;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        // Consume even with empty selection so Delete never reaches the DAW.
        deleteSelectedNotesFromEditor();
        return true;
    }

    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        const int dir = (key == juce::KeyPress::upKey) ? 1 : -1;
        const bool fine = key.getModifiers().isShiftDown();
        if (! selectedIds.empty())
        {
            nudgeSelectedPitch(dir, fine);
            return true;
        }
        return true; // consume so the DAW does not steal arrow keys while editor focused
    }

    if ((key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown())
        && ! key.getModifiers().isShiftDown()
        && (key.getKeyCode() == 'a' || key.getKeyCode() == 'A'))
    {
        selectAllVisibleNotes();
        repaint();
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (dragMode == DragMode::pitchEdit)
        {
            commitPitchDrag(false);
            return true;
        }
        if (dragMode == DragMode::marquee || dragMode == DragMode::pan
            || dragMode == DragMode::pitchPan || dragMode == DragMode::scrub)
        {
            dragMode = DragMode::none;
            pendingEmptyGesture = false;
            marqueeRect = {};
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
            return true;
        }
        clearSelection();
        tooltipText.clear();
        repaint();
        return true;
    }

    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier, 0))
    {
        processor.noteModel.getUndoManager().undo();
        repaint();
        return true;
    }
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress('y', juce::ModifierKeys::ctrlModifier, 0))
    {
        processor.noteModel.getUndoManager().redo();
        repaint();
        return true;
    }
    return false;
}
