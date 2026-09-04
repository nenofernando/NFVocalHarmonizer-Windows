#pragma once

#include <JuceHeader.h>

namespace nf::params
{
inline constexpr auto interval  = "interval";
inline constexpr auto harmony   = "harmony";
inline constexpr auto formant   = "formant";
inline constexpr auto humanize  = "humanize";
inline constexpr auto width     = "width";
inline constexpr auto mix       = "mix";
inline constexpr auto key       = "key";
inline constexpr auto scale     = "scale";
inline constexpr auto autoKey   = "autoKey";
inline constexpr auto enabled   = "enabled";

inline juce::StringArray intervalNames()
{
    return { "-8ve", "-6th", "-5th", "-3rd", "+3rd", "+5th", "+6th", "+8ve" };
}

inline juce::StringArray keyNames()
{
    return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

inline juce::StringArray scaleNames()
{
    return { "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor", "Chromatic" };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using APF = juce::AudioParameterFloat;
    using APC = juce::AudioParameterChoice;
    using APB = juce::AudioParameterBool;
    using R = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<APC>(juce::ParameterID { interval, 1 }, "Interval", intervalNames(), 4));
    p.push_back(std::make_unique<APF>(juce::ParameterID { harmony, 1 }, "Harmony", R { 0.0f, 100.0f, 0.1f }, 70.0f, "%"));
    p.push_back(std::make_unique<APF>(juce::ParameterID { formant, 1 }, "Formant", R { -12.0f, 12.0f, 0.01f }, 0.0f, " st"));
    p.push_back(std::make_unique<APF>(juce::ParameterID { humanize, 1 }, "Humanize", R { 0.0f, 100.0f, 0.1f }, 35.0f, "%"));
    p.push_back(std::make_unique<APF>(juce::ParameterID { width, 1 }, "Width", R { 0.0f, 100.0f, 0.1f }, 100.0f, "%"));
    p.push_back(std::make_unique<APF>(juce::ParameterID { mix, 1 }, "Mix", R { 0.0f, 100.0f, 0.1f }, 50.0f, "%"));
    p.push_back(std::make_unique<APC>(juce::ParameterID { key, 1 }, "Key", keyNames(), 7));
    p.push_back(std::make_unique<APC>(juce::ParameterID { scale, 1 }, "Scale", scaleNames(), 1));
    p.push_back(std::make_unique<APB>(juce::ParameterID { autoKey, 1 }, "Auto Key", true));
    p.push_back(std::make_unique<APB>(juce::ParameterID { enabled, 1 }, "Harmonize", false));
    return { p.begin(), p.end() };
}
}

