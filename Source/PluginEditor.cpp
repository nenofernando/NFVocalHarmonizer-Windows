#include "PluginEditor.h"
#include "dsp/MusicalScale.h"

namespace
{
constexpr auto cyan = 0xff2fe0ee;
constexpr auto violet = 0xffa249ed;

void prepareLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffe2e7ed));
    label.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
}
}

void NFVocalHarmonizerAudioProcessorEditor::FixedCanvas::paint(juce::Graphics& g)
{
    juce::ColourGradient bg(juce::Colour(0xff34383c), 0, 0, juce::Colour(0xff111419), 0,
                            static_cast<float>(designHeight), false);
    g.setGradientFill(bg);
    g.fillAll();
    g.setColour(juce::Colour(0xffaab0b5));
    g.drawRoundedRectangle(juce::Rectangle<float>(0.0f, 0.0f,
                                                  static_cast<float>(designWidth),
                                                  static_cast<float>(designHeight)).reduced(3.0f),
                           9.0f, 2.0f);
    g.setColour(juce::Colour(0xff0a0d11));
    g.fillRect(6, 6, designWidth - 12, 64);
    g.setColour(juce::Colour(0xffeceff2));
    g.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)));
    g.drawFittedText("NF Vocal Harmonizer", 250, 13, designWidth - 500, 33, juce::Justification::centred, 1);
    g.setColour(juce::Colour(0xffb7bec7));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawFittedText("INTELLIGENT VOCAL HARMONY", 250, 44, designWidth - 500, 15, juce::Justification::centred, 1);
    g.setColour(juce::Colour(cyan));
    g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    g.drawText("NF", 26, 16, 42, 25, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffdce1e6));
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.drawText("AUDIO TOOLS", 27, 39, 90, 15, juce::Justification::centredLeft);
}

NFVocalHarmonizerAudioProcessorEditor::NFVocalHarmonizerAudioProcessorEditor(NFVocalHarmonizerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), rail(p.apvts)
{
    setLookAndFeel(&look);

    constrainer.setFixedAspectRatio(static_cast<double>(designWidth) / static_cast<double>(designHeight));
    constrainer.setSizeLimits(820, 548, 1536, 1024);
    setConstrainer(&constrainer);
    setResizable(true, true);
    setSize(designWidth, designHeight);

    addAndMakeVisible(canvas);
    canvas.setInterceptsMouseClicks(false, true);

    configureKnob(harmony, "HARMONY", juce::Colour(cyan), 0, "%");
    configureKnob(formant, "FORMANT", juce::Colour(cyan), 2, " st");
    configureKnob(humanize, "HUMANIZE", juce::Colour(violet), 0, "%");
    configureKnob(width, "WIDTH", juce::Colour(cyan), 0, "%");
    configureKnob(mix, "MIX", juce::Colour(cyan), 0, "%");

    for (juce::Component* c : { static_cast<juce::Component*>(&rail),
                                static_cast<juce::Component*>(&trace),
                                static_cast<juce::Component*>(&inputMeter),
                                static_cast<juce::Component*>(&outputMeter),
                                static_cast<juce::Component*>(&harmony),
                                static_cast<juce::Component*>(&formant),
                                static_cast<juce::Component*>(&humanize),
                                static_cast<juce::Component*>(&width),
                                static_cast<juce::Component*>(&mix) })
        canvas.addAndMakeVisible(c);

    prepareLabel(harmonyLabel, "HARMONY"); prepareLabel(formantLabel, "FORMANT");
    prepareLabel(humanizeLabel, "HUMANIZE"); prepareLabel(widthLabel, "WIDTH"); prepareLabel(mixLabel, "MIX");
    for (auto* l : { &harmonyLabel, &formantLabel, &humanizeLabel, &widthLabel, &mixLabel })
        canvas.addAndMakeVisible(l);

    keyBox.addItemList(nf::params::keyNames(), 1);
    scaleBox.addItemList(nf::params::scaleNames(), 1);
    for (auto* c : { &keyBox, &scaleBox, &presetBox })
        canvas.addAndMakeVisible(c);

    for (auto* b : { &autoKeyButton, &analyzeButton, &harmonizeButton, &powerButton, &prevButton, &nextButton,
                     &aButton, &bButton, &copyButton, &saveButton })
        canvas.addAndMakeVisible(b);
    autoKeyButton.setClickingTogglesState(true);
    harmonizeButton.setClickingTogglesState(true);
    powerButton.setClickingTogglesState(true);
    aButton.setClickingTogglesState(true); bButton.setClickingTogglesState(true);
    aButton.setRadioGroupId(3101); bButton.setRadioGroupId(3101); aButton.setToggleState(true, juce::dontSendNotification);

    harmonizeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff247aff));
    harmonizeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    analyzeButton.onClick = [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)] { if (safe != nullptr) safe->audioProcessor.resetKeyAnalysis(); };
    aButton.onClick = [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)] { if (safe != nullptr) safe->switchAB(true); };
    bButton.onClick = [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)] { if (safe != nullptr) safe->switchAB(false); };
    copyButton.onClick = [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)] {
        if (safe != nullptr) { safe->audioProcessor.captureSlot(safe->activeA); safe->audioProcessor.copySlot(safe->activeA); }
    };
    saveButton.onClick = [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)] { if (safe != nullptr) safe->savePresetAsync(); };
    presetBox.onChange = [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)] {
        if (safe == nullptr || safe->presetBox.getSelectedItemIndex() < 0) return;
        juce::String error;
        if (! safe->audioProcessor.presets.loadPreset(safe->presetBox.getText(), error)) safe->showError(error);
    };
    prevButton.onClick = [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)] {
        if (safe == nullptr || safe->presetBox.getNumItems() == 0) return;
        safe->presetBox.setSelectedItemIndex(juce::jmax(0, safe->presetBox.getSelectedItemIndex() - 1), juce::sendNotification);
    };
    nextButton.onClick = [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)] {
        if (safe == nullptr || safe->presetBox.getNumItems() == 0) return;
        safe->presetBox.setSelectedItemIndex(juce::jmin(safe->presetBox.getNumItems()-1, safe->presetBox.getSelectedItemIndex()+1), juce::sendNotification);
    };

    detectedLabel.setJustificationType(juce::Justification::centred);
    detectedLabel.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    detectedLabel.setColour(juce::Label::textColourId, juce::Colour(cyan));
    canvas.addAndMakeVisible(detectedLabel);

    harmonyAtt = std::make_unique<SliderAttachment>(p.apvts, nf::params::harmony, harmony);
    formantAtt = std::make_unique<SliderAttachment>(p.apvts, nf::params::formant, formant);
    humanizeAtt = std::make_unique<SliderAttachment>(p.apvts, nf::params::humanize, humanize);
    widthAtt = std::make_unique<SliderAttachment>(p.apvts, nf::params::width, width);
    mixAtt = std::make_unique<SliderAttachment>(p.apvts, nf::params::mix, mix);
    autoKeyAtt = std::make_unique<ButtonAttachment>(p.apvts, nf::params::autoKey, autoKeyButton);
    harmonizeAtt = std::make_unique<ButtonAttachment>(p.apvts, nf::params::enabled, harmonizeButton);
    powerAtt = std::make_unique<ButtonAttachment>(p.apvts, nf::params::power, powerButton);
    keyAtt = std::make_unique<ComboAttachment>(p.apvts, nf::params::key, keyBox);
    scaleAtt = std::make_unique<ComboAttachment>(p.apvts, nf::params::scale, scaleBox);

    layoutFixedCanvas();
    refreshPresetList();
    startTimerHz(30);
}

