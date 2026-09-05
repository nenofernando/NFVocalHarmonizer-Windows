#include "HarmonyNoteEditor.h"
#include "../PluginProcessor.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace
{
juce::String formatOffsetTooltip(float offsetSemitones)
{
    const int semis = juce::roundToInt(std::floor(offsetSemitones));
    const int cents = juce::roundToInt((offsetSemitones - static_cast<float>(semis)) * 100.0f);
    juce::String text;
    if (semis != 0)
        text << (semis > 0 ? "+" : "") << semis << " st";
    if (cents != 0 || semis == 0)
    {
        if (text.isNotEmpty())
            text << " ";
        text << (cents > 0 ? "+" : "") << cents << " ct";
    }
    return text;
}

juce::String midiNoteDisplayName(float midi) noexcept
{
    static constexpr const char* names[] {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const int m = juce::jlimit(0, 127, juce::roundToInt(midi));
    const int pc = ((m % 12) + 12) % 12;
    const int octave = (m / 12) - 1;
    return juce::String(names[pc]) + juce::String(octave);
}

float smoothstep01(float t) noexcept
{
    t = juce::jlimit(0.0f, 1.0f, t);
    return t * t * (3.0f - 2.0f * t);
}

juce::MouseCursor makePencilToolCursor()
{
    juce::Image img(juce::Image::ARGB, 28, 28, true);
    juce::Graphics g(img);
    g.setColour(juce::Colour(0xfff4f7fa));
    juce::Path shaft;
    shaft.startNewSubPath(6.0f, 22.0f);
    shaft.lineTo(18.0f, 10.0f);
    shaft.lineTo(21.0f, 13.0f);
    shaft.lineTo(9.0f, 25.0f);
    shaft.closeSubPath();
    g.fillPath(shaft);
    g.setColour(juce::Colour(0xff1f9a4a));
    juce::Path tip;
    tip.addTriangle(5.0f, 23.0f, 8.5f, 26.0f, 4.0f, 27.0f);
    g.fillPath(tip);
    g.setColour(juce::Colour(0xff11161f));
    g.strokePath(shaft, juce::PathStrokeType(1.0f));
    return juce::MouseCursor(img, 5, 26);
}

juce::MouseCursor makeScissorsToolCursor()
{
    juce::Image img(juce::Image::ARGB, 28, 28, true);
    juce::Graphics g(img);
    g.setColour(juce::Colour(0xfff4f7fa));
    g.drawLine(8.0f, 6.0f, 20.0f, 22.0f, 2.0f);
    g.drawLine(20.0f, 6.0f, 8.0f, 22.0f, 2.0f);
    g.setColour(juce::Colour(0xff1f9a4a));
    g.drawEllipse(4.0f, 3.0f, 8.0f, 8.0f, 1.6f);
    g.drawEllipse(16.0f, 3.0f, 8.0f, 8.0f, 1.6f);
    return juce::MouseCursor(img, 14, 14);
}
}

HarmonyNoteEditor::HarmonyNoteEditor(NFVocalHarmonizerAudioProcessor& p)
    : processor(p)
{
    setWantsKeyboardFocus(true);
    snapLabel.setText("SNAP", juce::dontSendNotification);
    snapLabel.setJustificationType(juce::Justification::centredRight);
    snapLabel.setColour(juce::Label::textColourId, juce::Colour(0xffc9d0d8));
    snapLabel.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    addAndMakeVisible(snapLabel);

    snapBox.addItem("KEY", 1);
    snapBox.addItem("CHROMATIC", 2);
    snapBox.addItem("OFF", 3);
    snapBox.setSelectedItemIndex(static_cast<int>(processor.noteModel.getSnapMode()), juce::dontSendNotification);
    snapBox.setWantsKeyboardFocus(false); // keep Delete/Backspace on the editor, not the DAW
    snapBox.onChange = [this]
    {
        processor.noteModel.setSnapMode(static_cast<nf::notes::SnapMode>(snapBox.getSelectedItemIndex()));
    };
    addAndMakeVisible(snapBox);

    fitPitchButton.setWantsKeyboardFocus(false);
    fitPitchButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a222c));
    fitPitchButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffc9d0d8));
    fitPitchButton.onClick = [this]
    {
        userNavigatedPitch = false;
        fitPitchToAllNotes();
        repaint();
    };
    addAndMakeVisible(fitPitchButton);

    auto wireTool = [this](juce::TextButton& b, EditTool tool)
    {
        b.setWantsKeyboardFocus(false);
        b.setClickingTogglesState(false);
        b.getProperties().set("nfEditorTool", true);
        b.onClick = [this, tool]
        {
            setEditTool(tool);
        };
        addAndMakeVisible(b);
    };
    wireTool(selectToolButton, EditTool::select);
    wireTool(pencilFlatButton, EditTool::pencilFlat);
    wireTool(pencilSlopeButton, EditTool::pencilSlope);
    wireTool(scissorsButton, EditTool::scissors);
    refreshToolButtonStyles();

    lastIntervalChoice = juce::roundToInt(processor.apvts.getRawParameterValue(nf::params::interval)->load());
    lastPitchFitAnalysisState = processor.getAnalysisState();
    lastPitchFitRevision = processor.getPublishedAnalysisRevision();
    startTimerHz(60);
}

HarmonyNoteEditor::~HarmonyNoteEditor()
{
    stopTimer();
}

void HarmonyNoteEditor::resized()
{
    layout = computeLayout(getLocalBounds().toFloat());
    auto bar = layout.toolbar.toNearestInt();
    fitPitchButton.setBounds(bar.removeFromRight(40).reduced(0, 1));
    bar.removeFromRight(6);
    snapBox.setBounds(bar.removeFromRight(110).reduced(0, 1));
    snapLabel.setBounds(bar.removeFromRight(48).reduced(0, 1));
    bar.removeFromRight(8);
    scissorsButton.setBounds(bar.removeFromRight(40).reduced(0, 1));
    bar.removeFromRight(2);
    pencilSlopeButton.setBounds(bar.removeFromRight(42).reduced(0, 1));
    bar.removeFromRight(2);
    pencilFlatButton.setBounds(bar.removeFromRight(42).reduced(0, 1));
    bar.removeFromRight(2);
    selectToolButton.setBounds(bar.removeFromRight(40).reduced(0, 1));
    timelineView.clampView();
}

void HarmonyNoteEditor::setEditTool(EditTool tool)
{
    cancelPencilGesture();
    editTool = tool;
    refreshToolButtonStyles();
    updateTimelineCursor(getMouseXYRelative().toFloat(), false, false);
    repaint();
}

void HarmonyNoteEditor::refreshToolButtonStyles()
{
    auto style = [this](juce::TextButton& b, EditTool tool)
    {
        const bool on = (editTool == tool);
        b.getProperties().set("nfEditorTool", true);
        b.getProperties().set("nfToolActive", on);
        b.setToggleState(on, juce::dontSendNotification);
        b.repaint();
    };
    style(selectToolButton, EditTool::select);
    style(pencilFlatButton, EditTool::pencilFlat);
    style(pencilSlopeButton, EditTool::pencilSlope);
    style(scissorsButton, EditTool::scissors);
}

float HarmonyNoteEditor::proposedMidiFromY(float y) const
{
    float proposed = yToMidi(y, true);
    const auto ks = keyScale();
    if (fineDrag)
    {
        if (processor.noteModel.getSnapMode() != nf::notes::SnapMode::off)
            proposed = processor.noteModel.quantizeAbsoluteMidi(proposed, ks.first, ks.second);
        else
            proposed = std::round(proposed * 100.0f) / 100.0f;
    }
    else
    {
        proposed = processor.noteModel.quantizeAbsoluteMidi(proposed, ks.first, ks.second);
    }
    return juce::jlimit(0.0f, 127.0f, proposed);
}

float HarmonyNoteEditor::offsetFromProposedMidi(const nf::notes::HarmonyNote& note, float proposedMidi) const
{
    return juce::jlimit(-24.0f, 24.0f, proposedMidi - liveAutoHarmonyMidi(note));
}

void HarmonyNoteEditor::beginPencilGesture(const nf::notes::HarmonyNote& note, juce::Point<float> pos)
{
    selectOnly(note.id);
    pencilNoteId = note.id;
    pencilT0 = juce::jlimit(note.startSec, note.startSec + note.durationSec, timeAtMouseX(pos.x));
    pencilT1 = pencilT0;
    pencilMidi0 = proposedMidiFromY(pos.y);
    pencilMidi1 = pencilMidi0;
    pencilOffset0 = offsetFromProposedMidi(note, pencilMidi0);
    pencilOffset1 = pencilOffset0;
    dragMode = DragMode::pencil;
    dragId = note.id;
    updateTooltip(pencilOffset0);
}

void HarmonyNoteEditor::updatePencilGesture(juce::Point<float> pos, bool fine)
{
    auto* note = processor.noteModel.findNote(pencilNoteId);
    if (note == nullptr)
        return;

    fineDrag = fine;
    pencilT1 = juce::jlimit(note->startSec, note->startSec + note->durationSec, timeAtMouseX(pos.x));
    pencilMidi1 = proposedMidiFromY(pos.y);
    pencilOffset1 = offsetFromProposedMidi(*note, pencilMidi1);

    if (editTool == EditTool::pencilFlat)
    {
        // Flat: pitch follows Y; time spans the note for a full-width preview line.
        pencilT0 = note->startSec;
        pencilT1 = note->startSec + note->durationSec;
        pencilMidi0 = pencilMidi1;
        pencilOffset0 = pencilOffset1;
        tooltipText = formatOffsetTooltip(pencilOffset1);
    }
    else
    {
        tooltipText = formatOffsetTooltip(pencilOffset1);
    }

    pitchView.expandToInclude(pencilMidi0);
    pitchView.expandToInclude(pencilMidi1);
}

