#pragma once

#include "HarmonyNoteTypes.h"
#include "MusicalScale.h"
#include <vector>

namespace nf::notes
{
class NoteCapture
{
public:
    void reset() noexcept;
    void setArmed(bool shouldArm) noexcept { armed.store(shouldArm, std::memory_order_release); }
    bool isArmed() const noexcept { return armed.load(std::memory_order_acquire); }

    PitchSampleRing& ring() noexcept { return sampleRing; }

    /** Drain ring and append to internal raw buffer (message thread). */
    void drainRing();

    /** Build stable note blocks from captured samples. */
    std::vector<HarmonyNote> buildNotes(int intervalChoice, int keyRoot, nf::dsp::ScaleType scale) const;

    void clearRaw();

private:
    PitchSampleRing sampleRing;
    std::vector<PitchSample> raw;
    std::atomic<bool> armed { false };
};
}
