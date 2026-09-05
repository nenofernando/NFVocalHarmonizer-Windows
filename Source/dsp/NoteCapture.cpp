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

    // Looser merge than before: sung vibrato + short consonants used to explode into tiny blocks.
    constexpr float minConfidence = 0.50f;
    constexpr float mergeSemitone = 0.95f;
    constexpr double minDuration = 0.12;
    constexpr double maxGap = 0.16;
    constexpr int maxMissHops = 3;          // tolerate brief unvoiced dips inside a note
    constexpr double hopPadSec = 0.010;     // extend end by ~1 pitch hop
    constexpr float postMergeSemitone = 0.85f;
    constexpr double postMergeGap = 0.14;

    struct Acc
    {
        double start = 0.0, end = 0.0;
        double midiSum = 0.0, confSum = 0.0;
        int count = 0;
    };

    Acc current {};
    bool open = false;
    int missHops = 0;

    auto flush = [&]()
    {
        if (! open || current.count == 0)
            return;
        const double dur = (current.end - current.start) + hopPadSec;
        missHops = 0;
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
            if (open)
            {
                ++missHops;
                if (missHops > maxMissHops)
                    flush();
            }
            continue;
        }

        missHops = 0;

        if (! open)
        {
            current = { s.timeSec, s.timeSec, s.midi, s.confidence, 1 };
            open = true;
            continue;
        }

        const float mean = static_cast<float>(current.midiSum / static_cast<double>(current.count));
        const bool samePitch = std::abs(s.midi - mean) <= mergeSemitone
                               || std::abs(std::round(s.midi) - std::round(mean)) <= 1.0f;
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

    // Second pass: glue neighbouring shards that are musically the same note.
    if (notes.size() > 1)
    {
        std::vector<HarmonyNote> merged;
        merged.reserve(notes.size());
        merged.push_back(notes.front());
        for (size_t i = 1; i < notes.size(); ++i)
        {
            auto& prev = merged.back();
            const auto& next = notes[i];
            const double gap = next.startSec - (prev.startSec + prev.durationSec);
            const bool closeInTime = gap >= -0.02 && gap <= postMergeGap;
            const bool closeInPitch = std::abs(next.voiceMidi - prev.voiceMidi) <= postMergeSemitone;
            if (closeInTime && closeInPitch)
            {
                const double nextEnd = next.startSec + next.durationSec;
                const float w0 = prev.confidence;
                const float w1 = next.confidence;
                prev.voiceMidi = (prev.voiceMidi * w0 + next.voiceMidi * w1) / juce::jmax(1.0e-6f, w0 + w1);
                prev.confidence = 0.5f * (w0 + w1);
                prev.durationSec = nextEnd - prev.startSec;
                prev.autoHarmonyMidi = nf::dsp::MusicalScale::targetMidi(prev.voiceMidi, intervalChoice, keyRoot, scale);
            }
            else
            {
                merged.push_back(next);
            }
        }
        notes.swap(merged);
    }

    return notes;
}
}
