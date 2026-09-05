#pragma once

#include "HarmonyNoteTypes.h"
#include "MusicalScale.h"
#include <vector>

namespace nf::notes
{
class NoteEditModel
{
public:
    NoteEditModel();

    void clear();
    void setNotes(std::vector<HarmonyNote> notes);
    /** Append new analysis notes without replacing existing ones (skips time overlaps). */
    size_t appendNotes(std::vector<HarmonyNote> incoming);
    const std::vector<HarmonyNote>& getNotes() const noexcept { return notes; }
    HarmonyNote* findNote(const juce::String& id);
    const HarmonyNote* findNote(const juce::String& id) const;

    SnapMode getSnapMode() const noexcept { return snapMode; }
    void setSnapMode(SnapMode mode) noexcept { snapMode = mode; }

    float snapOffsetDelta(float currentAutoMidi, float proposedAbsoluteMidi,
                          int keyRoot, nf::dsp::ScaleType scale) const;
    float quantizeAbsoluteMidi(float midi, int keyRoot, nf::dsp::ScaleType scale) const;

    bool setManualOffset(const juce::String& id, float offsetSemitones, juce::UndoManager* undo);
    bool resetManualOffset(const juce::String& id, juce::UndoManager* undo);

    /** Flat pencil: constant offset across the note (clears pitch curve). */
    bool applyFlatPencil(const juce::String& id, float offsetSemitones, juce::UndoManager* undo);
    /** Slope pencil: straight line between two time/offset points. */
    bool applySlopePencil(const juce::String& id,
                          double t0, float offset0,
                          double t1, float offset1,
                          juce::UndoManager* undo);
    /** Scissors: split note at absolute time. Returns new right-hand note id, or {}. */
    juce::String splitNoteAt(const juce::String& id, double cutTimeSec, juce::UndoManager* undo);

    /** Remove notes from the edit map only (DSP falls back to auto harmony). One undo step. */
    bool removeNotes(const std::vector<juce::String>& ids, juce::UndoManager* undo);

    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& tree);

    OffsetTable& getOffsetTable() noexcept { return offsets; }
    const OffsetTable& getOffsetTable() const noexcept { return offsets; }
    void republishOffsets();

    juce::UndoManager& getUndoManager() noexcept { return undo; }

    class OffsetAction final : public juce::UndoableAction
    {
    public:
        OffsetAction(NoteEditModel& owner, juce::String noteId, float from, float to);
        bool perform() override;
        bool undo() override;
        int getSizeInUnits() override { return 1; }

    private:
        NoteEditModel& model;
        juce::String id;
        float previous = 0.0f, next = 0.0f;
    };

    class NoteMutateAction final : public juce::UndoableAction
    {
    public:
        NoteMutateAction(NoteEditModel& owner, HarmonyNote before, HarmonyNote after);
        bool perform() override;
        bool undo() override;
        int getSizeInUnits() override { return 1; }

    private:
        NoteEditModel& model;
        HarmonyNote previous, next;
    };

    class SplitNoteAction final : public juce::UndoableAction
    {
    public:
        SplitNoteAction(NoteEditModel& owner, HarmonyNote original, HarmonyNote left, HarmonyNote right);
        bool perform() override;
        bool undo() override;
        int getSizeInUnits() override { return 1; }

    private:
        NoteEditModel& model;
        HarmonyNote originalNote, leftNote, rightNote;
    };

    class DeleteNotesAction final : public juce::UndoableAction
    {
    public:
        DeleteNotesAction(NoteEditModel& owner, std::vector<HarmonyNote> removed);
        bool perform() override;
        bool undo() override;
        int getSizeInUnits() override { return 1; }

    private:
        NoteEditModel& model;
        std::vector<HarmonyNote> removedNotes;
    };

private:
    std::vector<HarmonyNote> notes;
    SnapMode snapMode = SnapMode::key;
    OffsetTable offsets;
    juce::UndoManager undo;
};

/** Pure geometry helper for marquee selection tests. */
inline bool noteIntersectsMarquee(double noteStart, double noteDur,
                                  float noteMidi, bool harmonyLane,
                                  double viewStart, double viewDur,
                                  juce::Rectangle<float> lane,
                                  juce::Rectangle<float> marquee,
                                  float midiTop = 84.0f, float midiSpan = 36.0f)
{
    if (viewDur <= 1.0e-12 || lane.getWidth() <= 1.0f)
        return false;
    const float x0 = lane.getX() + static_cast<float>((noteStart - viewStart) / viewDur) * lane.getWidth();
    const float x1 = lane.getX() + static_cast<float>((noteStart + noteDur - viewStart) / viewDur) * lane.getWidth();
    const float laneTop = harmonyLane ? lane.getCentreY() + 4.0f : lane.getY();
    const float laneH = lane.getHeight() * 0.42f;
    const float frac = juce::jlimit(0.0f, 1.0f, (midiTop - noteMidi) / midiSpan);
    const float y = laneTop + frac * laneH;
    const auto noteBounds = juce::Rectangle<float>(x0, y - 7.0f, juce::jmax(4.0f, x1 - x0), 14.0f);
    auto box = marquee;
    if (box.getWidth() < 0.0f)
    {
        box.setX(box.getRight());
        box.setWidth(std::abs(marquee.getWidth()));
    }
    if (box.getHeight() < 0.0f)
    {
        box.setY(box.getBottom());
        box.setHeight(std::abs(marquee.getHeight()));
    }
    return noteBounds.intersects(box);
}
}
