#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/HarmonyEngine.h"
#include "dsp/NoteEditModel.h"
#include "dsp/NoteCapture.h"
#include "dsp/AnalysisState.h"
#include <atomic>
#include <cstdint>

class NFVocalHarmonizerAudioProcessor final : public juce::AudioProcessor,
                                              private juce::Timer
{
public:
    struct TransportSnapshot final
    {
        int64_t currentHostSample = 0;
        int64_t lastValidPlayingSample = 0;
        int64_t analysisStartHostSample = 0;
        double sampleRate = 44100.0;
        bool isPlaying = false;
        bool hasValidHostPosition = false;
        uint64_t revision = 0;
    };

    NFVocalHarmonizerAudioProcessor();
    ~NFVocalHarmonizerAudioProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    void resetKeyAnalysis() { engine.resetKeyAnalysis(); }
    void beginAnalyzeCapture();
    void finalizeAnalyzeCapture();
    bool isAnalyzeArmed() const noexcept { return capture.isArmed(); }
    AnalysisState getAnalysisState() const noexcept
    {
        return analysisState.load(std::memory_order_relaxed);
    }

    nf::dsp::PitchEstimate getPitchEstimate() const { return engine.getPitchEstimate(); }
    nf::dsp::ScaleResult getDetectedScale() const { return engine.getDetectedScale(); }
    nf::dsp::MeterSnapshot getMeters() const { return engine.getMeters(); }

    TransportSnapshot getTransportSnapshot() const noexcept;
    void markAnalysisStartFromCurrentTransport() noexcept;
    void clearAnalysisTransportState() noexcept;

    /** True host clock for DSP / ANALYZE arming (never substituted with parked GUI cursor). */
    double getHostTimeSeconds() const noexcept;
    bool isHostPlaying() const noexcept;

    /** Session cursor seconds — written on message thread, read in getStateInformation. */
    void setEditorCursorSecondsForState(double seconds) noexcept;
    double getEditorCursorSecondsForState() const noexcept;
    /** >= 0 means HarmonyNoteEditor should restore once after setStateInformation. */
    double takePendingEditorCursorSeconds() noexcept;

    void captureSlot(bool slotA);
    void restoreSlot(bool slotA);
    void copySlot(bool fromA);

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    nf::notes::NoteEditModel noteModel;

private:
    void timerCallback() override;
    void captureHostTransportForEditor() noexcept;
    nf::dsp::HarmonySettings readSettings() const;
    std::pair<int, nf::dsp::ScaleType> activeKeyScale() const;

    nf::dsp::HarmonyEngine engine;
    nf::notes::NoteCapture capture;
    juce::ValueTree slotA { "SLOT_A" }, slotB { "SLOT_B" };

    std::atomic<int64_t> transportCurrentSample { 0 };
    std::atomic<int64_t> transportLastPlayingSample { 0 };
    std::atomic<int64_t> transportAnalysisStartSample { 0 };
    std::atomic<double> transportSampleRate { 44100.0 };
    std::atomic<bool> transportIsPlaying { false };
    std::atomic<bool> transportPositionValid { false };
    std::atomic<uint64_t> transportRevision { 0 };

    std::atomic<double> editorCursorSecondsForState { 0.0 };
    std::atomic<double> pendingEditorCursorSeconds { -1.0 };

    std::atomic<AnalysisState> analysisState { AnalysisState::idle };
    bool wasPlaying = false; // message-thread (ANALYZE finalize)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFVocalHarmonizerAudioProcessor)
};
