#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <vector>

namespace nf::notes
{
enum class SnapMode { key = 0, chromatic, off };

/** Manual pitch breakpoint for pencil lines (message-thread edit → OffsetTable). */
struct PitchCurvePoint
{
    double timeSec = 0.0;
    float offsetSemitones = 0.0f; // relative to autoHarmonyMidi
};

struct HarmonyNote
{
    juce::String id;
    double startSec = 0.0;
    double durationSec = 0.0;
    float voiceMidi = 60.0f;
    float autoHarmonyMidi = 64.0f;
    float confidence = 0.0f;
    float manualOffsetSemitones = 0.0f; // flat correction when pitchCurve.size() < 2
    std::vector<PitchCurvePoint> pitchCurve; // ≥2 points = time-varying pencil line

    float offsetAt(double timeSec) const noexcept
    {
        if (pitchCurve.size() < 2)
            return manualOffsetSemitones;

        if (timeSec <= pitchCurve.front().timeSec)
            return pitchCurve.front().offsetSemitones;
        if (timeSec >= pitchCurve.back().timeSec)
            return pitchCurve.back().offsetSemitones;

        for (size_t i = 1; i < pitchCurve.size(); ++i)
        {
            const auto& a = pitchCurve[i - 1];
            const auto& b = pitchCurve[i];
            if (timeSec > b.timeSec)
                continue;
            const double span = juce::jmax(1.0e-9, b.timeSec - a.timeSec);
            const float t = static_cast<float>((timeSec - a.timeSec) / span);
            return a.offsetSemitones + t * (b.offsetSemitones - a.offsetSemitones);
        }
        return pitchCurve.back().offsetSemitones;
    }

    float editedHarmonyMidi() const noexcept { return autoHarmonyMidi + manualOffsetSemitones; }
    float editedHarmonyMidiAt(double timeSec) const noexcept { return autoHarmonyMidi + offsetAt(timeSec); }
    bool hasPitchCurve() const noexcept { return pitchCurve.size() >= 2; }
    bool isEdited() const noexcept
    {
        return std::abs(manualOffsetSemitones) > 1.0e-6f || hasPitchCurve();
    }
};

struct PitchSample
{
    double timeSec = 0.0;
    float midi = 0.0f;
    float confidence = 0.0f;
    float amplitude = 0.0f; // observe-only frame energy for vocal blobs / waveform
    bool voiced = false;
};

/** Lock-free publish of offset regions for the audio thread. */
class OffsetTable
{
public:
    static constexpr int maxRegions = 512;

    struct Region
    {
        double startSec = 0.0;
        double endSec = 0.0;
        // Absolute harmony MIDI targets (legacy field names kept for binary layout clarity).
        float offsetStart = 0.0f;
        float offsetEnd = 0.0f;
    };

    void clear() noexcept
    {
        counts[writeIndex.load(std::memory_order_relaxed)].store(0, std::memory_order_relaxed);
        publish();
    }

    void publishFrom(const std::vector<HarmonyNote>& notes)
    {
        const int next = 1 - writeIndex.load(std::memory_order_relaxed);
        auto& dest = buffers[static_cast<size_t>(next)];
        int n = 0;
        for (const auto& note : notes)
        {
            if (! note.isEdited())
                continue;

            // Audio thread locks to ABSOLUTE harmony MIDI (not vibrato-following auto+offset).
            const double noteStart = note.startSec;
            const double noteEnd = note.startSec + juce::jmax(0.02, note.durationSec);

            if (note.hasPitchCurve())
            {
                std::vector<PitchCurvePoint> pts;
                pts.push_back({ noteStart, note.offsetAt(noteStart) });
                for (const auto& pt : note.pitchCurve)
                {
                    if (pt.timeSec > noteStart + 1.0e-6 && pt.timeSec < noteEnd - 1.0e-6)
                        pts.push_back(pt);
                }
                pts.push_back({ noteEnd, note.offsetAt(noteEnd) });

                for (size_t i = 1; i < pts.size(); ++i)
                {
                    if (n >= maxRegions)
                        break;
                    const auto& a = pts[i - 1];
                    const auto& b = pts[i];
                    dest[static_cast<size_t>(n)] = {
                        a.timeSec, b.timeSec,
                        note.autoHarmonyMidi + a.offsetSemitones,
                        note.autoHarmonyMidi + b.offsetSemitones
                    };
                    ++n;
                }
            }
            else
            {
                if (n >= maxRegions)
                    break;
                const float absMidi = note.editedHarmonyMidi();
                dest[static_cast<size_t>(n)] = { noteStart, noteEnd, absMidi, absMidi };
                ++n;
            }
        }
        counts[next].store(n, std::memory_order_relaxed);
        writeIndex.store(next, std::memory_order_release);
    }

    /** True when host time falls inside a manually edited harmony region. */
    bool hasTargetAt(double timeSec) const noexcept
    {
        float ignored = 0.0f;
        return tryGetTargetMidi(timeSec, ignored);
    }

    /** Absolute harmony MIDI target for edited regions. Returns false outside edits. */
    bool tryGetTargetMidi(double timeSec, float& outMidi) const noexcept
    {
        const int idx = writeIndex.load(std::memory_order_acquire);
        const int n = counts[idx].load(std::memory_order_relaxed);
        const auto& src = buffers[static_cast<size_t>(idx)];
        for (int i = 0; i < n; ++i)
        {
            const auto& r = src[static_cast<size_t>(i)];
            if (timeSec >= r.startSec && timeSec < r.endSec)
            {
                const double span = juce::jmax(1.0e-9, r.endSec - r.startSec);
                const float t = static_cast<float>((timeSec - r.startSec) / span);
                outMidi = r.offsetStart + t * (r.offsetEnd - r.offsetStart);
                return true;
            }
            if (timeSec == r.endSec)
            {
                outMidi = r.offsetEnd;
                return true;
            }
        }
        return false;
    }

    /** Absolute harmony MIDI, or 0 when no edit covers this time (legacy test helper). */
    float offsetAt(double timeSec) const noexcept
    {
        float midi = 0.0f;
        return tryGetTargetMidi(timeSec, midi) ? midi : 0.0f;
    }

private:
    void publish() noexcept { writeIndex.store(writeIndex.load(std::memory_order_relaxed), std::memory_order_release); }

    std::array<std::array<Region, maxRegions>, 2> buffers {};
    std::array<std::atomic<int>, 2> counts { 0, 0 };
    std::atomic<int> writeIndex { 0 };
};

/** Single-producer (audio) / single-consumer (message) pitch sample ring. */
class PitchSampleRing
{
public:
    static constexpr int capacity = 8192;

    void reset() noexcept
    {
        writePos.store(0, std::memory_order_relaxed);
        readPos.store(0, std::memory_order_relaxed);
    }

    bool push(const PitchSample& sample) noexcept
    {
        const auto w = writePos.load(std::memory_order_relaxed);
        const auto next = (w + 1) % capacity;
        if (next == readPos.load(std::memory_order_acquire))
            return false; // full — drop sample, no allocation
        data[static_cast<size_t>(w)] = sample;
        writePos.store(next, std::memory_order_release);
        return true;
    }

    bool pop(PitchSample& sample) noexcept
    {
        const auto r = readPos.load(std::memory_order_relaxed);
        if (r == writePos.load(std::memory_order_acquire))
            return false;
        sample = data[static_cast<size_t>(r)];
        readPos.store((r + 1) % capacity, std::memory_order_release);
        return true;
    }

private:
    std::array<PitchSample, capacity> data {};
    std::atomic<int> writePos { 0 }, readPos { 0 };
};
}
