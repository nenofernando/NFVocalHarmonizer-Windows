#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>

namespace nf::notes
{
enum class SnapMode { key = 0, chromatic, off };

struct HarmonyNote
{
    juce::String id;
    double startSec = 0.0;
    double durationSec = 0.0;
    float voiceMidi = 60.0f;
    float autoHarmonyMidi = 64.0f;
    float confidence = 0.0f;
    float manualOffsetSemitones = 0.0f; // added after auto target, before pitch shift

    float editedHarmonyMidi() const noexcept { return autoHarmonyMidi + manualOffsetSemitones; }
    bool isEdited() const noexcept { return std::abs(manualOffsetSemitones) > 1.0e-6f; }
};

struct PitchSample
{
    double timeSec = 0.0;
    float midi = 0.0f;
    float confidence = 0.0f;
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
        float offsetSemitones = 0.0f;
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
            if (n >= maxRegions)
                break;
            if (! note.isEdited())
                continue;
            dest[static_cast<size_t>(n)] = { note.startSec, note.startSec + note.durationSec,
                                             note.manualOffsetSemitones };
            ++n;
        }
        counts[next].store(n, std::memory_order_relaxed);
        writeIndex.store(next, std::memory_order_release);
    }

    float offsetAt(double timeSec) const noexcept
    {
        const int idx = writeIndex.load(std::memory_order_acquire);
        const int n = counts[idx].load(std::memory_order_relaxed);
        const auto& src = buffers[static_cast<size_t>(idx)];
        for (int i = 0; i < n; ++i)
        {
            const auto& r = src[static_cast<size_t>(i)];
            if (timeSec >= r.startSec && timeSec < r.endSec)
                return r.offsetSemitones;
        }
        return 0.0f;
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