void HarmonyNoteEditor::commitPencilGesture()
{
    auto* note = processor.noteModel.findNote(pencilNoteId);
    auto& um = processor.noteModel.getUndoManager();
    if (note != nullptr)
    {
        const float autoBase = liveAutoHarmonyMidi(*note);
        if (editTool == EditTool::pencilFlat)
        {
            processor.noteModel.applyFlatPencilAbsolute(pencilNoteId, pencilMidi1, autoBase, &um);
        }
        else if (editTool == EditTool::pencilSlope)
        {
            processor.noteModel.applySlopePencilAbsolute(pencilNoteId,
                                                        pencilT0, pencilMidi0,
                                                        pencilT1, pencilMidi1,
                                                        autoBase, &um);
        }
    }
    cancelPencilGesture();
}

void HarmonyNoteEditor::cancelPencilGesture()
{
    dragMode = DragMode::none;
    dragId = {};
    pencilNoteId = {};
    tooltipText.clear();
    repaint();
}

void HarmonyNoteEditor::drawPencilPreview(juce::Graphics& g) const
{
    if (dragMode != DragMode::pencil || pencilNoteId.isEmpty())
        return;
    const auto* note = processor.noteModel.findNote(pencilNoteId);
    if (note == nullptr)
        return;

    const float x0 = timeToX(pencilT0, layout.harmonyNotes);
    const float x1 = timeToX(pencilT1, layout.harmonyNotes);
    const float y0 = midiToY(pencilMidi0, true);
    const float y1 = midiToY(pencilMidi1, true);

    g.setColour(juce::Colour(0xfff4f7fa).withAlpha(0.95f));
    g.drawLine(x0, y0, x1, y1, 2.0f);
    g.fillEllipse(x0 - 3.0f, y0 - 3.0f, 6.0f, 6.0f);
    g.fillEllipse(x1 - 3.0f, y1 - 3.0f, 6.0f, 6.0f);
}

PitchEditorLayout HarmonyNoteEditor::computeLayout(juce::Rectangle<float> bounds) const
{
    PitchEditorLayout L;
    auto inner = bounds.reduced(outerPadding);
    L.toolbar = inner.removeFromTop(topToolbarHeight);

    // Two symmetric lanes: header + notes each, with a 1px divider between.
    const float remaining = juce::jmax(1.0f, inner.getHeight());
    const float fixed = laneHeaderHeight * 2.0f + dividerHeight;
    float contentH = (remaining - fixed) * 0.5f;
    // Prefer ~28px when space allows; never collapse below a usable note row.
    contentH = juce::jmax(18.0f, contentH);

    L.voiceHeader = inner.removeFromTop(laneHeaderHeight);
    L.voiceNotes = inner.removeFromTop(contentH);
    L.divider = inner.removeFromTop(dividerHeight);
    L.harmonyHeader = inner.removeFromTop(laneHeaderHeight);
    L.harmonyNotes = inner.removeFromTop(contentH);
    // Any leftover (rounding) stays unused below — keeps lanes equal.
    return L;
}

juce::Rectangle<float> HarmonyNoteEditor::getTimelineLaneBounds() const
{
    return layout.notesUnion();
}

std::pair<int, nf::dsp::ScaleType> HarmonyNoteEditor::keyScale() const
{
    auto root = juce::roundToInt(processor.apvts.getRawParameterValue(nf::params::key)->load());
    auto scale = static_cast<nf::dsp::ScaleType>(juce::roundToInt(processor.apvts.getRawParameterValue(nf::params::scale)->load()));
    if (processor.apvts.getRawParameterValue(nf::params::autoKey)->load() > 0.5f)
    {
        const auto detected = processor.getDetectedScale();
        if (detected.confidence > 0.20f)
        {
            root = detected.root;
            scale = detected.type;
        }
    }
    return { root, scale };
}

float HarmonyNoteEditor::midiToY(float midi, bool harmonyLane) const
{
    // Notes map exclusively into voiceNotes / harmonyNotes — never into headers.
    auto r = harmonyLane ? layout.harmonyNotes : layout.voiceNotes;
    // ≥5 px clearance under the lane header text (header is a separate rect).
    r = r.withTrimmedTop(5.0f).withTrimmedBottom(2.0f);
    const float range = juce::jmax(1.0f, pitchView.viewSpanMidi);
    const float visibleMinimumPitch = pitchView.viewBottomMidi();
    const float normalized = juce::jlimit(
        0.0f,
        1.0f,
        (midi - visibleMinimumPitch) / range);
    // High notes at top; low notes at bottom — same transform for draw + hit-test.
    return r.getBottom() - normalized * juce::jmax(1.0f, r.getHeight());
}

float HarmonyNoteEditor::yToMidi(float y, bool harmonyLane) const
{
    auto r = harmonyLane ? layout.harmonyNotes : layout.voiceNotes;
    r = r.withTrimmedTop(5.0f).withTrimmedBottom(2.0f);
    const float frac = juce::jlimit(0.0f, 1.0f, (r.getBottom() - y) / juce::jmax(1.0f, r.getHeight()));
    return pitchView.viewBottomMidi() + frac * pitchView.viewSpanMidi;
}

void HarmonyNoteEditor::drawPitchGrid(juce::Graphics& g, bool harmonyLane) const
{
    auto r = harmonyLane ? layout.harmonyNotes : layout.voiceNotes;
    r = r.withTrimmedTop(5.0f).withTrimmedBottom(2.0f);
    if (r.getHeight() < 8.0f || r.getWidth() < 8.0f)
        return;

    const float midiTop = pitchView.viewTopMidi;
    const float midiBottom = pitchView.viewBottomMidi();
    const float midiSpan = juce::jmax(1.0f, pitchView.viewSpanMidi);
    const float pxPerSemi = r.getHeight() / midiSpan;

    int step = 1;
    if (pxPerSemi < 2.2f)
        step = 2;
    if (pxPerSemi < 1.2f)
        step = 3;

    const int midiLo = static_cast<int>(std::floor(midiBottom));
    const int midiHi = static_cast<int>(std::ceil(midiTop));
    for (int midi = midiLo; midi <= midiHi; ++midi)
    {
        const bool isOctaveC = (midi % 12) == 0;
        const bool isWholeTone = (midi % 2) == 0;
        if (! isOctaveC && (midi % step) != 0)
            continue;

        const float y = midiToY(static_cast<float>(midi), harmonyLane);
        if (y < r.getY() - 0.5f || y > r.getBottom() + 0.5f)
            continue;

        if (isOctaveC)
        {
            g.setColour(juce::Colour(0xff3a4654));
            g.drawLine(r.getX(), y, r.getRight(), y, 1.0f);
        }
        else if (step == 1 && ! isWholeTone)
        {
            g.setColour(juce::Colour(0xff161c24));
            g.drawLine(r.getX(), y, r.getRight(), y, 0.6f);
        }
        else
        {
            g.setColour(juce::Colour(0xff1e2630));
            g.drawLine(r.getX(), y, r.getRight(), y, 0.8f);
        }
    }
}

juce::Rectangle<float> HarmonyNoteEditor::noteBounds(const nf::notes::HarmonyNote& note, bool harmonyLane) const
{
    // Bounding box of the vocal blob (for hit-test / marquee) — not a MIDI piano-roll tile.
    const auto lane = harmonyLane ? layout.harmonyNotes : layout.voiceNotes;
    const float x0 = timeToX(note.startSec, lane);
    const float x1 = timeToX(note.startSec + note.durationSec, lane);
    const float midi = harmonyLane ? harmonyDisplayMidi(note) : note.voiceMidi;
    const float y = midiToY(midi, harmonyLane);
    const float amp = juce::jlimit(0.08f, 1.0f, note.confidence > 0.05f ? note.confidence : 0.55f);
    const float halfH = 4.0f + amp * 10.0f;
    return juce::Rectangle<float>(x0, y - halfH, juce::jmax(4.0f, x1 - x0), halfH * 2.0f);
}

const std::vector<nf::notes::PitchSample>& HarmonyNoteEditor::activeVizSamples() const noexcept
{
    // While capturing, show live hops; once READY, only the frozen published set.
    if (processor.getAnalysisState() == AnalysisState::capturing)
        return processor.getLiveCaptureVizSamples();
    return processor.getPublishedVizSamples();
}

float HarmonyNoteEditor::amplitudeAtTime(double timeSec, const nf::notes::HarmonyNote& note) const
{
    const auto& samples = activeVizSamples();
    if (samples.empty())
    {
        // Synthetic soft envelope when session has notes but no viz hops.
        const double t0 = note.startSec;
        const double t1 = note.startSec + juce::jmax(1.0e-3, note.durationSec);
        const float u = static_cast<float>((timeSec - t0) / (t1 - t0));
        const float env = smoothstep01(u * 2.5f) * smoothstep01((1.0f - u) * 2.5f);
        return juce::jlimit(0.12f, 1.0f, 0.25f + env * juce::jmax(0.35f, note.confidence));
    }

    float bestAmp = 0.0f;
    double bestDist = 1.0e9;
    for (const auto& s : samples)
    {
        if (s.timeSec < note.startSec - 0.02 || s.timeSec > note.startSec + note.durationSec + 0.02)
            continue;
        const double d = std::abs(s.timeSec - timeSec);
        if (d < bestDist)
        {
            bestDist = d;
            bestAmp = s.amplitude;
        }
    }
    if (bestDist > 0.08)
    {
        const double t0 = note.startSec;
        const double t1 = note.startSec + juce::jmax(1.0e-3, note.durationSec);
        const float u = static_cast<float>((timeSec - t0) / (t1 - t0));
        return 0.2f + 0.55f * smoothstep01(u * 2.0f) * smoothstep01((1.0f - u) * 2.0f);
    }
    // Soft perceptual scale of RMS.
    return juce::jlimit(0.08f, 1.0f, std::sqrt(juce::jmax(0.0f, bestAmp)) * 4.5f);
}

