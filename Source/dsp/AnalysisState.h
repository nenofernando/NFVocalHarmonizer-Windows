#pragma once

#include <atomic>
#include <cstdint>

enum class AnalysisState : uint8_t
{
    empty = 0,
    capturing,
    ready
};

inline bool analysisShouldFinalizeOnStop(bool wasPlaying, bool hostPlaying) noexcept
{
    // Loop / seek keep getIsPlaying() true — only Play→Stop finalizes.
    return wasPlaying && ! hostPlaying;
}

inline AnalysisState analysisStateAfterFinish(bool hasValidNewNotes, bool hasPublishedMap) noexcept
{
    if (hasValidNewNotes)
        return AnalysisState::ready;
    return hasPublishedMap ? AnalysisState::ready : AnalysisState::empty;
}
