#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "PresetManager.h"
#include "dsp/HarmonyEngine.h"

class NFVocalHarmonizerAudioProcessor final : public juce::AudioProcessor
{
public:
    NFVocalHarmonizerAudioProcessor();
    ~NFVocalHarmonizerAudioProcessor() override = default;
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
    nf::dsp::PitchEstimate getPitchEstimate() const { return engine.getPitchEstimate(); }
    nf::dsp::ScaleResult getDetectedScale() const { return engine.getDetectedScale(); }
    nf::dsp::MeterSnapshot getMeters() const { return engine.getMeters(); }
    void captureSlot(bool slotA);
    void restoreSlot(bool slotA);
    void copySlot(bool fromA);

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

private:
    nf::dsp::HarmonySettings readSettings() const;
    nf::dsp::HarmonyEngine engine;
    juce::ValueTree slotA { "SLOT_A" }, slotB { "SLOT_B" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFVocalHarmonizerAudioProcessor)
};

