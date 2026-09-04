#include "IntervalRail.h"

IntervalRail::IntervalRail(juce::AudioProcessorValueTreeState& s) : state(s)
{
    selected.store(juce::roundToInt(state.getRawParameterValue(nf::params::interval)->load()));
    state.addParameterListener(nf::params::interval, this);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
IntervalRail::~IntervalRail() { state.removeParameterListener(nf::params::interval, this); }
void IntervalRail::parameterChanged(const juce::String&, float v) { selected.store(juce::roundToInt(v)); repaint(); }

int IntervalRail::choiceForRow(int row) const
{
    static constexpr int map[9] { 7, 6, 5, 4, -1, 3, 2, 1, 0 };
    return map[juce::jlimit(0, 8, row)];
}

void IntervalRail::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float cx = r.getCentreX();
    const float top = 20.0f, bottom = r.getHeight() - 20.0f;
    const float step = (bottom - top) / 8.0f;
    g.setColour(juce::Colour(0xff75808c)); g.fillRect(cx - 1.0f, top, 2.0f, bottom - top);
    static const juce::StringArray labels { "+8ve", "+6th", "+5th", "+3rd", "VOICE",
                                            "-3rd", "-5th", "-6th", "-8ve" };
    for (int row = 0; row < 9; ++row)
    {
        const float y = top + step * static_cast<float>(row);
        const int choice = choiceForRow(row);
        const bool voice = choice < 0;
        const bool active = choice == selected.load();
        const auto colour = voice ? juce::Colour(0xff38ddff)
                                  : active ? juce::Colour(0xffaa4df4) : juce::Colour(0xffd2d7dd);
        if (voice)
        {
            g.setColour(colour.withAlpha(0.65f)); g.fillRoundedRectangle(cx - 102.0f, y - 1.0f, 204.0f, 2.0f, 1.0f);
            g.setColour(colour.withAlpha(0.25f)); g.fillEllipse(cx - 23.0f, y - 23.0f, 46.0f, 46.0f);
            g.setColour(juce::Colour(0xff10151d)); g.fillEllipse(cx - 16.0f, y - 16.0f, 32.0f, 32.0f);
            g.setColour(colour); g.drawEllipse(cx - 16.0f, y - 16.0f, 32.0f, 32.0f, 3.0f);
            g.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
            g.drawFittedText(labels[row], juce::Rectangle<int>(static_cast<int>(cx - 165), static_cast<int>(y - 12), 95, 24), juce::Justification::centredRight, 1);
        }
        else
        {
            if (active) { g.setColour(colour.withAlpha(0.22f)); g.fillEllipse(cx - 15.0f, y - 15.0f, 30.0f, 30.0f); }
            g.setColour(juce::Colour(0xff151a21)); g.fillEllipse(cx - 8.0f, y - 8.0f, 16.0f, 16.0f);
            g.setColour(colour); g.drawEllipse(cx - 8.0f, y - 8.0f, 16.0f, 16.0f, active ? 3.0f : 1.6f);
            g.setFont(juce::Font(juce::FontOptions(16.0f, active ? juce::Font::bold : juce::Font::plain)));
            g.drawFittedText(labels[row], juce::Rectangle<int>(static_cast<int>(cx + 27), static_cast<int>(y - 11), 85, 23), juce::Justification::centredLeft, 1);
        }
    }
}

void IntervalRail::mouseDown(const juce::MouseEvent& e)
{
    const float top = 20.0f, bottom = static_cast<float>(getHeight()) - 20.0f;
    const int row = juce::jlimit(0, 8, juce::roundToInt((e.position.y - top) / ((bottom - top) / 8.0f)));
    const int choice = choiceForRow(row);
    if (choice < 0) return;
    if (auto* p = state.getParameter(nf::params::interval))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(choice)));
        p->endChangeGesture();
    }
}

