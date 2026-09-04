#pragma once
#include <JuceHeader.h>

class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& stateToUse);
    juce::File getPresetDirectory() const;
    juce::StringArray listPresets() const;
    bool savePreset(const juce::String& name, juce::String& error);
    bool savePresetToFile(const juce::File& file, juce::String& error);
    bool loadPreset(const juce::String& name, juce::String& error);
    bool loadPresetFromFile(const juce::File& file, juce::String& error);

private:
    static juce::String sanitiseName(const juce::String& name);
    juce::AudioProcessorValueTreeState& state;
};

