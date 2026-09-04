#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace nf::notes
{
/** Visual-only timeline window for the VOICE/HARMONY editor (not an APVTS parameter). */
struct TimelineViewState
{
    static constexpr double minVisibleSec = 0.25; // Zoom In floor (~250 ms)

    double viewStartSec = 0.0;
    double viewDurationSec = 8.0;
    double contentStartSec = 0.0;
    double contentEndSec = 8.0;

    double contentLength() const noexcept
    {
        return juce::jmax(minVisibleSec, contentEndSec - contentStartSec);
    }

    double maxVisibleSec() const noexcept { return contentLength(); }

    double zoomPercent() const noexcept
    {
        const auto dur = juce::jmax(1.0e-9, viewDurationSec);
        return 100.0 * maxVisibleSec() / dur;
    }

    void setContentRange(double startSec, double endSec) noexcept
    {
        contentStartSec = startSec;
        contentEndSec = juce::jmax(startSec + minVisibleSec, endSec);
        clampView();
    }

    void clampView() noexcept
    {
        const auto maxDur = maxVisibleSec();
        viewDurationSec = juce::jlimit(minVisibleSec, maxDur, viewDurationSec);
        const auto maxStart = juce::jmax(contentStartSec, contentEndSec - viewDurationSec);
        viewStartSec = juce::jlimit(contentStartSec, maxStart, viewStartSec);
    }

    double timeAtFraction(double fraction) const noexcept
    {
        return viewStartSec + juce::jlimit(0.0, 1.0, fraction) * viewDurationSec;
    }

    double fractionAtTime(double timeSec) const noexcept
    {
        if (viewDurationSec <= 1.0e-12)
            return 0.0;
        return (timeSec - viewStartSec) / viewDurationSec;
    }

    /** Exponential zoom. Positive deltaY = zoom in (scroll up). Keeps timeAtFraction under the pointer. */
    void zoomAtFraction(float deltaY, double pointerFraction) noexcept
    {
        const double timeUnderPointer = timeAtFraction(pointerFraction);
        // Smooth & proportional; Magic Mouse / trackpad send fractional deltas.
        const double factor = std::exp(static_cast<double>(-deltaY) * 0.85);
        viewDurationSec = juce::jlimit(minVisibleSec, maxVisibleSec(), viewDurationSec * factor);
        const double frac = juce::jlimit(0.0, 1.0, pointerFraction);
        viewStartSec = timeUnderPointer - frac * viewDurationSec;
        clampView();
    }

    /** Positive deltaY pans left (earlier); negative pans right (later). */
    void panFromWheel(float deltaY) noexcept
    {
        viewStartSec -= static_cast<double>(deltaY) * viewDurationSec * 0.55;
        clampView();
    }
};
}
