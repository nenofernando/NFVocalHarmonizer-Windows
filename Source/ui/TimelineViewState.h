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
        if (! std::isfinite(deltaY) || std::abs(deltaY) < 1.0e-6f)
            return;
        // Cap per-event gain so Magic Mouse / trackpad bursts don't jump the camera.
        deltaY = juce::jlimit(-0.35f, 0.35f, deltaY);
        const double timeUnderPointer = timeAtFraction(pointerFraction);
        const double factor = std::exp(static_cast<double>(-deltaY) * 0.55);
        viewDurationSec = juce::jlimit(minVisibleSec, maxVisibleSec(), viewDurationSec * factor);
        const double frac = juce::jlimit(0.0, 1.0, pointerFraction);
        viewStartSec = timeUnderPointer - frac * viewDurationSec;
        clampView();
    }

    /** Positive deltaY pans left (earlier); negative pans right (later). */
    void panFromWheel(float deltaY) noexcept
    {
        if (! std::isfinite(deltaY) || std::abs(deltaY) < 1.0e-6f)
            return;
        deltaY = juce::jlimit(-0.45f, 0.45f, deltaY);
        viewStartSec -= static_cast<double>(deltaY) * viewDurationSec * 0.40;
        clampView();
    }

    /**
     * Alt/Option drag pan. dragDistanceX = mouseX - dragStartX (pixels).
     * Dragging right reveals earlier audio; dragging left reveals later audio.
     * Notes / DSP timestamps are untouched — viewStartSec only.
     */
    void panByPixelDrag(double dragStartVisibleTime, double dragDistanceX, double timelineWidthPixels) noexcept
    {
        if (timelineWidthPixels <= 1.0e-9)
            return;
        const double secondsPerPixel = viewDurationSec / timelineWidthPixels;
        viewStartSec = dragStartVisibleTime - dragDistanceX * secondsPerPixel;
        clampView();
    }
};

/** Visible MIDI range after margin / min-octave / clamp (visual only). */
struct VisiblePitchRange
{
    float minimum = 48.0f;
    float maximum = 84.0f;
};

/**
 * Build the auto-fit pitch window from raw note extrema.
 * High notes map near the top of the lane; low notes near the bottom.
 */
inline VisiblePitchRange computeVisiblePitchRange(float minimumPitch, float maximumPitch,
                                                  float pitchMarginSemitones = 2.0f,
                                                  float minimumVisibleRange = 12.0f) noexcept
{
    if (! std::isfinite(minimumPitch) || ! std::isfinite(maximumPitch)
        || maximumPitch < minimumPitch)
    {
        return { 48.0f, 60.0f };
    }

    float visibleMinimumPitch = std::floor(minimumPitch - pitchMarginSemitones);
    float visibleMaximumPitch = std::ceil(maximumPitch + pitchMarginSemitones);

    if (visibleMaximumPitch - visibleMinimumPitch < minimumVisibleRange)
    {
        const float centre = (visibleMaximumPitch + visibleMinimumPitch) * 0.5f;
        visibleMinimumPitch = centre - minimumVisibleRange * 0.5f;
        visibleMaximumPitch = centre + minimumVisibleRange * 0.5f;
    }

    visibleMinimumPitch = juce::jlimit(0.0f, 127.0f, visibleMinimumPitch);
    visibleMaximumPitch = juce::jlimit(0.0f, 127.0f, visibleMaximumPitch);
    if (visibleMaximumPitch - visibleMinimumPitch < minimumVisibleRange)
    {
        if (visibleMinimumPitch <= 0.0f)
            visibleMaximumPitch = juce::jmin(127.0f, visibleMinimumPitch + minimumVisibleRange);
        else if (visibleMaximumPitch >= 127.0f)
            visibleMinimumPitch = juce::jmax(0.0f, visibleMaximumPitch - minimumVisibleRange);
    }

    return { visibleMinimumPitch, visibleMaximumPitch };
}

/** Visual-only vertical pitch window (MIDI). Auto-fit keeps all notes on-screen. */
struct PitchViewState
{
    static constexpr float absoluteMinMidi = 0.0f;
    static constexpr float absoluteMaxMidi = 127.0f;
    static constexpr float minSpanMidi = 12.0f;
    static constexpr float maxSpanMidi = 127.0f;
    static constexpr float defaultSpanMidi = 36.0f;
    static constexpr float pitchMarginSemitones = 2.0f;

    float viewTopMidi = 84.0f;
    float viewSpanMidi = defaultSpanMidi;
    float contentMinMidi = 48.0f;
    float contentMaxMidi = 84.0f;

    float viewBottomMidi() const noexcept { return viewTopMidi - viewSpanMidi; }

