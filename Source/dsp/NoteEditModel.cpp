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

size_t NoteEditModel::appendNotes(std::vector<HarmonyNote> incoming)
{
    if (incoming.empty())
        return 0;

    size_t added = 0;
    for (auto& n : incoming)
    {
        bool overlaps = false;
        const double a0 = n.startSec;
        const double a1 = n.startSec + juce::jmax(0.0, n.durationSec);
        for (const auto& e : notes)
        {
            const double b0 = e.startSec;
            const double b1 = e.startSec + juce::jmax(0.0, e.durationSec);
            // Protect previous captures — never overwrite or merge into them.
            if (a0 < b1 - 0.02 && a1 > b0 + 0.02)
            {
                overlaps = true;
                break;
            }
        }
        if (overlaps)
            continue;
        notes.push_back(std::move(n));
        ++added;
    }

    if (added == 0)
        return 0;

    std::sort(notes.begin(), notes.end(),
              [](const HarmonyNote& a, const HarmonyNote& b) { return a.startSec < b.startSec; });
    // Keep undo history for prior manual edits on earlier sections.
    republishOffsets();
    return added;
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
    return applyFlatPencil(id, offsetSemitones, um);
}

bool NoteEditModel::resetManualOffset(const juce::String& id, juce::UndoManager* um)
{
    return applyFlatPencil(id, 0.0f, um);
}

bool NoteEditModel::applyFlatPencil(const juce::String& id, float offsetSemitones, juce::UndoManager* um)
{
    auto* note = findNote(id);
    if (note == nullptr)
        return false;
    return applyFlatPencilAbsolute(id, note->autoHarmonyMidi + offsetSemitones, note->autoHarmonyMidi, um);
}

bool NoteEditModel::applyFlatPencilAbsolute(const juce::String& id, float absoluteMidi, float autoBaseMidi,
                                            juce::UndoManager* um)
{
    auto* note = findNote(id);
    if (note == nullptr)
        return false;

    HarmonyNote before = *note;
    HarmonyNote after = *note;
    after.autoHarmonyMidi = autoBaseMidi;
    after.manualOffsetSemitones = juce::jlimit(-24.0f, 24.0f, absoluteMidi - autoBaseMidi);
    after.pitchCurve.clear();

    if (std::abs(before.manualOffsetSemitones - after.manualOffsetSemitones) < 1.0e-6f
        && std::abs(before.autoHarmonyMidi - after.autoHarmonyMidi) < 1.0e-6f
        && before.pitchCurve.empty() && after.pitchCurve.empty())
        return false;

    if (um != nullptr)
    {
        um->beginNewTransaction("Pencil flat");
        return um->perform(new NoteMutateAction(*this, std::move(before), std::move(after)));
    }

    *note = std::move(after);
    republishOffsets();
    return true;
}

bool NoteEditModel::applySlopePencil(const juce::String& id,
                                     double t0, float offset0,
                                     double t1, float offset1,
                                     juce::UndoManager* um)
{
    auto* note = findNote(id);
    if (note == nullptr)
        return false;
    return applySlopePencilAbsolute(id,
                                    t0, note->autoHarmonyMidi + offset0,
                                    t1, note->autoHarmonyMidi + offset1,
                                    note->autoHarmonyMidi, um);
}

bool NoteEditModel::applySlopePencilAbsolute(const juce::String& id,
                                             double t0, float absoluteMidi0,
                                             double t1, float absoluteMidi1,
                                             float autoBaseMidi,
                                             juce::UndoManager* um)
{
    auto* note = findNote(id);
    if (note == nullptr)
        return false;

    const double noteStart = note->startSec;
    const double noteEnd = note->startSec + juce::jmax(0.02, note->durationSec);
    double aT = juce::jlimit(noteStart, noteEnd, t0);
    double bT = juce::jlimit(noteStart, noteEnd, t1);
    float aMidi = juce::jlimit(0.0f, 127.0f, absoluteMidi0);
    float bMidi = juce::jlimit(0.0f, 127.0f, absoluteMidi1);
    if (bT < aT)
    {
        std::swap(aT, bT);
        std::swap(aMidi, bMidi);
    }
    if (bT - aT < 1.0e-4)
        return applyFlatPencilAbsolute(id, 0.5f * (aMidi + bMidi), autoBaseMidi, um);

    HarmonyNote before = *note;
    HarmonyNote after = *note;
    after.autoHarmonyMidi = autoBaseMidi;
    const float aOff = juce::jlimit(-24.0f, 24.0f, aMidi - autoBaseMidi);
    const float bOff = juce::jlimit(-24.0f, 24.0f, bMidi - autoBaseMidi);
    // Full-note coverage: hold endpoints outside the drawn segment.
    after.pitchCurve = {
        { noteStart, aOff },
        { aT, aOff },
        { bT, bOff },
        { noteEnd, bOff }
    };
    after.manualOffsetSemitones = 0.5f * (aOff + bOff);

    if (um != nullptr)
    {
        um->beginNewTransaction("Pencil slope");
        return um->perform(new NoteMutateAction(*this, std::move(before), std::move(after)));
    }

    *note = std::move(after);
    republishOffsets();
    return true;
}