NFVocalHarmonizerAudioProcessorEditor::~NFVocalHarmonizerAudioProcessorEditor()
{
    stopTimer(); fileChooser.reset(); setLookAndFeel(nullptr);
}

void NFVocalHarmonizerAudioProcessorEditor::configureKnob(juce::Slider& s, const juce::String& name,
                                                           juce::Colour colour, int decimals, const juce::String& suffix)
{
    s.setName(name); s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
    s.setNumDecimalPlacesToDisplay(decimals); s.setTextValueSuffix(suffix);
    s.setColour(juce::Slider::rotarySliderFillColourId, colour);
    s.setDoubleClickReturnValue(true, name == "FORMANT" ? 0.0 : name == "MIX" ? 50.0 : name == "HARMONY" ? 70.0 : name == "HUMANIZE" ? 35.0 : 100.0);
}

void NFVocalHarmonizerAudioProcessorEditor::layoutFixedCanvas()
{
    // Layout is locked to the design canvas. Resize only scales this composition.
    canvas.setBounds(0, 0, designWidth, designHeight);

    auto b = juce::Rectangle<int>(0, 0, designWidth, designHeight).reduced(18);
    auto header = b.removeFromTop(44); b.removeFromTop(18);
    powerButton.setBounds(header.removeFromRight(62)); header.removeFromRight(8);
    saveButton.setBounds(header.removeFromRight(54)); copyButton.setBounds(header.removeFromRight(54));
    bButton.setBounds(header.removeFromRight(34)); aButton.setBounds(header.removeFromRight(34));
    nextButton.setBounds(header.removeFromRight(28)); presetBox.setBounds(header.removeFromRight(128)); prevButton.setBounds(header.removeFromRight(28));

    constexpr int meterW = 157;
    auto leftArea = b.removeFromLeft(meterW); b.removeFromLeft(14);
    auto rightArea = b.removeFromRight(meterW); b.removeFromRight(14);
    inputMeter.setBounds(leftArea); outputMeter.setBounds(rightArea);

    auto controls = b.removeFromBottom(154); b.removeFromBottom(8);
    auto top = b.removeFromTop(95); b.removeFromTop(5);
    auto analyze = top.removeFromRight(105); analyzeButton.setBounds(analyze.reduced(4, 28));
    autoKeyButton.setBounds(top.removeFromLeft(90).reduced(4, 28));
    keyBox.setBounds(top.removeFromLeft(72).reduced(3, 28));
    scaleBox.setBounds(top.removeFromLeft(135).reduced(3, 28));
    detectedLabel.setBounds(top.reduced(4, 27));
    trace.setBounds(b.removeFromTop(70)); b.removeFromTop(7);
    harmonizeButton.setBounds(b.removeFromBottom(62).reduced(60, 7));
    rail.setBounds(b.reduced(70, 2));

    const int cell = controls.getWidth() / 5;
    juce::Slider* sliders[] { &harmony, &formant, &humanize, &width, &mix };
    juce::Label* labels[] { &harmonyLabel, &formantLabel, &humanizeLabel, &widthLabel, &mixLabel };
    for (int i = 0; i < 5; ++i)
    {
        auto area = controls.removeFromLeft(i == 4 ? controls.getWidth() : cell);
        labels[i]->setBounds(area.removeFromTop(22));
        sliders[i]->setBounds(area.reduced(8, 0));
    }
}

void NFVocalHarmonizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0b0f14));
}

void NFVocalHarmonizerAudioProcessorEditor::resized()
{
    const auto scaleX = static_cast<float>(getWidth()) / static_cast<float>(designWidth);
    const auto scaleY = static_cast<float>(getHeight()) / static_cast<float>(designHeight);
    const auto scale = juce::jmin(scaleX, scaleY);
    const auto scaledW = static_cast<int>(std::round(static_cast<float>(designWidth) * scale));
    const auto scaledH = static_cast<int>(std::round(static_cast<float>(designHeight) * scale));
    const auto x = (getWidth() - scaledW) / 2;
    const auto y = (getHeight() - scaledH) / 2;

    canvas.setTransform(juce::AffineTransform());
    canvas.setBounds(x, y, designWidth, designHeight);
    canvas.setTransform(juce::AffineTransform::scale(scale));
}

void NFVocalHarmonizerAudioProcessorEditor::timerCallback()
{
    const auto meters = audioProcessor.getMeters();
    inputMeter.setLevels(meters.inputPeakL, meters.inputPeakR); outputMeter.setLevels(meters.outputPeakL, meters.outputPeakR);
    const auto pitch = audioProcessor.getPitchEstimate();
    auto root = juce::roundToInt(audioProcessor.apvts.getRawParameterValue(nf::params::key)->load());
    auto scale = static_cast<nf::dsp::ScaleType>(juce::roundToInt(audioProcessor.apvts.getRawParameterValue(nf::params::scale)->load()));
    const bool autoMode = audioProcessor.apvts.getRawParameterValue(nf::params::autoKey)->load() > 0.5f;
    if (autoMode)
    {
        const auto detected = audioProcessor.getDetectedScale(); root = detected.root; scale = detected.type;
        detectedLabel.setText(nf::params::keyNames()[root] + "  " + (scale == nf::dsp::ScaleType::major ? "MAJOR" : "MINOR"), juce::dontSendNotification);
    }
    else detectedLabel.setText(nf::params::keyNames()[root] + "  " + nf::params::scaleNames()[static_cast<int>(scale)].toUpperCase(),
                               juce::dontSendNotification);
    keyBox.setEnabled(! autoMode); scaleBox.setEnabled(! autoMode); analyzeButton.setEnabled(autoMode);
    const int interval = juce::roundToInt(audioProcessor.apvts.getRawParameterValue(nf::params::interval)->load());
    const float harmonyMidi = pitch.voiced ? nf::dsp::MusicalScale::targetMidi(pitch.midiNote, interval, root, scale) : 0.0f;
    trace.add(pitch.midiNote, harmonyMidi, pitch.voiced);
}

void NFVocalHarmonizerAudioProcessorEditor::refreshPresetList(const juce::String& select)
{
    presetBox.clear(juce::dontSendNotification);
    presetBox.addItemList(audioProcessor.presets.listPresets(), 1);
    if (select.isNotEmpty()) presetBox.setText(select, juce::dontSendNotification);
    else presetBox.setText("Default", juce::dontSendNotification);
}

void NFVocalHarmonizerAudioProcessorEditor::savePresetAsync()
{
    const auto initial = audioProcessor.presets.getPresetDirectory().getChildFile("My Harmony.nfhpreset");
    fileChooser = std::make_unique<juce::FileChooser>("Save NF Vocal Harmonizer Preset", initial, "*.nfhpreset");
    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)](const juce::FileChooser& chooser)
        {
            if (safe == nullptr) return;
            auto file = chooser.getResult();
            if (file == juce::File{}) return;
            file = file.withFileExtension("nfhpreset");
            juce::String error;
            if (! safe->audioProcessor.presets.savePresetToFile(file, error)) safe->showError(error);
            else safe->refreshPresetList(file.getFileNameWithoutExtension());
            juce::MessageManager::callAsync([safe] { if (safe != nullptr) safe->fileChooser.reset(); });
        });
}

void NFVocalHarmonizerAudioProcessorEditor::switchAB(bool useA)
{
    if (activeA == useA) return;
    audioProcessor.captureSlot(activeA); audioProcessor.restoreSlot(useA); activeA = useA;
}

void NFVocalHarmonizerAudioProcessorEditor::showError(const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                            "NF Vocal Harmonizer", message, "OK", this);
}
