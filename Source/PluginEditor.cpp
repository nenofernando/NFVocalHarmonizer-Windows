#include "PluginEditor.h"
#include <cmath>

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
    // Header strip matches layoutFixedCanvas headerHeight (84) + top inset.
    g.setColour(juce::Colour(0xff0a0d11));
    g.fillRect(6, 6, designWidth - 12, 84);

    // Version: footer bottom-right only (never near knob value readouts).
    constexpr float rightMargin = 24.0f;
    constexpr float bottomMargin = 7.0f;
    constexpr float versionWidth = 70.0f;
    constexpr float versionHeight = 14.0f;
    const auto versionBounds = juce::Rectangle<float>(
        static_cast<float>(designWidth) - rightMargin - versionWidth,
        static_cast<float>(designHeight) - bottomMargin - versionHeight,
        versionWidth,
        versionHeight);

    juce::String versionText (JucePlugin_VersionString);
    if (! versionText.startsWithIgnoreCase("v"))
        versionText = "v" + versionText;

    g.setColour(juce::Colours::lightgrey.withAlpha(0.70f));
    g.setFont(juce::Font(juce::FontOptions(9.5f)));
    g.drawText(versionText, versionBounds, juce::Justification::centredRight, false);
}

NFVocalHarmonizerAudioProcessorEditor::NFVocalHarmonizerAudioProcessorEditor(NFVocalHarmonizerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), titleHitZone(*this), rail(p.apvts), noteEditor(p)
{
    setLookAndFeel(&look);

    constrainer.setFixedAspectRatio(static_cast<double>(designWidth) / static_cast<double>(designHeight));
    // Minimum width keeps the fixed 424 px header control cluster uncompressed.
    constrainer.setSizeLimits(820, 602, 1536, 1126);
    setConstrainer(&constrainer);
    setResizable(true, true);
    setSize(defaultWidth, defaultHeight);

    addAndMakeVisible(canvas);
    canvas.setInterceptsMouseClicks(false, true);
    canvas.addAndMakeVisible(titleHitZone);
    titleHitZone.toFront(false);

    titleLabel.setText("NF Vocal Harmonizer", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffeceff2));
    titleLabel.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
    titleLabel.setInterceptsMouseClicks(false, false);
    canvas.addAndMakeVisible(titleLabel);

    subtitleLabel.setText("INTELLIGENT VOCAL HARMONY", juce::dontSendNotification);
    subtitleLabel.setJustificationType(juce::Justification::centred);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb7bec7));
    subtitleLabel.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    subtitleLabel.setInterceptsMouseClicks(false, false);
    canvas.addAndMakeVisible(subtitleLabel);

    configureKnob(harmony, "HARMONY", juce::Colour(cyan), 0, "%");
    configureKnob(formant, "FORMANT", juce::Colour(cyan), 2, " st");
    configureKnob(humanize, "HUMANIZE", juce::Colour(violet), 0, "%");
    configureKnob(width, "WIDTH", juce::Colour(cyan), 0, "%");
    configureKnob(mix, "MIX", juce::Colour(cyan), 0, "%");

    for (juce::Component* c : { static_cast<juce::Component*>(&rail),
                                static_cast<juce::Component*>(&noteEditor),
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

    for (juce::Component* c : { static_cast<juce::Component*>(&autoKeyButton),
                                static_cast<juce::Component*>(&analyzeButton),
                                static_cast<juce::Component*>(&harmonizeButton),
                                static_cast<juce::Component*>(&powerButton),
                                static_cast<juce::Component*>(&prevButton),
                                static_cast<juce::Component*>(&nextButton),
                                static_cast<juce::Component*>(&aButton),
                                static_cast<juce::Component*>(&bButton),
                                static_cast<juce::Component*>(&copyButton),
                                static_cast<juce::Component*>(&saveButton),
                                static_cast<juce::Component*>(&menuButton),
                                static_cast<juce::Component*>(&zoomOutButton),
                                static_cast<juce::Component*>(&zoomInButton) })
        canvas.addAndMakeVisible(c);
    autoKeyButton.setClickingTogglesState(true);
    harmonizeButton.setClickingTogglesState(true);
    powerButton.setClickingTogglesState(true);
    aButton.setClickingTogglesState(true); bButton.setClickingTogglesState(true);
    aButton.setRadioGroupId(3101); bButton.setRadioGroupId(3101); aButton.setToggleState(true, juce::dontSendNotification);

    for (auto* zb : { &zoomOutButton, &zoomInButton })
    {
        zb->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff121820));
        zb->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff1a2430));
        zb->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd7dde4));
        zb->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        zb->setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    }
    zoomOutButton.onClick = [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)]
    {
        if (safe != nullptr)
            safe->noteEditor.zoomTimelineOut();
    };
    zoomInButton.onClick = [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)]
    {
        if (safe != nullptr)
            safe->noteEditor.zoomTimelineIn();
    };

    harmonizeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff247aff));
    harmonizeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    analyzeButton.setComponentID("analyzeButton");
    analyzeButton.setButtonText("ANALYZE");
    analyzeButton.onClick = [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)]
    {
        if (safe == nullptr)
            return;
        if (safe->audioProcessor.isAnalyzeArmed())
        {
            // Finalize capture without zeroing the cursor.
            safe->audioProcessor.finalizeAnalyzeCapture();
        }
        else
        {
            // New analysis may reset the editor cursor; do not finalize-path reset.
            safe->audioProcessor.markAnalysisStartFromCurrentTransport();
            safe->noteEditor.resetCursorForNewAnalysis();
            safe->audioProcessor.beginAnalyzeCapture();
        }
        safe->refreshAnalyzeButtonVisual();
    };
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
    menuButton.onClick = [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)]
    {
        if (safe != nullptr)
            safe->showHeaderMenu();
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
    refreshAnalyzeButtonVisual();
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

    constexpr int headerHeight = 84;
    constexpr int outerMargin = 14;
    constexpr int controlsWidth = 424;
    constexpr int titleControlsGap = 24;

    auto header = juce::Rectangle<int>(0, 6, designWidth, headerHeight);
    auto content = header.reduced(outerMargin, 0);

    // Brand block on the left (replaces NF Audio Tools). Title + subtitle share one centre axis.
    constexpr int brandWidth = 340;
    auto brand = content.removeFromLeft(brandWidth);
    titleLabel.setBounds(brand.getX(), 8, brand.getWidth(), 31);
    subtitleLabel.setBounds(brand.getX(), 40, brand.getWidth(), 16);
    titleLabel.setJustificationType(juce::Justification::centred);
    subtitleLabel.setJustificationType(juce::Justification::centred);

    // Gap before the exclusive control cluster on the right.
    content.removeFromLeft(titleControlsGap);

    // Exclusive controls region — fixed width, left-to-right placement.
    auto controls = content.removeFromRight(controlsWidth);

    constexpr int normalHeight = 30;
    constexpr int powerHeight = 36;
    constexpr int controlsCentreY = 31 + 6; // header origin at y=6

    auto place = [&controls](juce::Component& component, int w, int h = normalHeight)
    {
        auto slot = controls.removeFromLeft(w);
        component.setBounds(slot.getX(), controlsCentreY - h / 2, w, h);
    };
    auto gap = [&controls](int amount) { controls.removeFromLeft(amount); };

    place(prevButton, 26);
    gap(4);
    place(presetBox, 100);
    gap(4);
    place(nextButton, 26);
    gap(10);
    place(aButton, 30);
    gap(4);
    place(bButton, 30);
    gap(8);
    place(copyButton, 46);
    gap(6);
    place(saveButton, 46);
    gap(8);
    place(menuButton, 30);
    gap(10);
    place(powerButton, 36, powerHeight);

    titleHitZone.setBounds(titleLabel.getBounds());
    titleHitZone.toFront(false);

    auto b = juce::Rectangle<int>(0, 0, designWidth, designHeight).reduced(18);
    b.removeFromTop(headerHeight); // below dedicated header

    constexpr int meterW = 157;
    auto leftArea = b.removeFromLeft(meterW); b.removeFromLeft(14);
    auto rightArea = b.removeFromRight(meterW); b.removeFromRight(14);
    inputMeter.setBounds(leftArea); outputMeter.setBounds(rightArea);

    auto knobs = b.removeFromBottom(154); b.removeFromBottom(8);
    // Slightly tighter key/analyze strip so the editor can grow upward.
    auto top = b.removeFromTop(82); b.removeFromTop(4);
    auto analyze = top.removeFromRight(105); analyzeButton.setBounds(analyze.reduced(4, 22));
    autoKeyButton.setBounds(top.removeFromLeft(90).reduced(4, 22));
    keyBox.setBounds(top.removeFromLeft(72).reduced(3, 22));
    scaleBox.setBounds(top.removeFromLeft(135).reduced(3, 22));
    detectedLabel.setBounds(top.reduced(4, 21));
    noteEditor.setBounds(b.removeFromTop(HarmonyNoteEditor::preferredPanelHeight));
    auto zoomRow = b.removeFromTop(24);
    b.removeFromTop(4);
    {
        constexpr int btn = 24;
        auto zoomArea = zoomRow.removeFromRight(btn * 2 + 4);
        zoomOutButton.setBounds(zoomArea.removeFromLeft(btn));
        zoomArea.removeFromLeft(4);
        zoomInButton.setBounds(zoomArea.removeFromLeft(btn));
    }
    harmonizeButton.setBounds(b.removeFromBottom(62).reduced(60, 7));
    rail.setBounds(b.reduced(70, 2));

    const int cell = knobs.getWidth() / 5;
    juce::Slider* sliders[] { &harmony, &formant, &humanize, &width, &mix };
    juce::Label* labels[] { &harmonyLabel, &formantLabel, &humanizeLabel, &widthLabel, &mixLabel };
    for (int i = 0; i < 5; ++i)
    {
        auto area = knobs.removeFromLeft(i == 4 ? knobs.getWidth() : cell);
        labels[i]->setBounds(area.removeFromTop(22));
        sliders[i]->setBounds(area.reduced(8, 0));
    }
}

void NFVocalHarmonizerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0b0f14));
}

void NFVocalHarmonizerAudioProcessorEditor::resetToDefaultSize()
{
    if (getWidth() > defaultWidth + 2 || getHeight() > defaultHeight + 2)
        setSize(defaultWidth, defaultHeight);
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
    auto root = juce::roundToInt(audioProcessor.apvts.getRawParameterValue(nf::params::key)->load());
    auto scale = static_cast<nf::dsp::ScaleType>(juce::roundToInt(audioProcessor.apvts.getRawParameterValue(nf::params::scale)->load()));
    const bool autoMode = audioProcessor.apvts.getRawParameterValue(nf::params::autoKey)->load() > 0.5f;
    if (autoMode)
    {
        const auto detected = audioProcessor.getDetectedScale();
        if (detected.confidence >= 0.20f)
        {
            root = detected.root;
            scale = detected.type;
            detectedLabel.setText(nf::params::keyNames()[root] + "  "
                                      + (scale == nf::dsp::ScaleType::major ? "MAJOR" : "MINOR"),
                                  juce::dontSendNotification);
        }
        else if (detectedLabel.getText().isEmpty()
                 || detectedLabel.getText() == "DETECTING…")
        {
            detectedLabel.setText("DETECTING…", juce::dontSendNotification);
        }
        // else keep last stable key text — don't flicker while confidence is low
    }
    else detectedLabel.setText(nf::params::keyNames()[root] + "  " + nf::params::scaleNames()[static_cast<int>(scale)].toUpperCase(),
                               juce::dontSendNotification);
    keyBox.setEnabled(! autoMode); scaleBox.setEnabled(! autoMode);
    analyzeButton.setEnabled(true);
    analyzeButton.setButtonText("ANALYZE");
    refreshAnalyzeButtonVisual();
}

