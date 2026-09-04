#include "PitchTrace.h"

void PitchTrace::add(float v, float h, bool ok)
{
    std::move(voice.begin() + 1, voice.end(), voice.begin());
    std::move(harmony.begin() + 1, harmony.end(), harmony.begin());
    std::move(valid.begin() + 1, valid.end(), valid.begin());
    voice.back() = v; harmony.back() = h; valid.back() = ok; repaint();
}

void PitchTrace::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff080c11)); g.fillRoundedRectangle(r, 6.0f);
    g.setColour(juce::Colour(0xff2a333e)); g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
    g.setColour(juce::Colour(0xff1d2630));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine(juce::roundToInt(r.getY() + r.getHeight() * i / 4.0f), r.getX(), r.getRight());

    auto build = [&](const std::array<float,160>& data, float yBase, juce::Colour colour)
    {
        juce::Path p; bool started = false;
        for (size_t i = 0; i < data.size(); ++i)
        {
            if (! valid[i]) { started = false; continue; }
            const float x = r.getX() + r.getWidth() * static_cast<float>(i) / static_cast<float>(data.size() - 1);
            const float frac = juce::jlimit(-1.0f, 1.0f, (data[i] - 60.0f) / 18.0f);
            const float y = yBase - frac * r.getHeight() * 0.18f;
            if (! started) { p.startNewSubPath(x, y); started = true; } else p.lineTo(x, y);
        }
        g.setColour(colour); g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved));
    };
    build(voice, r.getY() + r.getHeight() * 0.36f, juce::Colour(0xff29e1f2));
    build(harmony, r.getY() + r.getHeight() * 0.72f, juce::Colour(0xffa34bf2));
}

