#pragma once

#include <atomic>

enum class AnalysisState : int
{
    idle = 0,
    armed,
    analyzing,
    completed,
    failed
};

inline AnalysisState analysisStateForBegin(bool hostPlaying) noexcept
{
    return hostPlaying ? AnalysisState::analyzing : AnalysisState::armed;
}

inline AnalysisState analysisStateForFinalize(bool hasNotes) noexcept
{
    return hasNotes ? AnalysisState::completed : AnalysisState::failed;
}

inline AnalysisState analysisStateAfterArmedSeesPlay(AnalysisState state, bool hostPlaying) noexcept
{
    return (state == AnalysisState::armed && hostPlaying) ? AnalysisState::analyzing : state;
}

inline bool analysisShouldFinalizeOnStop(bool wasPlaying, bool hostPlaying) noexcept
{
    // Loop / seek keep getIsPlaying() true — only Play→Stop finalizes.
    return wasPlaying && ! hostPlaying;
}
