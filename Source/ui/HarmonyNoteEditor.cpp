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
    startTimerHz(30);
}

HarmonyNoteEditor::~HarmonyNoteEditor()
{
    stopTimer();
}

void HarmonyNoteEditor::resized()
{
    layout = computeLayout(getLocalBounds().toFloat());
    // SNAP reserved in the top toolbar only — identical margins to top/right edges.
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
        if (detected.confidence > 0.04f)
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
    const float frac = juce::jlimit(0.0f, 1.0f, (84.0f - midi) / 36.0f);
    return r.getY() + frac * juce::jmax(1.0f, r.getHeight());
}

float HarmonyNoteEditor::yToMidi(float y, bool harmonyLane) const
{
    auto r = harmonyLane ? layout.harmonyNotes : layout.voiceNotes;
    r = r.withTrimmedTop(5.0f).withTrimmedBottom(2.0f);
    const float frac = juce::jlimit(0.0f, 1.0f, (y - r.getY()) / juce::jmax(1.0f, r.getHeight()));
    return 84.0f - frac * 36.0f;
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
    // Clamp vertically so blocks never enter the header strip.
    bounds = bounds.getIntersection(lane);
    if (bounds.isEmpty())
        bounds = juce::Rectangle<float>(x0, lane.getCentreY() - noteH * 0.5f, juce::jmax(4.0f, x1 - x0), noteH)
                     .getIntersection(lane);
    return bounds;
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

void HarmonyNoteEditor::refreshContentRangeFromNotes()
{
    const auto& notes = processor.noteModel.getNotes();
    if (notes.empty())
    {
        timelineView.setContentRange(0.0, 8.0);
        return;
    }

    double start = notes.front().startSec;
    double end = notes.front().startSec + notes.front().durationSec;
    for (const auto& n : notes)
    {
        start = juce::jmin(start, n.startSec);
        end = juce::jmax(end, n.startSec + n.durationSec);
    }
    timelineView.setContentRange(juce::jmax(0.0, start - 0.05), end + 0.25);

    if (! userNavigatedTimeline)
    {
        timelineView.viewStartSec = timelineView.contentStartSec;
        timelineView.viewDurationSec = timelineView.maxVisibleSec();
        timelineView.clampView();
    }
}

void HarmonyNoteEditor::showZoomHud()
{
    zoomHudUntilMs = juce::Time::getMillisecondCounter() + 500;
}

void HarmonyNoteEditor::zoomTimelineAtMouse(float deltaY, float mouseX)
{
    const auto lane = getTimelineLaneBounds();
    if (lane.getWidth() <= 1.0f)
        return;
    const double fraction = juce::jlimit(0.0, 1.0, static_cast<double>((mouseX - lane.getX()) / lane.getWidth()));
    timelineView.zoomAtFraction(deltaY, fraction);
    userNavigatedTimeline = true;
    showZoomHud();
}

void HarmonyNoteEditor::panTimelineFromWheel(float deltaY)
{
    timelineView.panFromWheel(deltaY);
    userNavigatedTimeline = true;
}

void HarmonyNoteEditor::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (! layout.notesUnion().contains(event.position))
    {
        juce::Component::mouseWheelMove(event, wheel);
        return;
    }

    if (event.mods.isAltDown())
        panTimelineFromWheel(wheel.deltaY);
    else
        zoomTimelineAtMouse(wheel.deltaY, event.position.x);

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
        g.setColour(juce::Colour(0xff29e1f2).withAlpha(selected ? 0.75f : 0.55f));
        g.fillRoundedRectangle(voice, 3.0f);
        if (selected)
        {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.drawRoundedRectangle(voice, 3.0f, 1.2f);
        }

        auto harm = noteBounds(note, true);
        g.setColour(selected ? juce::Colour(0xffc56bff) : juce::Colour(0xffa34bf2).withAlpha(note.isEdited() ? 0.95f : 0.7f));
        g.fillRoundedRectangle(harm, 3.0f);
        if (selected)
        {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.drawRoundedRectangle(harm, 3.0f, 1.2f);
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

    const double play = processor.getHostTimeSeconds();
    const auto dur = juce::jmax(1.0e-9, timelineView.viewDurationSec);
    const float px = notesArea.getX()
                     + static_cast<float>((play - timelineView.viewStartSec) / dur) * notesArea.getWidth();
    if (px >= notesArea.getX() && px <= notesArea.getRight())
    {
        g.setColour(juce::Colour(0xffedf1f4).withAlpha(0.8f));
        g.drawLine(px, notesArea.getY(), px, notesArea.getBottom(), 1.2f);
    }

    if (dragMode == DragMode::pitchEdit && tooltipText.isNotEmpty())
    {
        g.setColour(juce::Colour(0xee11161f));
        g.fillRoundedRectangle(bounds.getCentreX() - 50.0f, layout.toolbar.getBottom() + 2.0f, 100.0f, 18.0f, 4.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawFittedText(tooltipText,
                         juce::roundToInt(bounds.getCentreX() - 50.0f),
                         juce::roundToInt(layout.toolbar.getBottom() + 2.0f),
                         100, 18, juce::Justification::centred, 1);
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
    refreshContentRangeFromNotes();

    // Drop selection ids that no longer exist (after delete / ANALYZE rebuild).
    selectedIds.erase(std::remove_if(selectedIds.begin(), selectedIds.end(),
                                     [this](const juce::String& id) { return processor.noteModel.findNote(id) == nullptr; }),
                      selectedIds.end());

    const double play = processor.getHostTimeSeconds();
    if (processor.isHostPlaying())
    {
        if (play < timelineView.viewStartSec || play > timelineView.viewStartSec + timelineView.viewDurationSec * 0.92)
        {
            timelineView.viewStartSec = play - timelineView.viewDurationSec * 0.15;
            timelineView.clampView();
        }
    }

    repaint();
}

void HarmonyNoteEditor::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    // Clicks on toolbar / headers clear selection but do not start marquee or pitch edit.
    if (layout.toolbar.contains(e.position)
        || layout.voiceHeader.contains(e.position)
        || layout.harmonyHeader.contains(e.position)
        || layout.divider.contains(e.position))
    {
        if (! e.mods.isShiftDown())
            clearSelection();
        dragMode = DragMode::none;
        repaint();
        return;
    }

    if (! layout.notesUnion().contains(e.position))
    {
        clearSelection();
        repaint();
        return;
    }

    const int hit = hitTestNote(e.position);
    const bool shift = e.mods.isShiftDown();

    if (hit >= 0)
    {
        const auto& note = processor.noteModel.getNotes()[static_cast<size_t>(hit)];
        if (shift)
        {
            // Shift+click toggles membership; do not start pitch edit.
            toggleSelection(note.id);
            dragMode = DragMode::none;
            dragId = {};
            repaint();
            return;
        }

        if (! isSelected(note.id) || selectedIds.size() != 1)
            selectOnly(note.id);

        // Drag starting on a note keeps vertical pitch edit.
        dragMode = DragMode::pitchEdit;
        dragId = note.id;
        dragStartOffset = note.manualOffsetSemitones;
        dragStartMidi = note.editedHarmonyMidi();
        dragStartY = e.position.y;
        fineDrag = false;
        updateTooltip(0.0f);
        repaint();
        return;
    }

    // Empty notes area: clear or prepare additive marquee.
    if (! shift)
        clearSelection();
    dragMode = DragMode::marquee;
    shiftMarqueeAdditive = shift;
    marqueeOrigin = e.position;
    marqueeRect = juce::Rectangle<float>(e.position, e.position);
    dragId = {};
    repaint();
}

void HarmonyNoteEditor::mouseDrag(const juce::MouseEvent& e)
{
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
    auto* note = processor.noteModel.findNote(dragId);
    if (note == nullptr)
        return;

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

    const float newOffset = proposedAbsolute - note->autoHarmonyMidi;
    note->manualOffsetSemitones = juce::jlimit(-24.0f, 24.0f, newOffset);
    processor.noteModel.republishOffsets();
    updateTooltip(note->manualOffsetSemitones - dragStartOffset);
    repaint();
}

void HarmonyNoteEditor::mouseUp(const juce::MouseEvent&)
{
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
    if (! dragId.isEmpty())
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
    tooltipText.clear();
    repaint();
}

bool HarmonyNoteEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        // Consume even with empty selection so Delete never reaches the DAW.
        deleteSelectedNotesFromEditor();
        return true;
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
        if (dragMode == DragMode::marquee)
        {
            dragMode = DragMode::none;
            marqueeRect = {};
            repaint();
            return true;
        }
        clearSelection();
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
