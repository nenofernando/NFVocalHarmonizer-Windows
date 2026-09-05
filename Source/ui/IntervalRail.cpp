#include "IntervalRail.h"
#include <cmath>

namespace
{
struct RailGeometry
{
    float cx = 0.0f;
    float top = 20.0f;
    float bottom = 0.0f;
    float step = 32.0f;

    static RailGeometry fromBounds(juce::Rectangle<float> r)
    {
        RailGeometry g;
        g.cx = r.getCentreX();

        // 9 nodes (rows 0..8): +8ve … VOICE … -8ve. Never clip the octave ends.
        // Prefer ~32px pitch when height allows; shrink evenly when the rail is short.
        constexpr float preferredStep = 32.0f;
        constexpr float edgePad = 14.0f;
        constexpr float spanRows = 8.0f;

        const float usable = juce::jmax(8.0f, r.getHeight() - edgePad * 2.0f);
        const float fitStep = usable / spanRows;

        if (fitStep >= preferredStep)
        {
            g.step = preferredStep;
            const float span = g.step * spanRows;
            g.top = (r.getHeight() - span) * 0.5f;
        }
        else
        {
            g.step = fitStep;
            g.top = edgePad;
        }
        g.bottom = g.top + g.step * spanRows;
        return g;
    }

    float rowY(int row) const noexcept
    {
        return top + step * static_cast<float>(row);
    }
};
}

IntervalRail::IntervalRail(juce::AudioProcessorValueTreeState& s) : state(s)
{
    selected.store(juce::roundToInt(state.getRawParameterValue(nf::params::interval)->load()));
    state.addParameterListener(nf::params::interval, this);
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

IntervalRail::~IntervalRail() { state.removeParameterListener(nf::params::interval, this); }

void IntervalRail::parameterChanged(const juce::String&, float v)
{
    selected.store(juce::roundToInt(v));
    repaint();
}

int IntervalRail::choiceForRow(int row) const
{
    // Top→bottom: +8ve … VOICE/tônica … -8ve  (indices match Parameters::intervalNames)
    static constexpr int map[9] { 8, 7, 6, 5, 4, 3, 2, 1, 0 };
    return map[juce::jlimit(0, 8, row)];
}

int IntervalRail::hitTestIntervalCircle(juce::Point<float> pos) const
{
    const auto g = RailGeometry::fromBounds(getLocalBounds().toFloat());
    constexpr float hitRadius = 11.0f;
    constexpr float voiceHitRadius = 16.0f;

    for (int row = 0; row < 9; ++row)
    {
        const int choice = choiceForRow(row);
        const bool voice = (choice == 4);
        const float y = g.rowY(row);
        const float dx = pos.x - g.cx;
        const float dy = pos.y - y;
        const float radius = voice ? voiceHitRadius : hitRadius;
        if ((dx * dx + dy * dy) <= radius * radius)
            return choice;
    }
    return -1;
}

void IntervalRail::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const auto geom = RailGeometry::fromBounds(r);
    const float cx = geom.cx;
    g.setColour(juce::Colour(0xff75808c));
    g.fillRect(cx - 1.0f, geom.top, 2.0f, geom.bottom - geom.top);

    static const juce::StringArray labels { "+8ve", "+6th", "+5th", "+3rd", "VOICE",
                                            "-3rd", "-5th", "-6th", "-8ve" };
    for (int row = 0; row < 9; ++row)
    {
        const float y = geom.rowY(row);
        const int choice = choiceForRow(row);
        const bool voice = (choice == 4); // Tônica central (VOICE / unison)
        const bool active = choice == selected.load();
        const auto colour = voice ? juce::Colour(0xff38ddff)
                                  : active ? juce::Colour(0xffaa4df4) : juce::Colour(0xffd2d7dd);
        if (voice)
        {
            // Slightly tighter glow so neighbours keep a clean gap.
            g.setColour(colour.withAlpha(active ? 0.85f : 0.65f));
            g.fillRoundedRectangle(cx - 96.0f, y - 1.0f, 192.0f, 2.0f, 1.0f);
            g.setColour(colour.withAlpha(active ? 0.32f : 0.22f));
            g.fillEllipse(cx - 20.0f, y - 20.0f, 40.0f, 40.0f);
            g.setColour(juce::Colour(0xff10151d));
            g.fillEllipse(cx - 15.0f, y - 15.0f, 30.0f, 30.0f);
            g.setColour(colour);
            g.drawEllipse(cx - 15.0f, y - 15.0f, 30.0f, 30.0f, active ? 3.5f : 3.0f);
            g.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
            g.drawFittedText(labels[row],
                             juce::Rectangle<int>(static_cast<int>(cx - 160), static_cast<int>(y - 12), 100, 24),
                             juce::Justification::centredRight, 1);
        }
        else
        {
            if (active)
            {
                g.setColour(colour.withAlpha(0.20f));
                g.fillEllipse(cx - 13.0f, y - 13.0f, 26.0f, 26.0f);
            }
            g.setColour(juce::Colour(0xff151a21));
            g.fillEllipse(cx - 8.0f, y - 8.0f, 16.0f, 16.0f);
            g.setColour(colour);
            g.drawEllipse(cx - 8.0f, y - 8.0f, 16.0f, 16.0f, active ? 3.0f : 1.6f);
            g.setFont(juce::Font(juce::FontOptions(16.0f, active ? juce::Font::bold : juce::Font::plain)));
            // Labels share one left edge so the column reads aligned.
            g.drawFittedText(labels[row],
                             juce::Rectangle<int>(static_cast<int>(cx + 26), static_cast<int>(y - 11), 88, 22),
                             juce::Justification::centredLeft, 1);
        }
    }
}

void IntervalRail::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(hitTestIntervalCircle(e.position) >= 0
                       ? juce::MouseCursor::PointingHandCursor
                       : juce::MouseCursor::NormalCursor);
}

void IntervalRail::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void IntervalRail::mouseDown(const juce::MouseEvent& e)
{
    const int choice = hitTestIntervalCircle(e.position);
    if (choice < 0)
        return;

    if (auto* p = state.getParameter(nf::params::interval))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(choice)));
        p->endChangeGesture();
    }
}