void NFVocalHarmonizerAudioProcessorEditor::refreshAnalyzeButtonVisual()
{
    const auto state = audioProcessor.getAnalysisState();
    int visual = 0; // idle / failed → original look
    float intensity = 0.0f;

    switch (state)
    {
        case AnalysisState::idle:
        case AnalysisState::failed:
            visual = 0;
            intensity = 0.0f;
            break;
        case AnalysisState::armed:
        case AnalysisState::completed:
            visual = 1; // fixed neon green
            intensity = 0.72f;
            break;
        case AnalysisState::analyzing:
        {
            visual = 2;
            // ~1.5–2 Hz soft pulse (0.0018 * 2π ≈ 1.8 cycles/sec)
            const float phase = std::fmod(static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.0018),
                                         juce::MathConstants<float>::twoPi);
            const float pulse = 0.5f + 0.5f * std::sin(phase);
            intensity = juce::jmap(pulse, 0.45f, 1.0f);
            break;
        }
    }

    analyzeButton.getProperties().set("analysisVisual", visual);
    analyzeButton.getProperties().set("analysisPulse", intensity);
    analyzeButton.repaint();
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

void NFVocalHarmonizerAudioProcessorEditor::openEmbeddedManual(bool portuguese)
{
    const char* resourceName = portuguese ? "NF_Vocal_Harmonizer_Manual_Portugues_pdf"
                                          : "NF_Vocal_Harmonizer_User_Manual_English_pdf";
    int dataSize = 0;
    const char* data = BinaryData::getNamedResource(resourceName, dataSize);
    if (data == nullptr || dataSize <= 0)
    {
        showError("User manual is missing from this build.");
        return;
    }

    const auto fileName = portuguese ? "NF_Vocal_Harmonizer_Manual_Portugues.pdf"
                                     : "NF_Vocal_Harmonizer_User_Manual_English.pdf";
    auto dest = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("NF Audio Tools")
                    .getChildFile("NF Vocal Harmonizer");
    if (! dest.createDirectory() && ! dest.isDirectory())
    {
        showError("Could not create a temporary folder for the manual.");
        return;
    }

    auto pdf = dest.getChildFile(fileName);
    if (! pdf.replaceWithData(data, static_cast<size_t>(dataSize)))
    {
        showError("Could not write the user manual PDF.");
        return;
    }

    if (! pdf.startAsProcess())
        juce::Process::openDocument(pdf.getFullPathName(), {});
}