float HarmonyNoteEditor::pitchMidiAtTime(double timeSec, const nf::notes::HarmonyNote& note, bool harmonyLane) const
{
    const float baseHarmony = harmonyDisplayMidi(note);
    const float baseVoice = note.voiceMidi;
    const float intervalShift = baseHarmony - baseVoice;

    const auto& samples = activeVizSamples();
    float midi = harmonyLane ? baseHarmony : baseVoice;
    double bestDist = 1.0e9;
    bool found = false;
    for (const auto& s : samples)
    {
        if (! s.voiced || s.confidence < 0.35f)
            continue;
        if (s.timeSec < note.startSec - 0.01 || s.timeSec > note.startSec + note.durationSec + 0.01)
            continue;
        const double d = std::abs(s.timeSec - timeSec);
        if (d < bestDist)
        {
            bestDist = d;
            midi = harmonyLane ? (s.midi + intervalShift) : s.midi;
            found = true;
        }
    }
    if (! found || bestDist > 0.06)
        return harmonyLane ? baseHarmony : baseVoice;
    return midi;
}

void HarmonyNoteEditor::drawWaveformBackground(juce::Graphics& g, juce::Rectangle<float> area) const
{
    const auto& samples = activeVizSamples();
    if (samples.size() < 2 || area.getWidth() < 8.0f)
        return;

    const double viewStart = displayedVisibleStart;
    const double viewEnd = displayedVisibleStart + timelineView.viewDurationSec;
    juce::Path wave;
    bool started = false;
    const float midY = area.getCentreY();
    const float maxH = area.getHeight() * 0.42f;

    for (const auto& s : samples)
    {
        if (s.timeSec < viewStart - 0.05 || s.timeSec > viewEnd + 0.05)
            continue;
        const float x = timeToX(s.timeSec, area);
        const float amp = juce::jlimit(0.0f, 1.0f, std::sqrt(juce::jmax(0.0f, s.amplitude)) * 4.0f);
        const float h = amp * maxH;
        if (! started)
        {
            wave.startNewSubPath(x, midY - h);
            started = true;
        }
        else
        {
            wave.lineTo(x, midY - h);
        }
    }
    if (! started)
        return;

    // Mirror lower half for a soft stereo-ish band.
    for (int i = static_cast<int>(samples.size()) - 1; i >= 0; --i)
    {
        const auto& s = samples[static_cast<size_t>(i)];
        if (s.timeSec < viewStart - 0.05 || s.timeSec > viewEnd + 0.05)
            continue;
        const float x = timeToX(s.timeSec, area);
        const float amp = juce::jlimit(0.0f, 1.0f, std::sqrt(juce::jmax(0.0f, s.amplitude)) * 4.0f);
        wave.lineTo(x, midY + amp * maxH);
    }
    wave.closeSubPath();

    g.setColour(juce::Colour(0xff1a6a78).withAlpha(0.20f));
    g.fillPath(wave);
}

