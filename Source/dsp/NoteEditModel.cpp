#include "NoteEditModel.h"
#include <cmath>
#include <algorithm>

namespace nf::notes
{
NoteEditModel::NoteEditModel() = default;

void NoteEditModel::clear()
{
    notes.clear();
    undo.clearUndoHistory();
    offsets.clear();
}

void NoteEditModel::setNotes(std::vector<HarmonyNote> next)
{
    notes = std::move(next);
    undo.clearUndoHistory();
    republishOffsets();
}

HarmonyNote* NoteEditModel::findNote(const juce::String& id)
{
    for (auto& n : notes)
        if (n.id == id)
            return &n;
    return nullptr;
}

const HarmonyNote* NoteEditModel::findNote(const juce::String& id) const
{
    for (const auto& n : notes)
        if (n.id == id)
            return &n;
    return nullptr;
}

float NoteEditModel::quantizeAbsoluteMidi(float midi, int keyRoot, nf::dsp::ScaleType scale) const
{
    switch (snapMode)
    {
        case SnapMode::off:
            return midi;
        case SnapMode::chromatic:
            return std::round(midi);
        case SnapMode::key:
        {
            float best = midi;
            float bestDist = 1.0e9f;
            const int base = juce::roundToInt(std::floor(midi)) - 2;
            for (int m = base; m <= base + 6; ++m)
            {
                if (! nf::dsp::MusicalScale::isInScale(m, keyRoot, scale))
                    continue;
                const float d = std::abs(static_cast<float>(m) - midi);
                if (d < bestDist)
                {
                    bestDist = d;
                    best = static_cast<float>(m);
                }
            }
            return best;
        }
    }
    return midi;
}

float NoteEditModel::snapOffsetDelta(float currentAutoMidi, float proposedAbsoluteMidi,
                                     int keyRoot, nf::dsp::ScaleType scale) const
{
    const auto snapped = quantizeAbsoluteMidi(proposedAbsoluteMidi, keyRoot, scale);
    return snapped - currentAutoMidi;
}

bool NoteEditModel::setManualOffset(const juce::String& id, float offsetSemitones, juce::UndoManager* um)
{
    auto* note = findNote(id);
    if (note == nullptr)
        return false;
    const float clamped = juce::jlimit(-24.0f, 24.0f, offsetSemitones);
    if (std::abs(clamped - note->manualOffsetSemitones) < 1.0e-6f)
        return false;
    if (um != nullptr)
    {
        um->beginNewTransaction("Edit harmony note");
        um->perform(new OffsetAction(*this, id, note->manualOffsetSemitones, clamped));
    }
    else
    {
        note->manualOffsetSemitones = clamped;
        republishOffsets();
    }
    return true;
}

bool NoteEditModel::resetManualOffset(const juce::String& id, juce::UndoManager* um)
{
    return setManualOffset(id, 0.0f, um);
}

bool NoteEditModel::removeNotes(const std::vector<juce::String>& ids, juce::UndoManager* um)
{
    std::vector<HarmonyNote> removed;
    for (const auto& id : ids)
        if (const auto* n = findNote(id))
            removed.push_back(*n);

    if (removed.empty())
        return false;

    if (um != nullptr)
    {
        um->beginNewTransaction("Delete harmony notes");
        return um->perform(new DeleteNotesAction(*this, std::move(removed)));
    }

    notes.erase(std::remove_if(notes.begin(), notes.end(),
                               [&](const HarmonyNote& n)
                               {
                                   return std::any_of(ids.begin(), ids.end(),
                                                      [&](const juce::String& id) { return id == n.id; });
                               }),
                notes.end());
    republishOffsets();
    return true;
}

void NoteEditModel::republishOffsets()
{
    offsets.publishFrom(notes);
}

juce::ValueTree NoteEditModel::toValueTree() const
{
    juce::ValueTree tree("HARMONY_NOTE_EDITS");
    tree.setProperty("snap", static_cast<int>(snapMode), nullptr);
    for (const auto& n : notes)
    {
        juce::ValueTree child("NOTE");
        child.setProperty("id", n.id, nullptr);
        child.setProperty("start", n.startSec, nullptr);
        child.setProperty("dur", n.durationSec, nullptr);
        child.setProperty("voice", n.voiceMidi, nullptr);
        child.setProperty("auto", n.autoHarmonyMidi, nullptr);
        child.setProperty("conf", n.confidence, nullptr);
        child.setProperty("offset", n.manualOffsetSemitones, nullptr);
        tree.addChild(child, -1, nullptr);
    }
    return tree;
}

void NoteEditModel::fromValueTree(const juce::ValueTree& tree)
{
    clear();
    if (! tree.hasType("HARMONY_NOTE_EDITS"))
        return;
    snapMode = static_cast<SnapMode>(juce::jlimit(0, 2, static_cast<int>(tree.getProperty("snap", 0))));
    std::vector<HarmonyNote> loaded;
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto child = tree.getChild(i);
        if (! child.hasType("NOTE"))
            continue;
        HarmonyNote n;
        n.id = child.getProperty("id").toString();
        n.startSec = static_cast<double>(child.getProperty("start"));
        n.durationSec = static_cast<double>(child.getProperty("dur"));
        n.voiceMidi = static_cast<float>(child.getProperty("voice"));
        n.autoHarmonyMidi = static_cast<float>(child.getProperty("auto"));
        n.confidence = static_cast<float>(child.getProperty("conf"));
        n.manualOffsetSemitones = static_cast<float>(child.getProperty("offset"));
        if (n.id.isEmpty())
            n.id = juce::Uuid().toString();
        loaded.push_back(n);
    }
    notes = std::move(loaded);
    republishOffsets();
}

NoteEditModel::OffsetAction::OffsetAction(NoteEditModel& owner, juce::String noteId, float from, float to)
    : model(owner), id(std::move(noteId)), previous(from), next(to)
{
}

bool NoteEditModel::OffsetAction::perform()
{
    if (auto* note = model.findNote(id))
    {
        note->manualOffsetSemitones = next;
        model.republishOffsets();
        return true;
    }
    return false;
}

bool NoteEditModel::OffsetAction::undo()
{
    if (auto* note = model.findNote(id))
    {
        note->manualOffsetSemitones = previous;
        model.republishOffsets();
        return true;
    }
    return false;
}

NoteEditModel::DeleteNotesAction::DeleteNotesAction(NoteEditModel& owner, std::vector<HarmonyNote> removed)
    : model(owner), removedNotes(std::move(removed))
{
}

bool NoteEditModel::DeleteNotesAction::perform()
{
    for (const auto& rem : removedNotes)
    {
        model.notes.erase(std::remove_if(model.notes.begin(), model.notes.end(),
                                         [&](const HarmonyNote& n) { return n.id == rem.id; }),
                          model.notes.end());
    }
    model.republishOffsets();
    return true;
}

bool NoteEditModel::DeleteNotesAction::undo()
{
    for (const auto& rem : removedNotes)
    {
        if (model.findNote(rem.id) == nullptr)
            model.notes.push_back(rem);
    }
    std::sort(model.notes.begin(), model.notes.end(),
              [](const HarmonyNote& a, const HarmonyNote& b) { return a.startSec < b.startSec; });
    model.republishOffsets();
    return true;
}
}
