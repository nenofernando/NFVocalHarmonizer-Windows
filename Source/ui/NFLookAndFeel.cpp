#include "NFLookAndFeel.h"

NFLookAndFeel::NFLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe8edf4));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0b0e13));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff303844));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffe9eef5));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0c1016));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff394452));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff141923));
    setColour(juce::PopupMenu::textColourId, juce::Colours::white);
}

void NFLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                     float pos, float start, float end, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                         static_cast<float>(w), static_cast<float>(h)).reduced(7.0f);
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = start + pos * (end - start);
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId, true);

    juce::Path arc;
    arc.addCentredArc(centre.x, centre.y, radius + 3.0f, radius + 3.0f, 0.0f, start, end, true);
    g.setColour(juce::Colour(0xff303944));
    g.strokePath(arc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved));
    juce::Path active;
    active.addCentredArc(centre.x, centre.y, radius + 3.0f, radius + 3.0f, 0.0f, start, angle, true);
    g.setColour(accent.withAlpha(0.88f));
    g.strokePath(active, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved));

    juce::ColourGradient metal(juce::Colour(0xfff2f4f5), centre.x - radius, centre.y - radius,
                               juce::Colour(0xff454b52), centre.x + radius, centre.y + radius, false);
    metal.addColour(0.48, juce::Colour(0xff8e969e));
    g.setGradientFill(metal);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colour(0xff11151a));
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);

    juce::Path marker;
    marker.addRoundedRectangle(-1.4f, -radius + 8.0f, 2.8f, radius * 0.42f, 1.4f);
    g.setColour(juce::Colours::white.withAlpha(0.94f));
    g.fillPath(marker, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
}

void NFLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                         bool hover, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const bool on = b.getToggleState();
    if (b.getButtonText() == "POWER")
    {
        auto fill = on ? juce::Colour(0xff1a3a55) : juce::Colour(0xff11161f);
        if (hover) fill = fill.brighter(0.12f);
        if (down) fill = fill.darker(0.18f);
        const auto radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f - 1.0f;
        const auto centre = r.getCentre();
        g.setColour(fill);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(on ? juce::Colour(0xff5fe8ff) : juce::Colour(0xff485462));
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, on ? 1.8f : 1.0f);
        return;
    }

    // ANALYZE visual states (pulse intensity driven by UI timer properties only).
    if (b.getComponentID() == "analyzeButton")
    {
        const int visual = static_cast<int>(b.getProperties().getWithDefault("analysisVisual", 0));
        const float intensity = static_cast<float>(b.getProperties().getWithDefault("analysisPulse", 0.0));
        if (visual == 0)
        {
            auto fill = juce::Colour(0xff11161f);
            if (hover) fill = fill.brighter(0.12f);
            if (down) fill = fill.darker(0.18f);
            g.setColour(fill);
            g.fillRoundedRectangle(r, 5.0f);
            g.setColour(juce::Colour(0xff485462));
            g.drawRoundedRectangle(r, 5.0f, 1.0f);
            return;
        }

        const auto neon = juce::Colour::fromRGB(20, 255, 135);
        const float glow = juce::jlimit(0.0f, 1.0f, intensity);
        // Small controlled halo — no heavy circular shadow, stays within the button area.
        g.setColour(neon.withAlpha(0.10f + 0.14f * glow));
        g.fillRoundedRectangle(r.expanded(1.5f), 6.0f);

        auto fill = juce::Colour(0xff11161f).interpolatedWith(neon, 0.22f + 0.38f * glow);
        if (hover) fill = fill.brighter(0.06f);
        if (down) fill = fill.darker(0.12f);
        g.setColour(fill);
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(neon.withAlpha(0.55f + 0.40f * glow));
        g.drawRoundedRectangle(r, 5.0f, 1.4f + 0.4f * glow);
        return;
    }

    auto fill = on ? juce::Colour(0xff247aff) : juce::Colour(0xff11161f);
    if (hover) fill = fill.brighter(0.12f);
    if (down) fill = fill.darker(0.18f);
    g.setColour(fill);
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(on ? juce::Colour(0xff5fe8ff) : juce::Colour(0xff485462));
    g.drawRoundedRectangle(r, 5.0f, on ? 1.8f : 1.0f);
}

void NFLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    if (b.getButtonText() == "POWER")
    {
        auto bounds = b.getLocalBounds().toFloat().reduced(14.0f);
        const auto colour = b.getToggleState() ? juce::Colour(0xff5fe8ff) : juce::Colour(0xffc6ced8);
        g.setColour(colour);
        g.drawEllipse(bounds, 2.0f);
        g.fillRect(bounds.getCentreX() - 1.2f, bounds.getY() - 3.0f, 2.4f, bounds.getHeight() * 0.55f);
        return;
    }

    // ANALYZE label stays white and readable in every analysis visual state.
    if (b.getComponentID() == "analyzeButton")
    {
        const int visual = static_cast<int>(b.getProperties().getWithDefault("analysisVisual", 0));
        g.setColour(visual != 0 ? juce::Colours::white : juce::Colour(0xffc6ced8));
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawFittedText("ANALYZE", b.getLocalBounds().reduced(5), juce::Justification::centred, 1);
        return;
    }

    g.setColour(b.getToggleState() ? juce::Colours::white : juce::Colour(0xffc6ced8));
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(5), juce::Justification::centred, 1);
}

void NFLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                 int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
    juce::Path arrow;
    arrow.addTriangle(width - 17.0f, height * 0.42f, width - 9.0f, height * 0.42f,
                      width - 13.0f, height * 0.62f);
    g.setColour(juce::Colour(0xff26ddeb)); g.fillPath(arrow);
}