void HarmonyNoteEditor::drawMidiScaleLabels(juce::Graphics& g) const
{
    auto lane = layout.voiceNotes;
    if (lane.getHeight() < 12.0f)
        return;

    g.setFont(juce::Font(juce::FontOptions(9.5f)));
    const int midiLo = static_cast<int>(std::floor(pitchView.viewBottomMidi()));
    const int midiHi = static_cast<int>(std::ceil(pitchView.viewTopMidi));
    for (int midi = midiLo; midi <= midiHi; ++midi)
    {
        if ((midi % 12) != 0) // C notes only — discrete scale
            continue;
        const float y = midiToY(static_cast<float>(midi), false);
        if (y < lane.getY() - 2.0f || y > lane.getBottom() + 2.0f)
            continue;
        g.setColour(juce::Colour(0xff6a7684));
        g.drawText(midiNoteDisplayName(static_cast<float>(midi)),
                   juce::Rectangle<float>(lane.getX() + 2.0f, y - 7.0f, 28.0f, 14.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);
    }

    // Mirror C labels into harmony lane as well.
    lane = layout.harmonyNotes;
    for (int midi = midiLo; midi <= midiHi; ++midi)
    {
        if ((midi % 12) != 0)
            continue;
        const float y = midiToY(static_cast<float>(midi), true);
        if (y < lane.getY() - 2.0f || y > lane.getBottom() + 2.0f)
            continue;
        g.setColour(juce::Colour(0xff6a7684));
        g.drawText(midiNoteDisplayName(static_cast<float>(midi)),
                   juce::Rectangle<float>(lane.getX() + 2.0f, y - 7.0f, 28.0f, 14.0f).toNearestInt(),
                   juce::Justification::centredLeft, false);
    }
}

void HarmonyNoteEditor::drawVocalBlob(juce::Graphics& g, const nf::notes::HarmonyNote& note,
                                      bool harmonyLane, bool selected) const
{
    const auto lane = harmonyLane ? layout.harmonyNotes : layout.voiceNotes;
    const double t0 = note.startSec;
    const double t1 = note.startSec + juce::jmax(0.02, note.durationSec);
    const int steps = juce::jlimit(6, 48, juce::roundToInt(static_cast<float>((t1 - t0) * 40.0)));

    constexpr float minimumHeight = 3.5f;
    constexpr float amplitudeVisualRange = 11.0f;

    juce::Path blob;
    struct Edge { float x, yTop, yBot, pitchY; };
    std::vector<Edge> edges;
    edges.reserve(static_cast<size_t>(steps + 1));

    for (int i = 0; i <= steps; ++i)
    {
        const double t = t0 + (t1 - t0) * (static_cast<double>(i) / static_cast<double>(steps));
        const float x = timeToX(t, lane);
        const float env = amplitudeAtTime(t, note);
        const float halfHeight = minimumHeight + env * amplitudeVisualRange;
        const float pitchMidi = pitchMidiAtTime(t, note, harmonyLane);
        const float cy = midiToY(pitchMidi, harmonyLane);
        edges.push_back({ x, cy - halfHeight, cy + halfHeight, cy });
    }

    if (edges.size() < 2)
        return;

    blob.startNewSubPath(edges.front().x, edges.front().yTop);
    for (size_t i = 1; i < edges.size(); ++i)
        blob.lineTo(edges[i].x, edges[i].yTop);
    for (int i = static_cast<int>(edges.size()) - 1; i >= 0; --i)
        blob.lineTo(edges[static_cast<size_t>(i)].x, edges[static_cast<size_t>(i)].yBot);
    blob.closeSubPath();

    // Neon VOICE (cyan) / HARMONY (violet) — vivid fill + hot border + bright pitch curve.
    const juce::Colour fill = harmonyLane
                                  ? juce::Colour::fromRGB(196, 64, 255).withAlpha(selected ? 0.72f : 0.52f)
                                  : juce::Colour::fromRGB(0, 240, 255).withAlpha(selected ? 0.68f : 0.48f);
    const juce::Colour border = harmonyLane
                                    ? juce::Colour::fromRGB(232, 140, 255).withAlpha(selected ? 1.0f : 0.82f)
                                    : juce::Colour::fromRGB(120, 255, 255).withAlpha(selected ? 1.0f : 0.78f);
    const juce::Colour curveCol = harmonyLane
                                      ? juce::Colour::fromRGB(255, 210, 255)
                                      : juce::Colour::fromRGB(210, 255, 255);

    g.setColour(fill);
    g.fillPath(blob);
    g.setColour(border);
    g.strokePath(blob, juce::PathStrokeType(selected ? 1.7f : 1.2f));

    // Pitch contour (vibrato / portamento) — visual only.
    juce::Path pitchPath;
    bool started = false;
    float prevY = 0.0f;
    for (const auto& e : edges)
    {
        // Light visual smoothing.
        const float y = started ? (prevY * 0.55f + e.pitchY * 0.45f) : e.pitchY;
        prevY = y;
        if (! started)
        {
            pitchPath.startNewSubPath(e.x, y);
            started = true;
        }
        else
        {
            pitchPath.lineTo(e.x, y);
        }
    }
    g.setColour(curveCol.withAlpha(selected ? 1.0f : 0.90f));
    g.strokePath(pitchPath, juce::PathStrokeType(1.55f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    // Persistent correction guide after FLAT / LINE commit.
    if (harmonyLane && note.isEdited())
    {
        juce::Path guide;
        bool guideStarted = false;
        for (const auto& e : edges)
        {
            if (! guideStarted)
            {
                guide.startNewSubPath(e.x, e.pitchY);
                guideStarted = true;
            }
            else
            {
                guide.lineTo(e.x, e.pitchY);
            }
        }
        g.setColour(juce::Colour::fromRGB(255, 230, 120).withAlpha(selected ? 0.95f : 0.75f));
        g.strokePath(guide, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }
}

void HarmonyNoteEditor::drawSelectionHud(juce::Graphics& g) const
{
    if (selectedIds.empty())
        return;
    const auto* note = processor.noteModel.findNote(selectedIds.front());
    if (note == nullptr)
        return;

    const float voiceMidi = note->voiceMidi;
    const float harmMidi = harmonyDisplayMidi(*note);
    const int cents = juce::roundToInt(note->manualOffsetSemitones * 100.0f);
    juce::String line;
    line << "VOICE: " << midiNoteDisplayName(voiceMidi)
         << "   HARMONY: " << midiNoteDisplayName(harmMidi)
         << "   CORRECTION: " << (cents >= 0 ? "+" : "") << cents << " cents";

    const auto font = juce::Font(juce::FontOptions(12.0f, juce::Font::bold));
    g.setFont(font);
    const float textW = juce::jmax(220.0f, juce::GlyphArrangement::getStringWidth(font, line) + 20.0f);
    const float textH = 22.0f;
    const float tipX = layout.notesUnion().getX() + 8.0f;
    const float tipY = layout.toolbar.getBottom() + 1.0f;
    g.setColour(juce::Colour(0xdd0d1218));
    g.fillRoundedRectangle(tipX, tipY, textW, textH, 4.0f);
    g.setColour(juce::Colour(0xffd7dee6));
    g.drawFittedText(line,
                     juce::roundToInt(tipX), juce::roundToInt(tipY),
                     juce::roundToInt(textW), juce::roundToInt(textH),
                     juce::Justification::centredLeft, 1);
}

int HarmonyNoteEditor::hitTestNote(juce::Point<float> pos) const
{
    const auto& notes = processor.noteModel.getNotes();
    for (int i = static_cast<int>(notes.size()) - 1; i >= 0; --i)
    {
        const auto& n = notes[static_cast<size_t>(i)];
        // Voice + Harmony are the same musical event — either hit selects the pair.
        if (noteBounds(n, true).contains(pos) || noteBounds(n, false).contains(pos))
            return i;
    }
    return -1;
}

bool HarmonyNoteEditor::isSelected(const juce::String& id) const
{
    return std::find(selectedIds.begin(), selectedIds.end(), id) != selectedIds.end();
}

void HarmonyNoteEditor::clearSelection()
{
    selectedIds.clear();
}

void HarmonyNoteEditor::selectOnly(const juce::String& id)
{
    selectedIds = { id };
}

void HarmonyNoteEditor::addToSelection(const juce::String& id)
{
    if (! isSelected(id))
        selectedIds.push_back(id);
}

void HarmonyNoteEditor::toggleSelection(const juce::String& id)
{
    if (isSelected(id))
        selectedIds.erase(std::remove(selectedIds.begin(), selectedIds.end(), id), selectedIds.end());
    else
        selectedIds.push_back(id);
}

void HarmonyNoteEditor::selectAllVisibleNotes()
{
    selectedIds.clear();
    const auto view = layout.notesUnion();
    for (const auto& n : processor.noteModel.getNotes())
    {
        if (noteBounds(n, true).intersects(view) || noteBounds(n, false).intersects(view))
            selectedIds.push_back(n.id);
    }
}

void HarmonyNoteEditor::deleteSelectedNotesFromEditor()
{
    if (selectedIds.empty())
        return; // still consume Delete in keyPressed even when empty

    processor.noteModel.removeNotes(selectedIds, &processor.noteModel.getUndoManager());
    clearSelection();
    dragMode = DragMode::none;
    dragId = {};
    tooltipText.clear();
    repaint();
}

static juce::Rectangle<float> normalisedRect(juce::Rectangle<float> box)
{
    if (box.getWidth() < 0.0f)
    {
        box.setX(box.getRight());
        box.setWidth(std::abs(box.getWidth()));
    }
    if (box.getHeight() < 0.0f)
    {
        box.setY(box.getBottom());
        box.setHeight(std::abs(box.getHeight()));
    }
    return box;
}

void HarmonyNoteEditor::applyMarqueeSelection(bool additive)
{
    if (! additive)
        clearSelection();

    const auto box = normalisedRect(marqueeRect);
    for (const auto& n : processor.noteModel.getNotes())
    {
        const bool hit = noteBounds(n, true).intersects(box) || noteBounds(n, false).intersects(box);
        if (hit)
            addToSelection(n.id);
    }
}

void HarmonyNoteEditor::beginMarquee(juce::Point<float> origin, bool additive)
{
    dragMode = DragMode::marquee;
    shiftMarqueeAdditive = additive;
    marqueeOrigin = origin;
    marqueeRect = juce::Rectangle<float>(origin, origin);
    pendingEmptyGesture = false;
    dragId = {};
    pitchDragNotes.clear();
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

void HarmonyNoteEditor::capturePitchDragSnapshot()
{
    pitchDragNotes.clear();
    for (const auto& id : selectedIds)
    {
        auto* note = processor.noteModel.findNote(id);
        if (note == nullptr)
            continue;
        pitchDragNotes.push_back({ id, harmonyDisplayMidi(*note), note->manualOffsetSemitones });
    }

    // Fallback: primary drag note only (should already be in selection).
    if (pitchDragNotes.empty() && ! dragId.isEmpty())
    {
        if (auto* note = processor.noteModel.findNote(dragId))
            pitchDragNotes.push_back({ dragId, harmonyDisplayMidi(*note), note->manualOffsetSemitones });
    }
}

void HarmonyNoteEditor::applyPitchDragToSnapshot(float primaryProposedMidi)
{
    if (pitchDragNotes.empty())
        return;

    float primaryStart = dragStartMidi;
    for (const auto& snap : pitchDragNotes)
    {
        if (snap.id == dragId)
        {
            primaryStart = snap.startEditedMidi;
            break;
        }
    }
    const float deltaMidi = primaryProposedMidi - primaryStart;

    for (const auto& snap : pitchDragNotes)
    {
        auto* note = processor.noteModel.findNote(snap.id);
        if (note == nullptr)
            continue;
        const float newAbs = snap.startEditedMidi + deltaMidi;
        note->manualOffsetSemitones = newAbs - liveAutoHarmonyMidi(*note);
        pitchView.expandToInclude(newAbs);
        pitchView.expandToInclude(note->voiceMidi);
    }
    processor.noteModel.republishOffsets();
}

float HarmonyNoteEditor::liveAutoHarmonyMidi(const nf::notes::HarmonyNote& note) const
{
    const auto ks = keyScale();
    const int interval = juce::roundToInt(processor.apvts.getRawParameterValue(nf::params::interval)->load());
    return nf::dsp::MusicalScale::targetMidi(note.voiceMidi, interval, ks.first, ks.second);
}

float HarmonyNoteEditor::harmonyDisplayMidi(const nf::notes::HarmonyNote& note) const
{
    // Match live DSP: interval target + flat offset or curve midpoint.
    const double midT = note.startSec + note.durationSec * 0.5;
    return liveAutoHarmonyMidi(note) + note.offsetAt(midT);
}

void HarmonyNoteEditor::fitPitchToAllNotes()
{
    applyAutoFitPitchVertical(true);
}

void HarmonyNoteEditor::applyAutoFitPitchVertical(bool allowShrink)
{
    const auto& notes = processor.noteModel.getNotes();
    if (notes.empty())
    {
        if (allowShrink)
            pitchView.setContentMidiRange(48.0f, 84.0f);
        pitchView.fitContent(nf::notes::PitchViewState::pitchMarginSemitones);
        return;
    }

    float minimumPitch = std::numeric_limits<float>::max();
    float maximumPitch = std::numeric_limits<float>::lowest();
    for (const auto& n : notes)
    {
        minimumPitch = std::min(minimumPitch, n.voiceMidi);
        maximumPitch = std::max(maximumPitch, n.voiceMidi);
        const float harm = harmonyDisplayMidi(n);
        minimumPitch = std::min(minimumPitch, harm);
        maximumPitch = std::max(maximumPitch, harm);
    }

    if (! allowShrink)
    {
        minimumPitch = std::min(minimumPitch, pitchView.contentMinMidi);
        maximumPitch = std::max(maximumPitch, pitchView.contentMaxMidi);
        pitchView.setContentMidiRange(minimumPitch, maximumPitch);
        // Expand viewport only — never jump/shrink while capturing.
        pitchView.expandToInclude(minimumPitch);
        pitchView.expandToInclude(maximumPitch);
        return;
    }

    pitchView.setContentMidiRange(minimumPitch, maximumPitch);
    pitchView.fitContent(nf::notes::PitchViewState::pitchMarginSemitones);
    lastFittedMidiMin = minimumPitch;
    lastFittedMidiMax = maximumPitch;
}

void HarmonyNoteEditor::refreshContentRangeFromNotes()
{
    const auto& notes = processor.noteModel.getNotes();
    const auto analysisState = processor.getAnalysisState();
    const auto revision = processor.getPublishedAnalysisRevision();
    const int interval = juce::roundToInt(processor.apvts.getRawParameterValue(nf::params::interval)->load());

    const bool finishedAnalysis = (lastPitchFitAnalysisState == AnalysisState::capturing
                                   && analysisState == AnalysisState::ready)
                                  || (revision != lastPitchFitRevision && analysisState == AnalysisState::ready);
    const bool intervalChanged = (lastIntervalChoice >= 0 && interval != lastIntervalChoice);
    lastPitchFitAnalysisState = analysisState;
    lastPitchFitRevision = revision;
    lastIntervalChoice = interval;

    if (notes.empty())
    {
        // Avoid clamp churn every timer tick when nothing changed.
        if (lastFittedNoteCount != 0
            || std::abs(timelineView.contentStartSec) > 1.0e-9
            || std::abs(timelineView.contentEndSec - 8.0) > 1.0e-9)
        {
            timelineView.setContentRange(0.0, 8.0);
            pitchView.setContentMidiRange(48.0f, 84.0f);
        }
        const bool identityChanged = lastFittedNoteCount != 0;
        lastFittedNoteCount = 0;
        lastFittedContentStart = 0.0;
        lastFittedContentEnd = 8.0;
        lastFittedMidiMin = 48.0f;
        lastFittedMidiMax = 84.0f;
        if (identityChanged)
        {
            userNavigatedTimeline = false;
            userNavigatedPitch = false;
            fitViewsToNewNotesIfNeeded(true);
        }
        return;
    }

    double start = notes.front().startSec;
    double end = notes.front().startSec + notes.front().durationSec;
    float midiMin = juce::jmin(notes.front().voiceMidi, harmonyDisplayMidi(notes.front()));
    float midiMax = juce::jmax(notes.front().voiceMidi, harmonyDisplayMidi(notes.front()));
    for (const auto& n : notes)
    {
        start = juce::jmin(start, n.startSec);
        end = juce::jmax(end, n.startSec + n.durationSec);
        const float harm = harmonyDisplayMidi(n);
        midiMin = juce::jmin(midiMin, n.voiceMidi, harm);
        midiMax = juce::jmax(midiMax, n.voiceMidi, harm);
    }
    start = juce::jmax(0.0, start - 0.05);
    end = end + 0.25;

    // Only ANALYZE / note-set timing changes reset the horizontal camera.
    const bool noteSetChanged = notes.size() != lastFittedNoteCount
                                || std::abs(start - lastFittedContentStart) > 1.0e-3
                                || std::abs(end - lastFittedContentEnd) > 1.0e-3;
    // Appended capture further ahead: keep camera / prior notes intact.
    const bool appendedSection = notes.size() > lastFittedNoteCount
                                 && lastFittedNoteCount > 0
                                 && std::abs(start - lastFittedContentStart) < 0.05
                                 && end > lastFittedContentEnd + 0.02;

    const bool timeRangeChanged = std::abs(start - timelineView.contentStartSec) > 1.0e-6
                                  || std::abs(end - timelineView.contentEndSec) > 1.0e-6;
    const bool midiRangeChanged = std::abs(midiMin - pitchView.contentMinMidi) > 0.02f
                                  || std::abs(midiMax - pitchView.contentMaxMidi) > 0.02f;

    if (timeRangeChanged)
        timelineView.setContentRange(start, end);

    if (analysisState == AnalysisState::capturing)
    {
        // While capturing: content may grow; never shrink the vertical window.
        if (midiRangeChanged || noteSetChanged)
            applyAutoFitPitchVertical(false);
    }
    else if (appendedSection)
    {
        // New bars ahead — expand pitch range only; do not recentre or wipe prior view.
        pitchView.setContentMidiRange(midiMin, midiMax);
        applyAutoFitPitchVertical(false);
        followPlayhead = true;
    }
    else if (finishedAnalysis || intervalChanged || noteSetChanged)
    {
        // Finalize / interval / new map: definitive fit so grave notes stay visible.
        userNavigatedPitch = false;
        pitchView.setContentMidiRange(midiMin, midiMax);
        applyAutoFitPitchVertical(true);
    }
    else if (midiRangeChanged)
    {
        pitchView.setContentMidiRange(midiMin, midiMax);
        // Edits that leave the window: expand (TEST E). Full shrink only via FIT.
        if (midiMin < pitchView.viewBottomMidi() + 0.5f || midiMax > pitchView.viewTopMidi - 0.5f)
            applyAutoFitPitchVertical(true);
    }

    if (noteSetChanged)
    {
        lastFittedNoteCount = notes.size();
        lastFittedContentStart = start;
        lastFittedContentEnd = end;
        lastFittedMidiMin = midiMin;
        lastFittedMidiMax = midiMax;
        if (! appendedSection)
        {
            userNavigatedTimeline = false;
            followPlayhead = true;
            fitViewsToNewNotesIfNeeded(true);
        }
        else
        {
            timelineView.clampView();
        }
    }
}

void HarmonyNoteEditor::fitViewsToNewNotesIfNeeded(bool notesIdentityChanged)
{
    if (! notesIdentityChanged)
        return;

    if (! userNavigatedTimeline)
    {
        // First page (~8 s), not the whole song — otherwise pan/follow cannot work.
        timelineView.viewStartSec = timelineView.contentStartSec;
        timelineView.viewDurationSec = juce::jmin(8.0, timelineView.maxVisibleSec());
        timelineView.clampView();
        syncDisplayedVisibleStartFromView();
    }

    if (! userNavigatedPitch)
        applyAutoFitPitchVertical(true);
}

void HarmonyNoteEditor::updateContinuousFollow()
{
    if (! followPlayhead || ! processor.isHostPlaying())
        return;

    const double playSec = getDisplayPlayheadSeconds();
    const double viewportSec = juce::jmax(1.0e-9, timelineView.viewDurationSec);
    constexpr double playheadAnchor = 0.45;

    // Keep playhead visually at 45%; scroll content continuously underneath.
    const double targetStart = playSec - viewportSec * playheadAnchor;
    const double maximumStart = juce::jmax(timelineView.contentStartSec,
                                           timelineView.contentEndSec - viewportSec);

    displayedVisibleStart = juce::jlimit(timelineView.contentStartSec, maximumStart, targetStart);
    applyDisplayedVisibleStartToView();
}

void HarmonyNoteEditor::syncDisplayedVisibleStartFromView() noexcept
{
    displayedVisibleStart = timelineView.viewStartSec;
}

void HarmonyNoteEditor::applyDisplayedVisibleStartToView() noexcept
{
    timelineView.viewStartSec = displayedVisibleStart;
    timelineView.clampView();
    displayedVisibleStart = timelineView.viewStartSec;
}

float HarmonyNoteEditor::timeToX(double timeSec, juce::Rectangle<float> lane) const noexcept
{
    const double dur = juce::jmax(1.0e-9, timelineView.viewDurationSec);
    return lane.getX()
           + static_cast<float>((timeSec - displayedVisibleStart) / dur) * lane.getWidth();
}

void HarmonyNoteEditor::showZoomHud()
{
    zoomHudUntilMs = juce::Time::getMillisecondCounter() + 700;
}

void HarmonyNoteEditor::zoomTimelineAtMouse(float deltaY, float mouseX)
{
    const auto lane = getTimelineLaneBounds();
    if (lane.getWidth() <= 1.0f)
        return;
    const double fraction = juce::jlimit(0.0, 1.0, static_cast<double>((mouseX - lane.getX()) / lane.getWidth()));
    timelineView.zoomAtFraction(deltaY, fraction);
    userNavigatedTimeline = true;
    followPlayhead = false;
    syncDisplayedVisibleStartFromView();
    showZoomHud();
}

void HarmonyNoteEditor::zoomTimelineIn()
{
    const auto lane = getTimelineLaneBounds();
    zoomTimelineAtMouse(0.45f, lane.getCentreX());
    repaint();
}

void HarmonyNoteEditor::zoomTimelineOut()
{
    const auto lane = getTimelineLaneBounds();
    zoomTimelineAtMouse(-0.45f, lane.getCentreX());
    repaint();
}

void HarmonyNoteEditor::zoomPitchAtMouse(float deltaY, float mouseY)
{
    const auto lane = layout.notesUnion();
    if (lane.getHeight() <= 1.0f)
        return;
    auto r = lane.withTrimmedTop(5.0f).withTrimmedBottom(2.0f);
    const float frac = juce::jlimit(0.0f, 1.0f, (mouseY - r.getY()) / juce::jmax(1.0f, r.getHeight()));
    pitchView.zoomAtFraction(deltaY, frac);
    userNavigatedPitch = true;
    showZoomHud();
}

void HarmonyNoteEditor::panTimelineFromWheel(float deltaY)
{
    timelineView.panFromWheel(deltaY);
    userNavigatedTimeline = true;
    followPlayhead = false;
    syncDisplayedVisibleStartFromView();
}

void HarmonyNoteEditor::panPitchFromWheel(float deltaY)
{
    pitchView.panFromWheel(deltaY);
    userNavigatedPitch = true;
}

void HarmonyNoteEditor::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (! layout.notesUnion().contains(event.position))
    {
        juce::Component::mouseWheelMove(event, wheel);
        return;
    }

    // macOS Shift+scroll often remaps into deltaX; prefer the dominant axis.
    float delta = wheel.deltaY;
    if (std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
        delta = wheel.deltaX;
    if (! std::isfinite(delta) || std::abs(delta) < 0.008f)
        return;

    // Inertial coasting after lift makes zoom/pan feel like it fights the user.
    if (wheel.isInertial)
        return;

    // Cmd/Ctrl + scroll = pitch zoom (zoom into low/high notes).
    // Shift + scroll = pitch pan.
    // Alt/Option + scroll = time pan.
    // Plain scroll = time zoom.
    if (event.mods.isCommandDown() || event.mods.isCtrlDown())
        zoomPitchAtMouse(delta, event.position.y);
    else if (event.mods.isShiftDown())
        panPitchFromWheel(delta);
    else if (event.mods.isAltDown())
        panTimelineFromWheel(delta);
    else
        zoomTimelineAtMouse(delta, event.position.x);

    repaint();
}

void HarmonyNoteEditor::paint(juce::Graphics& g)
{
    layout = computeLayout(getLocalBounds().toFloat());

    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff080c11));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colour(0xff2a333e));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    // Fixed lane headers — never share vertical space with note blocks.
    g.setFont(juce::Font(juce::FontOptions(13.5f, juce::Font::bold)));
    g.setColour(juce::Colour::fromRGB(0, 240, 255));
    g.drawFittedText("VOICE",
                     layout.voiceHeader.toNearestInt().withTrimmedLeft(2),
                     juce::Justification::centredLeft, 1);
    g.setColour(juce::Colour::fromRGB(196, 64, 255));
    g.drawFittedText("HARMONY",
                     layout.harmonyHeader.toNearestInt().withTrimmedLeft(2),
                     juce::Justification::centredLeft, 1);

    // Discrete horizontal divider between VOICE and HARMONY.
    g.setColour(juce::Colour(0xff1d2630));
    g.fillRect(layout.divider);

    // Semitone / pitch grid behind note blocks (meio tom when the lane is tall enough).
    {
        juce::Graphics::ScopedSaveState clipNotes(g);
        g.reduceClipRegion(layout.notesUnion().toNearestInt());
        drawPitchGrid(g, false);
        drawPitchGrid(g, true);
        drawWaveformBackground(g, layout.notesUnion());
        drawMidiScaleLabels(g);
    }

    const auto notesArea = layout.notesUnion();
    if (processor.noteModel.getNotes().empty())
    {
        g.setColour(juce::Colour(0xff8b949e));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawFittedText("Press ANALYZE, play the DAW, then stop - purple HARMONY blobs become editable",
                         notesArea.reduced(24.0f, 4.0f).toNearestInt(), juce::Justification::centred, 2);
    }

    {
        juce::Graphics::ScopedSaveState clipNotes(g);
        g.reduceClipRegion(notesArea.toNearestInt());
        for (const auto& note : processor.noteModel.getNotes())
        {
            const bool selected = isSelected(note.id);
            drawVocalBlob(g, note, false, selected); // VOICE — never erased by harmony edits
            drawVocalBlob(g, note, true, selected);  // HARMONY
        }
    }

    drawSelectionHud(g);
    drawPencilPreview(g);

    // Active tool badge inside the note area (visual only).
    if (editTool == EditTool::pencilFlat || editTool == EditTool::pencilSlope
        || editTool == EditTool::scissors)
    {
        const juce::String badge = (editTool == EditTool::scissors) ? "CUT"
                                 : (editTool == EditTool::pencilFlat) ? "FLAT" : "LINE";
        const auto font = juce::Font(juce::FontOptions(11.0f, juce::Font::bold));
        const float bw = juce::GlyphArrangement::getStringWidth(font, badge) + 16.0f;
        const float bx = notesArea.getRight() - bw - 6.0f;
        const float by = notesArea.getY() + 4.0f;
        g.setColour(juce::Colour(0xff1f9a4a).withAlpha(0.92f));
        g.fillRoundedRectangle(bx, by, bw, 18.0f, 4.0f);
        g.setColour(juce::Colour(0xfff2fff6));
        g.setFont(font);
        g.drawFittedText(badge, juce::Rectangle<int>(juce::roundToInt(bx), juce::roundToInt(by),
                                                     juce::roundToInt(bw), 18),
                         juce::Justification::centred, 1);

        // Small pencil / scissors glyph next to the badge.
        const float ix = bx - 20.0f;
        const float iy = by + 2.0f;
        g.setColour(juce::Colour(0xff7dffa8));
        if (editTool == EditTool::scissors)
        {
            g.drawLine(ix + 4.0f, iy + 2.0f, ix + 14.0f, iy + 14.0f, 1.6f);
            g.drawLine(ix + 14.0f, iy + 2.0f, ix + 4.0f, iy + 14.0f, 1.6f);
            g.drawEllipse(ix + 1.0f, iy, 6.0f, 6.0f, 1.2f);
            g.drawEllipse(ix + 11.0f, iy, 6.0f, 6.0f, 1.2f);
        }
        else
        {
            g.drawLine(ix + 3.0f, iy + 13.0f, ix + 13.0f, iy + 3.0f, 2.0f);
            g.fillEllipse(ix + 1.0f, iy + 12.0f, 4.0f, 4.0f);
        }
    }

    if (dragMode == DragMode::marquee)
    {
        const auto box = normalisedRect(marqueeRect);
        g.setColour(juce::Colour(0x552fe0ee));
        g.fillRect(box);
        g.setColour(juce::Colour(0xcc2fe0ee));
        g.drawRect(box, 1.0f);
    }

    const double play = getDisplayPlayheadSeconds();
    const float px = timeToX(play, notesArea);
    if (px >= notesArea.getX() && px <= notesArea.getRight())
    {
        const bool frozen = ! processor.isHostPlaying();
        g.setColour(juce::Colour(0xffedf1f4).withAlpha(frozen ? 1.0f : 0.8f));
        g.drawLine(px, notesArea.getY(), px, notesArea.getBottom(), 1.6f);

        // Persistent cursor marker (spec): small cyan triangle at the top.
        g.setColour(juce::Colour::fromRGB(40, 224, 238));
        juce::Path triangle;
        triangle.addTriangle(px - 4.0f, notesArea.getY(),
                             px + 4.0f, notesArea.getY(),
                             px, notesArea.getY() + 6.0f);
        g.fillPath(triangle);
    }

    if (tooltipText.isNotEmpty()
        && (dragMode == DragMode::pitchEdit || dragMode == DragMode::pencil
            || ! selectedIds.empty()))
    {
        const auto font = juce::Font(juce::FontOptions(20.0f, juce::Font::bold));
        g.setFont(font);
        const float textW = juce::jmax(72.0f, juce::GlyphArrangement::getStringWidth(font, tooltipText) + 24.0f);
        const float textH = 30.0f;
        const float tipX = bounds.getCentreX() - textW * 0.5f;
        const float tipY = layout.toolbar.getBottom() + 2.0f;
        g.setColour(juce::Colour(0xee11161f));
        g.fillRoundedRectangle(tipX, tipY, textW, textH, 6.0f);
        g.setColour(juce::Colours::white);
        g.drawFittedText(tooltipText,
                         juce::roundToInt(tipX),
                         juce::roundToInt(tipY),
                         juce::roundToInt(textW),
                         juce::roundToInt(textH),
                         juce::Justification::centred, 1);
    }

    if (juce::Time::getMillisecondCounter() < zoomHudUntilMs)
    {
        const int pct = juce::roundToInt(timelineView.zoomPercent());
        const juce::String hud = juce::String(pct) + "%";
        g.setColour(juce::Colour(0xaa0b1016));
        g.fillRoundedRectangle(bounds.getRight() - 64.0f, bounds.getBottom() - 22.0f, 54.0f, 16.0f, 3.0f);
        g.setColour(juce::Colour(0xffc9d0d8));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawFittedText(hud, juce::roundToInt(bounds.getRight() - 64.0f), juce::roundToInt(bounds.getBottom() - 22.0f),
                         54, 16, juce::Justification::centred, 1);
    }
}

void HarmonyNoteEditor::timerCallback()
{
    // Don't fight the camera while the user is actively dragging the view/notes.
    if (dragMode != DragMode::pan && dragMode != DragMode::pitchPan
        && dragMode != DragMode::pitchEdit)
        refreshContentRangeFromNotes();

    // Drop selection ids that no longer exist (after delete / ANALYZE rebuild).
    selectedIds.erase(std::remove_if(selectedIds.begin(), selectedIds.end(),
                                     [this](const juce::String& id) { return processor.noteModel.findNote(id) == nullptr; }),
                      selectedIds.end());

    const double pending = processor.takePendingEditorCursorSeconds();
    if (pending >= 0.0)
        restoreCursorFromState(pending);

    updatePlayhead();
}

int64_t HarmonyNoteEditor::getAnalysisLengthSamples() const noexcept
{
    const auto transport = processor.getTransportSnapshot();
    const double sr = juce::jmax(1.0, transport.sampleRate);
    const double endSec = juce::jmax(timelineView.contentEndSec, timelineView.contentStartSec + 0.25);
    const int64_t endSample = static_cast<int64_t>(std::llround(endSec * sr));
    const int64_t relativeEnd = endSample - transport.analysisStartHostSample;
    return juce::jmax<int64_t>(1, relativeEnd);
}

void HarmonyNoteEditor::updatePlayhead()
{
    const auto transport = processor.getTransportSnapshot();
    const bool isPlayingNow = transport.isPlaying;
    const int64_t analysisLength = getAnalysisLengthSamples();

    if (isPlayingNow)
    {
        // Accept host positions ONLY while playing.
        if (transport.hasValidHostPosition)
        {
            const int64_t relativeSample = transport.currentHostSample
                                         - transport.analysisStartHostSample;

            lastValidPlayingSample = juce::jlimit<int64_t>(0, analysisLength, relativeSample);
            displayedPlayheadSample = lastValidPlayingSample;
        }

        if (! wasPlaying)
            followPlayhead = true; // re-arm continuous follow on each Play

        if (followPlayhead)
            updateContinuousFollow();
    }
    else
    {
        if (wasPlaying)
        {
            // PLAY → STOP: capture last valid position; prefer audio-thread freeze if newer.
            const int64_t fromAudio = juce::jlimit<int64_t>(
                0,
                analysisLength,
                transport.lastValidPlayingSample - transport.analysisStartHostSample);
            frozenStopSample = juce::jmax(lastValidPlayingSample, fromAudio);
            // Freeze viewport — do not move displayedVisibleStart / recentre / zero.
            syncDisplayedVisibleStartFromView();
        }

        // While stopped, ignore host position completely (DAW may return sample 0).
        displayedPlayheadSample = frozenStopSample;
    }

    wasPlaying = isPlayingNow;
    processor.setEditorCursorSecondsForState(getCursorSecondsForState());
    repaint();
}

void HarmonyNoteEditor::resetCursorForNewAnalysis() noexcept
{
    displayedPlayheadSample = 0;
    lastValidPlayingSample = 0;
    frozenStopSample = 0;
    wasPlaying = false;
    followPlayhead = true;
    displayedVisibleStart = timelineView.contentStartSec;
    applyDisplayedVisibleStartToView();
    processor.setEditorCursorSecondsForState(0.0);
    repaint();
}

void HarmonyNoteEditor::restoreCursorFromState(double seconds) noexcept
{
    const auto transport = processor.getTransportSnapshot();
    const double sr = juce::jmax(1.0, transport.sampleRate);
    const int64_t absolute = static_cast<int64_t>(std::llround(seconds * sr));
    const int64_t relative = juce::jlimit<int64_t>(
        0, getAnalysisLengthSamples(), absolute - transport.analysisStartHostSample);
    displayedPlayheadSample = relative;
    lastValidPlayingSample = relative;
    frozenStopSample = relative;
    processor.setEditorCursorSecondsForState(getCursorSecondsForState());
    repaint();
}

double HarmonyNoteEditor::getCursorSecondsForState() const noexcept
{
    const auto transport = processor.getTransportSnapshot();
    const double sr = juce::jmax(1.0, transport.sampleRate);
    // Always from frozen/displayed sample — never from transport.currentHostSample while stopped.
    return static_cast<double>(transport.analysisStartHostSample + displayedPlayheadSample) / sr;
}

void HarmonyNoteEditor::setPlayheadFromX(float mouseX)
{
    const double t = timeAtMouseX(mouseX);
    const auto transport = processor.getTransportSnapshot();
    const double sr = juce::jmax(1.0, transport.sampleRate);
    const int64_t absolute = static_cast<int64_t>(std::llround(t * sr));
    const int64_t clickedSample = juce::jlimit<int64_t>(
        0, getAnalysisLengthSamples(), absolute - transport.analysisStartHostSample);

    displayedPlayheadSample = clickedSample;
    lastValidPlayingSample = clickedSample;
    frozenStopSample = clickedSample;

    processor.setEditorCursorSecondsForState(getCursorSecondsForState());
    userNavigatedTimeline = true;

    // Keep clicked point on-screen while stopped without following host.
    if (! processor.isHostPlaying())
    {
        const double playSec = getDisplayPlayheadSeconds();
        const double viewportSec = juce::jmax(1.0e-9, timelineView.viewDurationSec);
        if (playSec < displayedVisibleStart || playSec > displayedVisibleStart + viewportSec)
        {
            displayedVisibleStart = playSec - viewportSec * 0.45;
            applyDisplayedVisibleStartToView();
        }
    }
}

double HarmonyNoteEditor::getDisplayPlayheadSeconds() const
{
    return getCursorSecondsForState();
}

namespace
{
/** Closed / grabbing fist — JUCE only exposes openHand via DraggingHandCursor on macOS. */
juce::MouseCursor makeClosedHandCursor()
{
    juce::Image img(juce::Image::ARGB, 32, 32, true);
    {
        juce::Graphics g(img);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.addTransform(juce::AffineTransform::scale(2.0f));

        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillEllipse(3.5f, 8.5f, 10.0f, 7.0f);

        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(3.0f, 6.5f, 10.0f, 7.5f, 2.2f);
        g.fillRoundedRectangle(1.5f, 8.0f, 3.2f, 5.0f, 1.4f);
        for (int i = 0; i < 4; ++i)
            g.fillRoundedRectangle(3.5f + static_cast<float>(i) * 2.2f, 5.0f, 1.9f, 3.0f, 0.8f);
    }
    return juce::MouseCursor(juce::ScaledImage(img, 2.0), { 8, 8 });
}
}

void HarmonyNoteEditor::beginTimelinePan(float mouseX)
{
    dragMode = DragMode::pan;
    panDragStartX = mouseX;
    panDragStartVisibleTime = timelineView.viewStartSec;
    userNavigatedTimeline = true;
    followPlayhead = false;
    pendingEmptyGesture = false;
}

void HarmonyNoteEditor::beginPitchPan(float mouseY)
{
    dragMode = DragMode::pitchPan;
    panDragStartY = mouseY;
    panDragStartTopMidi = pitchView.viewTopMidi;
    userNavigatedPitch = true;
    pendingEmptyGesture = false;
}

double HarmonyNoteEditor::timeAtMouseX(float mouseX) const
{
    const auto lane = layout.notesUnion();
    const double width = juce::jmax(1.0, static_cast<double>(lane.getWidth()));
    const double frac = juce::jlimit(0.0, 1.0, static_cast<double>((mouseX - lane.getX()) / width));
    return displayedVisibleStart + frac * juce::jmax(1.0e-9, timelineView.viewDurationSec);
}

bool HarmonyNoteEditor::isNearPlayhead(float mouseX) const
{
    const auto lane = layout.notesUnion();
    if (lane.getWidth() <= 1.0f)
        return false;
    const float px = timeToX(getDisplayPlayheadSeconds(), lane);
    return std::abs(mouseX - px) <= 5.0f;
}

void HarmonyNoteEditor::updateTimelineCursor(juce::Point<float> pos, bool /*altDown*/, bool dragging)
{
    const bool overNotes = layout.notesUnion().contains(pos);

    if (overNotes && (editTool == EditTool::pencilFlat || editTool == EditTool::pencilSlope
                      || dragMode == DragMode::pencil))
    {
        static const juce::MouseCursor pencil = makePencilToolCursor();
        setMouseCursor(pencil);
        return;
    }
    if (overNotes && editTool == EditTool::scissors)
    {
        static const juce::MouseCursor scissors = makeScissorsToolCursor();
        setMouseCursor(scissors);
        return;
    }

    const bool panning = dragging || dragMode == DragMode::pan || dragMode == DragMode::pitchPan;
    const bool emptyForPan = overNotes && hitTestNote(pos) < 0;

    if (overNotes && (panning || emptyForPan))
    {
        static const juce::MouseCursor closedHand = makeClosedHandCursor();
        setMouseCursor(panning ? closedHand : juce::MouseCursor::DraggingHandCursor);
        return;
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void HarmonyNoteEditor::mouseMove(const juce::MouseEvent& e)
{
    updateTimelineCursor(e.position, e.mods.isAltDown(),
                          dragMode == DragMode::pan || dragMode == DragMode::pitchPan);
}

void HarmonyNoteEditor::mouseEnter(const juce::MouseEvent& e)
{
    updateTimelineCursor(e.position, e.mods.isAltDown(),
                          dragMode == DragMode::pan || dragMode == DragMode::pitchPan);
}

void HarmonyNoteEditor::mouseExit(const juce::MouseEvent&)
{
    if (dragMode != DragMode::pan && dragMode != DragMode::pitchPan)
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void HarmonyNoteEditor::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    pendingEmptyGesture = false;

    if (layout.toolbar.contains(e.position))
    {
        if (! e.mods.isShiftDown())
            clearSelection();
        dragMode = DragMode::none;
        repaint();
        return;
    }

    // Click / drag on lane headers places (and can drag) the playhead cursor.
    if (layout.voiceHeader.contains(e.position)
        || layout.harmonyHeader.contains(e.position)
        || layout.divider.contains(e.position))
    {
        if (! e.mods.isShiftDown())
            clearSelection();
        setPlayheadFromX(e.position.x);
        dragMode = DragMode::scrub;
        repaint();
        return;
    }

    if (! layout.notesUnion().contains(e.position))
    {
        clearSelection();
        repaint();
        return;
    }

    const auto mods = e.mods;
    const bool altHeld = mods.isAltDown()
                         || juce::ModifierKeys::getCurrentModifiersRealtime().isAltDown();
    const bool middle = mods.isMiddleButtonDown();
    const bool shift = mods.isShiftDown();

    // Middle-click → time pan.
    if (middle)
    {
        beginTimelinePan(e.position.x);
        updateTimelineCursor(e.position, false, true);
        repaint();
        return;
    }

    // Option/Alt + left drag = marquee multi-select ("lençol").
    // Shift+Option keeps previous selection (additive).
    if (altHeld)
    {
        beginMarquee(e.position, shift);
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
        repaint();
        return;
    }

    // Drag near the playhead line to reposition it.
    if (! shift && isNearPlayhead(e.position.x) && hitTestNote(e.position) < 0)
    {
        setPlayheadFromX(e.position.x);
        dragMode = DragMode::scrub;
        repaint();
        return;
    }

    const int hit = hitTestNote(e.position);

    if (hit >= 0)
    {
        const auto& note = processor.noteModel.getNotes()[static_cast<size_t>(hit)];
        if (shift && editTool == EditTool::select)
        {
            toggleSelection(note.id);
            dragMode = DragMode::none;
            dragId = {};
            pitchDragNotes.clear();
            repaint();
            return;
        }

        if (editTool == EditTool::scissors)
        {
            const double cutT = timeAtMouseX(e.position.x);
            const auto leftId = note.id;
            const auto rightId = processor.noteModel.splitNoteAt(leftId, cutT,
                                                                &processor.noteModel.getUndoManager());
            if (rightId.isNotEmpty())
            {
                selectedIds = { leftId, rightId };
                tooltipText = "CUT";
            }
            else
            {
                selectOnly(leftId);
            }
            dragMode = DragMode::none;
            dragId = {};
            repaint();
            return;
        }

        if (editTool == EditTool::pencilFlat || editTool == EditTool::pencilSlope)
        {
            fineDrag = shift;
            beginPencilGesture(note, e.position);
            updatePencilGesture(e.position, shift);
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
            repaint();
            return;
        }

        // SEL: clicking an unselected note selects only it; inside multi-selection keeps all.
        if (! isSelected(note.id))
            selectOnly(note.id);

        dragMode = DragMode::pitchEdit;
        dragId = note.id;
        dragStartOffset = note.manualOffsetSemitones;
        dragStartMidi = harmonyDisplayMidi(note);
        dragStartY = e.position.y;
        fineDrag = false;
        capturePitchDragSnapshot();
        updateTooltip(0.0f);
        repaint();
        return;
    }

    // Shift + empty drag = additive marquee.
    if (shift)
    {
        beginMarquee(e.position, true);
        repaint();
        return;
    }

    // Empty click → place playhead; empty drag → pan time (horizontal) or pitch (vertical).
    clearSelection();
    pitchDragNotes.clear();
    pendingEmptyGesture = true;
    emptyGestureOrigin = e.position;
    emptyGestureStartX = e.position.x;
    emptyGestureStartY = e.position.y;
    emptyGestureStartView = timelineView.viewStartSec;
    emptyGestureStartTopMidi = pitchView.viewTopMidi;
    dragMode = DragMode::none;
    repaint();
}

void HarmonyNoteEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (pendingEmptyGesture)
    {
        const float dx = e.position.x - emptyGestureOrigin.x;
        const float dy = e.position.y - emptyGestureOrigin.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 5.0f)
        {
            pendingEmptyGesture = false;
            if (std::abs(dy) > std::abs(dx))
            {
                beginPitchPan(emptyGestureStartY);
            }
            else
            {
                beginTimelinePan(emptyGestureStartX);
            }
        }
        else
        {
            return;
        }
    }

    if (dragMode == DragMode::scrub)
    {
        setPlayheadFromX(e.position.x);
        repaint();
        return;
    }

    if (dragMode == DragMode::pencil)
    {
        updatePencilGesture(e.position, e.mods.isShiftDown());
        repaint();
        return;
    }

    if (dragMode == DragMode::pitchPan)
    {
        const float laneH = juce::jmax(1.0f, layout.notesUnion().getHeight());
        const float dragDistanceY = e.position.y - panDragStartY;
        pitchView.panByPixelDrag(panDragStartTopMidi, dragDistanceY, laneH);
        userNavigatedPitch = true;
        updateTimelineCursor(e.position, false, true);
        repaint();
        return;
    }

    if (dragMode == DragMode::pan)
    {
        const double timelineWidth = juce::jmax(1.0, static_cast<double>(layout.notesUnion().getWidth()));
        const double dragDistanceX = static_cast<double>(e.position.x - panDragStartX);
        timelineView.panByPixelDrag(panDragStartVisibleTime, dragDistanceX, timelineWidth);
        userNavigatedTimeline = true;
        syncDisplayedVisibleStartFromView();
        updateTimelineCursor(e.position, true, true);
        repaint();
        return;
    }

    if (dragMode == DragMode::marquee)
    {
        marqueeRect = juce::Rectangle<float>(marqueeOrigin, e.position);
        repaint();
        return;
    }

    if (dragMode != DragMode::pitchEdit)
        return;

    fineDrag = e.mods.isShiftDown();
    const float deltaY = dragStartY - e.position.y;
    const auto ks = keyScale();
    float proposedAbsolute = dragStartMidi;
    if (fineDrag)
    {
        proposedAbsolute = dragStartMidi + deltaY * 0.02f;
        if (processor.noteModel.getSnapMode() != nf::notes::SnapMode::off)
            proposedAbsolute = processor.noteModel.quantizeAbsoluteMidi(proposedAbsolute, ks.first, ks.second);
        else
            proposedAbsolute = std::round(proposedAbsolute * 100.0f) / 100.0f;
    }
    else
    {
        const int steps = juce::roundToInt(deltaY / 10.0f);
        proposedAbsolute = dragStartMidi + static_cast<float>(steps);
        proposedAbsolute = processor.noteModel.quantizeAbsoluteMidi(proposedAbsolute, ks.first, ks.second);
    }

    applyPitchDragToSnapshot(proposedAbsolute);
    if (auto* note = processor.noteModel.findNote(dragId))
        tooltipText = formatOffsetTooltip(note->manualOffsetSemitones);
    repaint();
}

void HarmonyNoteEditor::mouseUp(const juce::MouseEvent& e)
{
    if (pendingEmptyGesture)
    {
        pendingEmptyGesture = false;
        setPlayheadFromX(e.position.x);
        dragMode = DragMode::none;
        updateTimelineCursor(e.position, e.mods.isAltDown(), false);
        repaint();
        return;
    }

    if (dragMode == DragMode::scrub)
    {
        setPlayheadFromX(e.position.x);
        dragMode = DragMode::none;
        repaint();
        return;
    }

    if (dragMode == DragMode::pan || dragMode == DragMode::pitchPan)
    {
        dragMode = DragMode::none;
        updateTimelineCursor(e.position, e.mods.isAltDown(), false);
        repaint();
        return;
    }

    if (dragMode == DragMode::marquee)
    {
        applyMarqueeSelection(shiftMarqueeAdditive);
        dragMode = DragMode::none;
        marqueeRect = {};
        repaint();
        return;
    }

    if (dragMode == DragMode::pencil)
    {
        commitPencilGesture();
        return;
    }

    if (dragMode == DragMode::pitchEdit)
        commitPitchDrag(true);
}

void HarmonyNoteEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.mods.isAltDown())
        return;

    const int hit = hitTestNote(e.position);
    if (hit < 0)
        return;
    const auto id = processor.noteModel.getNotes()[static_cast<size_t>(hit)].id;
    processor.noteModel.resetManualOffset(id, &processor.noteModel.getUndoManager());
    selectOnly(id);
    dragMode = DragMode::none;
    dragId = {};
    tooltipText.clear();
    repaint();
}