    void setContentMidiRange(float minMidi, float maxMidi) noexcept
    {
        contentMinMidi = juce::jlimit(absoluteMinMidi, absoluteMaxMidi - minSpanMidi, minMidi);
        contentMaxMidi = juce::jlimit(contentMinMidi + minSpanMidi, absoluteMaxMidi, maxMidi);
        clampView();
    }

    /** Fit the viewport so every note in contentMin/Max is visible (with margin). */
    void fitContent(float paddingSemitones = pitchMarginSemitones) noexcept
    {
        const auto range = computeVisiblePitchRange(contentMinMidi, contentMaxMidi,
                                                    paddingSemitones, minSpanMidi);
        viewSpanMidi = juce::jlimit(minSpanMidi, maxSpanMidi, range.maximum - range.minimum);
        viewTopMidi = range.maximum;
        clampView();
    }

    /** Expand viewport upward/downward to include a pitch; never shrink. */
    void expandToInclude(float midiPitch) noexcept
    {
        if (! std::isfinite(midiPitch))
            return;
        midiPitch = juce::jlimit(absoluteMinMidi, absoluteMaxMidi, midiPitch);
        contentMinMidi = juce::jmin(contentMinMidi, midiPitch);
        contentMaxMidi = juce::jmax(contentMaxMidi, midiPitch);

        const float bottom = viewBottomMidi();
        if (midiPitch < bottom)
        {
            const float need = bottom - midiPitch + pitchMarginSemitones;
            viewSpanMidi = juce::jmin(maxSpanMidi, viewSpanMidi + need);
            // Keep top; span grows downward.
        }
        else if (midiPitch > viewTopMidi)
        {
            const float need = midiPitch - viewTopMidi + pitchMarginSemitones;
            viewTopMidi = midiPitch + pitchMarginSemitones;
            viewSpanMidi = juce::jmin(maxSpanMidi, viewSpanMidi + need);
        }
        clampView();
    }

    void clampView() noexcept
    {
        viewSpanMidi = juce::jlimit(minSpanMidi, maxSpanMidi, viewSpanMidi);
        const float padMin = juce::jmax(absoluteMinMidi, contentMinMidi - 6.0f);
        const float padMax = juce::jmin(absoluteMaxMidi, contentMaxMidi + 6.0f);
        const float room = padMax - padMin;
        if (room <= viewSpanMidi + 1.0e-3f)
        {
            // Show the full content band centred in the span when possible.
            viewTopMidi = padMin + viewSpanMidi;
            if (viewTopMidi < padMax)
                viewTopMidi = padMax;
            viewTopMidi = juce::jlimit(absoluteMinMidi + viewSpanMidi, absoluteMaxMidi, viewTopMidi);
            return;
        }
        const float minTop = padMin + viewSpanMidi;
        const float maxTop = padMax;
        viewTopMidi = juce::jlimit(minTop, maxTop, viewTopMidi);
    }

    /** Scroll up (positive deltaY) reveals higher pitches. */
    void panFromWheel(float deltaY) noexcept
    {
        if (! std::isfinite(deltaY) || std::abs(deltaY) < 1.0e-6f)
            return;
        deltaY = juce::jlimit(-0.45f, 0.45f, deltaY);
        viewTopMidi += deltaY * viewSpanMidi * 0.32f;
        clampView();
    }

    /** Pixel drag: positive dragY (mouse moved down) reveals higher pitches. */
    void panByPixelDrag(float dragStartTopMidi, float dragDistanceY, float laneHeightPixels) noexcept
    {
        if (laneHeightPixels <= 1.0e-3f)
            return;
        const float semisPerPixel = viewSpanMidi / laneHeightPixels;
        viewTopMidi = dragStartTopMidi + dragDistanceY * semisPerPixel;
        clampView();
    }

    /** Zoom pitch around a vertical fraction (0 = top, 1 = bottom). Positive deltaY = zoom in. */
    void zoomAtFraction(float deltaY, float pointerFraction) noexcept
    {
        if (! std::isfinite(deltaY) || std::abs(deltaY) < 1.0e-6f)
            return;
        deltaY = juce::jlimit(-0.35f, 0.35f, deltaY);
        const float frac = juce::jlimit(0.0f, 1.0f, pointerFraction);
        const float midiUnderPointer = viewTopMidi - frac * viewSpanMidi;
        const float factor = static_cast<float>(std::exp(static_cast<double>(-deltaY) * 0.55));
        viewSpanMidi = juce::jlimit(minSpanMidi, maxSpanMidi, viewSpanMidi * factor);
        viewTopMidi = midiUnderPointer + frac * viewSpanMidi;
        clampView();
    }
};
}
