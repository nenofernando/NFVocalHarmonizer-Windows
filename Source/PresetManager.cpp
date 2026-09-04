#include "PresetManager.h"

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& stateToUse) : state(stateToUse) {}

juce::File PresetManager::getPresetDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("NF Audio Tools").getChildFile("NF Vocal Harmonizer").getChildFile("Presets");
}

juce::String PresetManager::sanitiseName(const juce::String& input)
{
    auto name = input.trim();
    for (auto c : juce::String("/\\:*?\"<>|")) name = name.replaceCharacter(c, '_');
    return name.substring(0, 80);
}

juce::StringArray PresetManager::listPresets() const
{
    juce::StringArray names;
    const auto directory = getPresetDirectory();
    if (! directory.exists()) return names;
    for (const auto& file : directory.findChildFiles(juce::File::findFiles, false, "*.nfhpreset"))
        names.add(file.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool PresetManager::savePreset(const juce::String& rawName, juce::String& error)
{
    const auto name = sanitiseName(rawName);
    if (name.isEmpty()) { error = "Preset name is empty."; return false; }
    return savePresetToFile(getPresetDirectory().getChildFile(name + ".nfhpreset"), error);
}

bool PresetManager::savePresetToFile(const juce::File& target, juce::String& error)
{
    const auto parent = target.getParentDirectory();
    if (! parent.exists() && ! parent.createDirectory())
    { error = "Could not create the preset directory."; return false; }

    const auto snapshot = state.copyState();
    const auto xml = snapshot.createXml();
    if (xml == nullptr) { error = "Could not serialize the plugin state."; return false; }

    juce::TemporaryFile temporary(target);
    {
        auto stream = temporary.getFile().createOutputStream();
        if (stream == nullptr || ! stream->openedOk())
        { error = "Could not open the preset file for writing."; return false; }
        xml->writeTo(*stream, {});
        stream->flush();
        if (stream->getStatus().failed())
        { error = stream->getStatus().getErrorMessage(); return false; }
    }
    if (! temporary.overwriteTargetFileWithTemporary())
    { error = "Could not finalize the preset file."; return false; }
    return true;
}

bool PresetManager::loadPreset(const juce::String& name, juce::String& error)
{
    return loadPresetFromFile(getPresetDirectory().getChildFile(sanitiseName(name) + ".nfhpreset"), error);
}

bool PresetManager::loadPresetFromFile(const juce::File& file, juce::String& error)
{
    if (! file.existsAsFile()) { error = "Preset file not found."; return false; }
    const auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || ! xml->hasTagName(state.state.getType()))
    { error = "Invalid or incompatible preset."; return false; }
    const auto restored = juce::ValueTree::fromXml(*xml);
    if (! restored.isValid()) { error = "Preset state is invalid."; return false; }
    state.replaceState(restored);
    return true;
}