void HarmonyNoteEditor::updateTooltip(float)
{
    if (auto* note = processor.noteModel.findNote(dragId))
        tooltipText = formatOffsetTooltip(note->manualOffsetSemitones);
}

void HarmonyNoteEditor::commitPitchDrag(bool apply)
{
    if (! pitchDragNotes.empty())
    {
        if (apply)
        {
            auto& um = processor.noteModel.getUndoManager();
            um.beginNewTransaction(pitchDragNotes.size() > 1 ? "Edit harmony notes" : "Edit harmony note");
            for (const auto& snap : pitchDragNotes)
            {
                auto* note = processor.noteModel.findNote(snap.id);
                if (note == nullptr)
                    continue;
                const float finalOffset = note->manualOffsetSemitones;
                note->manualOffsetSemitones = snap.startOffset;
                processor.noteModel.setManualOffset(snap.id, finalOffset, &um);
            }
        }
        else
        {
            for (const auto& snap : pitchDragNotes)
            {
                if (auto* note = processor.noteModel.findNote(snap.id))
                    note->manualOffsetSemitones = snap.startOffset;
            }
            processor.noteModel.republishOffsets();
        }
    }
    else if (! dragId.isEmpty())
    {
        if (auto* note = processor.noteModel.findNote(dragId))
        {
            if (apply)
            {
                const float finalOffset = note->manualOffsetSemitones;
                note->manualOffsetSemitones = dragStartOffset;
                processor.noteModel.setManualOffset(dragId, finalOffset, &processor.noteModel.getUndoManager());
            }
            else
            {
                note->manualOffsetSemitones = dragStartOffset;
                processor.noteModel.republishOffsets();
            }
        }
    }
    dragMode = DragMode::none;
    dragId = {};
    pitchDragNotes.clear();
    tooltipText.clear();
    repaint();
}

