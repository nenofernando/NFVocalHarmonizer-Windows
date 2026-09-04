#include "StereoMeter.h"
#include <cmath>

void StereoMeter::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0b0f14)); g.fillRoundedRectangle(b, 9.0f);
    g.setColour(juce::Colour(0xff59636e)); g.drawRoundedRectangle(b.reduced(0.8f), 9.0f, 1.2f);
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xffe7ecf1)); g.drawFittedText(title, 0, 10, getWidth(), 25, juce::Justification::centred, 1);
    const auto meterArea = b.reduced(24.0f, 48.0f);
    const float gap = 18.0f;
    const float width = (meterArea.getWidth() - gap) * 0.5f;
    auto draw = [&](float value, float x, const juce::String& label)
    {
        auto track = juce::Rectangle<float>(x, meterArea.getY(), width, meterArea.getHeight());
        g.setColour(juce::Colour(0xff05070a)); g.fillRoundedRectangle(track, 3.0f);
        const float db = juce::Decibels::gainToDecibels(juce::jmax(value, 0.00001f), -60.0f);
        const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto fill = track.withTop(track.getBottom() - track.getHeight() * norm);
        juce::ColourGradient grad(juce::Colour(0xff007b89), fill.getBottomLeft(), juce::Colour(0xff38e5f0), fill.getTopLeft(), false);
        g.setGradientFill(grad); g.fillRoundedRectangle(fill, 2.5f);
        g.setColour(juce::Colour(0xffc7d0d8)); g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawFittedText(label, juce::roundToInt(x), 35, juce::roundToInt(width), 18, juce::Justification::centred, 1);
        g.setColour(juce::Colour(0xff27dce9));
        g.drawFittedText(juce::String(db, 1), juce::roundToInt(x-4), getHeight()-33, juce::roundToInt(width+8), 20, juce::Justification::centred, 1);
    };
    draw(left, meterArea.getX(), "L"); draw(right, meterArea.getX() + width + gap, "R");
}

