#include "NoteCapture.h"
#include <cmath>

namespace nf::notes
{
void NoteCapture::reset() noexcept
{
    armed.store(false, std::memory_order_release);
    sampleRing.reset();
}

void NoteCapture::clearRaw()
{
    raw.clear();
}

void NoteCapture::drainRing()
{
    PitchSample sample;
    while (sampleRing.pop(sample))
        raw.push_back(sample);
}

std::vector<HarmonyNote> NoteCapture::buildNotes(int intervalChoice, int keyRoot, nf::dsp::ScaleType scale) const
{
    std::vector<HarmonyNote> notes;
    if (raw.empty())
        return notes;

    constexpr float minConfidence = 0.55f;
    constexpr float mergeSemitone = 0.45f;
    constexpr double minDuration = 0.08;
    constexpr double maxGap = 0.06;

    struct Acc
    {
        double start = 0.0, end = 0.0;
        double midiSum = 0.0, confSum = 0.0;
        int count = 0;
    };

    Acc current {};
    bool open = false;

    auto flush = [&]()
    {
        if (! open || current.count == 0)
            return;
        const double dur = current.end - current.start;
        if (dur < minDuration)
        {
            open = false;
            return;
        }
        HarmonyNote n;
        n.id = juce::Uuid().toString();
        n.startSec = current.start;
        n.durationSec = dur;
        n.voiceMidi = static_cast<float>(current.midiSum / static_cast<double>(current.count));
        n.confidence = static_cast<float>(current.confSum / static_cast<double>(current.count));
        n.autoHarmonyMidi = nf::dsp::MusicalScale::targetMidi(n.voiceMidi, intervalChoice, keyRoot, scale);
        n.manualOffsetSemitones = 0.0f;
        notes.push_back(n);
        open = false;
    };

    for (const auto& s : raw)
    {
        if (! s.voiced || s.confidence < minConfidence)
        {
            flush();
            continue;
        }

        if (! open)
        {
            current = { s.timeSec, s.timeSec, s.midi, s.confidence, 1 };
            open = true;
            continue;
        }

        const float mean = static_cast<float>(current.midiSum / static_cast<double>(current.count));
        const bool samePitch = std::abs(s.midi - mean) <= mergeSemitone;
        const bool contiguous = (s.timeSec - current.end) <= maxGap;
        if (samePitch && contiguous)
        {
            current.end = s.timeSec;
            current.midiSum += s.midi;
            current.confSum += s.confidence;
            ++current.count;
        }
        else
        {
            flush();
            current = { s.timeSec, s.timeSec, s.midi, s.confidence, 1 };
            open = true;
        }
    }
    flush();
    return notes;
}
}