void HarmonyNoteEditor::nudgeSelectedPitch(int direction, bool fineCents)
{
    if (selectedIds.empty() || direction == 0)
        return;
    if (dragMode == DragMode::pitchEdit || dragMode == DragMode::marquee
        || dragMode == DragMode::pan || dragMode == DragMode::pitchPan
        || dragMode == DragMode::scrub || dragMode == DragMode::pencil)
        return;

    const auto ks = keyScale();
    const auto snap = processor.noteModel.getSnapMode();
    bool changed = false;

    auto nextScaleDegree = [&](float fromMidi, int dir) -> float
    {
        const int start = juce::roundToInt(fromMidi);
        for (int step = 1; step <= 24; ++step)
        {
            const int candidate = start + dir * step;
            if (candidate < 0 || candidate > 127)
                break;
            if (nf::dsp::MusicalScale::isInScale(candidate, ks.first, ks.second))
                return static_cast<float>(candidate);
        }
        return static_cast<float>(juce::jlimit(0, 127, start + dir));
    };

    for (const auto& id : selectedIds)
    {
        auto* note = processor.noteModel.findNote(id);
        if (note == nullptr)
            continue;

        // Prefer the sounding harmony pitch (curve centre / flat offset).
        const double midT = note->startSec + note->durationSec * 0.5;
        float proposed = note->hasPitchCurve() ? note->editedHarmonyMidiAt(midT)
                                               : harmonyDisplayMidi(*note);

        if (fineCents)
        {
            // Cents steps must not re-quantize back to the same scale degree.
            proposed += static_cast<float>(direction) * 0.01f;
            proposed = std::round(proposed * 100.0f) / 100.0f;
        }
        else if (snap == nf::notes::SnapMode::key)
        {
            proposed = nextScaleDegree(proposed, direction);
        }
        else if (snap == nf::notes::SnapMode::chromatic)
        {
            proposed = std::round(proposed) + static_cast<float>(direction);
        }
        else
        {
            proposed += static_cast<float>(direction);
            proposed = std::round(proposed);
        }

        proposed = juce::jlimit(0.0f, 127.0f, proposed);
        const float newOffset = juce::jlimit(-24.0f, 24.0f, proposed - liveAutoHarmonyMidi(*note));
        if (processor.noteModel.setManualOffset(id, newOffset, &processor.noteModel.getUndoManager()))
        {
            pitchView.expandToInclude(proposed);
            pitchView.expandToInclude(note->voiceMidi);
            changed = true;
            tooltipText = formatOffsetTooltip(newOffset);
            dragId = id;
        }
    }

    if (changed)
        repaint();
}