juce::String NoteEditModel::splitNoteAt(const juce::String& id, double cutTimeSec, juce::UndoManager* um)
{
    auto* note = findNote(id);
    if (note == nullptr)
        return {};

    const double start = note->startSec;
    const double end = note->startSec + note->durationSec;
    constexpr double minPart = 0.04;
    if (cutTimeSec <= start + minPart || cutTimeSec >= end - minPart)
        return {};

    HarmonyNote original = *note;
    HarmonyNote left = *note;
    HarmonyNote right = *note;
    right.id = juce::Uuid().toString();
    left.durationSec = cutTimeSec - start;
    right.startSec = cutTimeSec;
    right.durationSec = end - cutTimeSec;

    auto splitCurve = [](const HarmonyNote& src, HarmonyNote& part)
    {
        if (! src.hasPitchCurve())
        {
            part.pitchCurve.clear();
            return;
        }
        std::vector<PitchCurvePoint> pts;
        const double p0 = part.startSec;
        const double p1 = part.startSec + part.durationSec;
        pts.push_back({ p0, src.offsetAt(p0) });
        for (const auto& pt : src.pitchCurve)
        {
            if (pt.timeSec > p0 + 1.0e-6 && pt.timeSec < p1 - 1.0e-6)
                pts.push_back(pt);
        }
        pts.push_back({ p1, src.offsetAt(p1) });
        if (pts.size() >= 2
            && std::abs(pts.front().offsetSemitones - pts.back().offsetSemitones) < 1.0e-5f
            && pts.size() == 2)
        {
            part.manualOffsetSemitones = pts.front().offsetSemitones;
            part.pitchCurve.clear();
        }
        else
        {
            part.pitchCurve = std::move(pts);
            part.manualOffsetSemitones = 0.5f * (part.offsetAt(p0) + part.offsetAt(p1));
        }
    };
    splitCurve(original, left);
    splitCurve(original, right);

    if (um != nullptr)
    {
        um->beginNewTransaction("Split note");
        if (! um->perform(new SplitNoteAction(*this, std::move(original), left, right)))
            return {};
        return right.id;
    }

    *note = left;
    notes.push_back(right);
    std::sort(notes.begin(), notes.end(),
              [](const HarmonyNote& a, const HarmonyNote& b) { return a.startSec < b.startSec; });
    republishOffsets();
    return right.id;
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
        if (n.hasPitchCurve())
        {
            juce::ValueTree curve("CURVE");
            for (const auto& pt : n.pitchCurve)
            {
                juce::ValueTree p("P");
                p.setProperty("t", pt.timeSec, nullptr);
                p.setProperty("o", pt.offsetSemitones, nullptr);
                curve.addChild(p, -1, nullptr);
            }
            child.addChild(curve, -1, nullptr);
        }
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
        const auto curve = child.getChildWithName("CURVE");
        if (curve.isValid())
        {
            for (int c = 0; c < curve.getNumChildren(); ++c)
            {
                const auto p = curve.getChild(c);
                if (! p.hasType("P"))
                    continue;
                n.pitchCurve.push_back({
                    static_cast<double>(p.getProperty("t")),
                    static_cast<float>(p.getProperty("o"))
                });
            }
        }
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
        note->pitchCurve.clear();
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
        note->pitchCurve.clear();
        model.republishOffsets();
        return true;
    }
    return false;
}

NoteEditModel::NoteMutateAction::NoteMutateAction(NoteEditModel& owner, HarmonyNote before, HarmonyNote after)
    : model(owner), previous(std::move(before)), next(std::move(after))
{
}

bool NoteEditModel::NoteMutateAction::perform()
{
    if (auto* note = model.findNote(next.id))
    {
        *note = next;
        model.republishOffsets();
        return true;
    }
    return false;
}

bool NoteEditModel::NoteMutateAction::undo()
{
    if (auto* note = model.findNote(previous.id))
    {
        *note = previous;
        model.republishOffsets();
        return true;
    }
    return false;
}

NoteEditModel::SplitNoteAction::SplitNoteAction(NoteEditModel& owner, HarmonyNote original,
                                                HarmonyNote left, HarmonyNote right)
    : model(owner), originalNote(std::move(original)), leftNote(std::move(left)), rightNote(std::move(right))
{
}

bool NoteEditModel::SplitNoteAction::perform()
{
    auto* note = model.findNote(originalNote.id);
    if (note == nullptr)
        return false;
    *note = leftNote;
    if (model.findNote(rightNote.id) == nullptr)
        model.notes.push_back(rightNote);
    std::sort(model.notes.begin(), model.notes.end(),
              [](const HarmonyNote& a, const HarmonyNote& b) { return a.startSec < b.startSec; });
    model.republishOffsets();
    return true;
}

bool NoteEditModel::SplitNoteAction::undo()
{
    model.notes.erase(std::remove_if(model.notes.begin(), model.notes.end(),
                                     [&](const HarmonyNote& n)
                                     {
                                         return n.id == leftNote.id || n.id == rightNote.id;
                                     }),
                      model.notes.end());
    model.notes.push_back(originalNote);
    std::sort(model.notes.begin(), model.notes.end(),
              [](const HarmonyNote& a, const HarmonyNote& b) { return a.startSec < b.startSec; });
    model.republishOffsets();
    return true;
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
