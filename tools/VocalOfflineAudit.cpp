#include <JuceHeader.h>
#include "dsp/HarmonyEngine.h"
#include "dsp/YinPitchDetector.h"
#include "dsp/MusicalScale.h"
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
int failures = 0;
void check(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    if (! condition)
        ++failures;
}

struct Stats
{
    int voicedFrames = 0;
    int silenceFrames = 0;
    int phantomOnSilence = 0;
    double medianCents = 0.0;
    float peak = 0.0f;
    bool finite = true;
};

Stats analyseFile(const juce::File& file, int intervalChoice, int key, nf::dsp::ScaleType scale)
{
    Stats stats;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
    {
        std::cout << "FAIL Could not open " << file.getFullPathName() << '\n';
        ++failures;
        return stats;
    }

    const double sr = reader->sampleRate;
    const int total = static_cast<int>(juce::jmin<juce::int64>(reader->lengthInSamples, static_cast<juce::int64>(sr * 12.0)));
    juce::AudioBuffer<float> buffer(2, total);
    buffer.clear();
    reader->read(&buffer, 0, total, 0, true, true);

    nf::dsp::HarmonyEngine engine;
    engine.prepare(sr, 512, 2);
    nf::dsp::HarmonySettings settings;
    settings.enabled = true;
    settings.autoKey = false;
    settings.key = key;
    settings.scale = scale;
    settings.intervalChoice = intervalChoice;
    settings.mixPercent = 100.0f;
    settings.harmonyPercent = 100.0f;
    settings.humanizePercent = 0.0f;
    settings.widthPercent = 0.0f;
    settings.formantSemitones = 0.0f;

    juce::AudioBuffer<float> block(2, 512);
    std::vector<float> centsErrors;
    centsErrors.reserve(static_cast<size_t>(total / 512));

    nf::dsp::YinPitchDetector detector;
    detector.prepare(sr);

    for (int pos = 0; pos < total; pos += 512)
    {
        const int n = juce::jmin(512, total - pos);
        block.setSize(2, n, false, false, true);
        for (int ch = 0; ch < 2; ++ch)
            block.copyFrom(ch, 0, buffer, juce::jmin(ch, buffer.getNumChannels() - 1), pos, n);

        float inputEnergy = 0.0f;
        for (int i = 0; i < n; ++i)
            inputEnergy = juce::jmax(inputEnergy, std::abs(block.getSample(0, i)));

        engine.process(block, settings);
        const auto pitch = engine.getPitchEstimate();

        for (int i = 0; i < n; ++i)
        {
            const auto out = block.getSample(0, i);
            stats.finite = stats.finite && std::isfinite(out);
            stats.peak = juce::jmax(stats.peak, std::abs(out));
            if (detector.pushSample(block.getSample(0, i)))
            {
                const auto est = detector.getEstimate();
                if (inputEnergy < 1.0e-3f)
                {
                    ++stats.silenceFrames;
                    if (est.voiced && est.confidence > 0.7f)
                        ++stats.phantomOnSilence;
                }
                else if (est.voiced && pitch.voiced && pitch.confidence > 0.75f)
                {
                    ++stats.voicedFrames;
                    const auto expected = nf::dsp::MusicalScale::targetMidi(pitch.midiNote, intervalChoice, key, scale);
                    const auto cents = (est.midiNote - expected) * 100.0f;
                    centsErrors.push_back(std::abs(cents));
                }
            }
        }
    }

    if (! centsErrors.empty())
    {
        std::sort(centsErrors.begin(), centsErrors.end());
        stats.medianCents = centsErrors[centsErrors.size() / 2];
    }
    return stats;
}
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    if (argc < 2)
    {
        std::cerr << "Usage: VocalOfflineAudit <wav> [more wavs...]\n";
        return 2;
    }

    for (int i = 1; i < argc; ++i)
    {
        const juce::File file(argv[i]);
        std::cout << "\n=== " << file.getFileName() << " ===\n";
        if (! file.existsAsFile())
        {
            std::cout << "FAIL Missing file\n";
            ++failures;
            continue;
        }

        const auto majorThird = analyseFile(file, 4, 0, nf::dsp::ScaleType::major);
        std::cout << "Voiced frames: " << majorThird.voicedFrames
                  << " silence frames: " << majorThird.silenceFrames
                  << " median cents: " << majorThird.medianCents
                  << " peak: " << majorThird.peak << '\n';
        check(majorThird.finite, "Output finite (+3rd major)");
        check(majorThird.peak < 4.0f, "Peak bounded (+3rd major)");
        check(majorThird.phantomOnSilence == 0, "No phantom notes on silence (+3rd major)");
        if (majorThird.voicedFrames > 20)
            check(majorThird.medianCents < 25.0, "Median pitch error under 25 cents on real voice (target <10 commercial)");
        else
            std::cout << "SKIP Cents gate (not enough voiced frames in excerpt)\n";

        const auto octave = analyseFile(file, 7, 0, nf::dsp::ScaleType::major);
        check(octave.finite, "Output finite (+8ve)");
        check(octave.peak < 4.0f, "Peak bounded (+8ve)");
    }

    std::cout << "\nFailures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