bool HarmonyNoteEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        // Do not consume Space — the DAW owns Play/Stop; timer freezes the cursor.
        return false;
    }

    if (key.getKeyCode() == juce::KeyPress::deleteKey
        || key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        // Consume even with empty selection so Delete never reaches the DAW.
        deleteSelectedNotesFromEditor();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::upKey
        || key.getKeyCode() == juce::KeyPress::downKey)
    {
        grabKeyboardFocus();
        const int dir = (key.getKeyCode() == juce::KeyPress::upKey) ? 1 : -1;
        const bool fine = key.getModifiers().isShiftDown();
        if (! selectedIds.empty())
            nudgeSelectedPitch(dir, fine);
        return true; // consume so the DAW does not steal arrow keys while editor focused
    }

    if ((key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown())
        && ! key.getModifiers().isShiftDown()
        && (key.getKeyCode() == 'a' || key.getKeyCode() == 'A'))
    {
        selectAllVisibleNotes();
        repaint();
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (dragMode == DragMode::pitchEdit)
        {
            commitPitchDrag(false);
            return true;
        }
        if (dragMode == DragMode::pencil)
        {
            cancelPencilGesture();
            return true;
        }
        if (dragMode == DragMode::marquee || dragMode == DragMode::pan
            || dragMode == DragMode::pitchPan || dragMode == DragMode::scrub)
        {
            dragMode = DragMode::none;
            pendingEmptyGesture = false;
            marqueeRect = {};
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
            return true;
        }
        if (editTool != EditTool::select)
        {
            setEditTool(EditTool::select);
            return true;
        }
        clearSelection();
        tooltipText.clear();
        repaint();
        return true;
    }

    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier, 0))
    {
        processor.noteModel.getUndoManager().undo();
        repaint();
        return true;
    }
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress('y', juce::ModifierKeys::ctrlModifier, 0))
    {
        processor.noteModel.getUndoManager().redo();
        repaint();
        return true;
    }
    return false;
}
