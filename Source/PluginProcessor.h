#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/HarmonyEngine.h"
#include "dsp/NoteEditModel.h"
#include "dsp/NoteCapture.h"

class NFVocalHarmonizerAudioProcessor final : public juce::AudioProcessor,
                                              private juce::Timer
{
public:
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

    nf::dsp::PitchEstimate getPitchEstimate() const { return engine.getPitchEstimate(); }
    nf::dsp::ScaleResult getDetectedScale() const { return engine.getDetectedScale(); }
    nf::dsp::MeterSnapshot getMeters() const { return engine.getMeters(); }
    double getHostTimeSeconds() const noexcept { return hostTimeSec.load(std::memory_order_relaxed); }
    bool isHostPlaying() const noexcept { return hostPlaying.load(std::memory_order_relaxed); }

    void captureSlot(bool slotA);
    void restoreSlot(bool slotA);
    void copySlot(bool fromA);

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    nf::notes::NoteEditModel noteModel;

private:
    void timerCallback() override;
    nf::dsp::HarmonySettings readSettings() const;
    std::pair<int, nf::dsp::ScaleType> activeKeyScale() const;

    nf::dsp::HarmonyEngine engine;
    nf::notes::NoteCapture capture;
    juce::ValueTree slotA { "SLOT_A" }, slotB { "SLOT_B" };
    std::atomic<double> hostTimeSec { 0.0 };
    std::atomic<bool> hostPlaying { false };
    bool wasPlaying = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFVocalHarmonizerAudioProcessor)
};