void NFVocalHarmonizerAudioProcessorEditor::showHeaderMenu()
{
    juce::PopupMenu menu;
    juce::String versionText (JucePlugin_VersionString);
    if (! versionText.startsWithIgnoreCase("v"))
        versionText = "v" + versionText;
    menu.addItem(1, "NF Vocal Harmonizer " + versionText, false, false);
    menu.addSeparator();
    menu.addItem(2, "User Manual (EN)…");
    menu.addItem(3, juce::String::fromUTF8(u8"Manual do usuário (PT)…"));
    menu.addSeparator();
    menu.addItem(4, "Reset window size");
    menu.addItem(5, "About");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(menuButton),
                       [safe = juce::Component::SafePointer<NFVocalHarmonizerAudioProcessorEditor>(this)](int result)
                       {
                           if (safe == nullptr)
                               return;
                           if (result == 2)
                               safe->openEmbeddedManual(false);
                           else if (result == 3)
                               safe->openEmbeddedManual(true);
                           else if (result == 4)
                               safe->resetToDefaultSize();
                           else if (result == 5)
                               juce::AlertWindow::showMessageBoxAsync(
                                   juce::MessageBoxIconType::InfoIcon,
                                   "NF Vocal Harmonizer",
                                   juce::String::fromUTF8(
                                       u8"NF Vocal Harmonizer ")
                                       + juce::String(JucePlugin_VersionString)
                                       + juce::String::fromUTF8(
                                           u8"\nNF Audio Tools / Nenno Fernando\n"
                                           u8"VST3 · AU · AAX\n\n"
                                           u8"© 2026 NF Audio Tools / Nenno Fernando\n"
                                           u8"All rights reserved."),
                                   "OK",
                                   safe.getComponent());
                       });
}
