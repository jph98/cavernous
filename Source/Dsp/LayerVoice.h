#pragma once

#include "PitchShifter.h"
#include "StereoDelay.h"
#include "CavernReverb.h"
#include <array>

struct DelaySettings
{
    int semitones = 0;
    float delayMs = 500.0f;
    float feedback = 40.0f;
    float dryDb = 0.0f;
    float wetDb = 0.0f;
    bool on = false;
    bool flutter = false;
    float flutterMs = 8.0f;
};

struct ReverbSettings
{
    float dryDb = 0.0f;
    float wetDb = 0.0f;
    float decaySeconds = 4.0f;
    float sizePercent = 100.0f;
    bool on = false;
    int type = 0;
    float shiftSemitones = 12.0f;
    float diffusion = 80.0f;
    float lowCutHz = 150.0f;
    float highCutHz = 8000.0f;
};

struct LayerSettings
{
    std::array<DelaySettings, 4> delay;
    std::array<ReverbSettings, 4> reverb;
};

class LayerVoice
{
public:
    void prepare (double sampleRate)
    {
        for (auto& shifter : shifters)
            shifter.prepare (sampleRate);

        for (auto& stage : delays)
            stage.prepare (sampleRate);

        for (auto& stage : reverbs)
            stage.prepare (sampleRate);

        droneWet.setSize (2, 8192, false, true, true);
    }

    void reset()
    {
        for (auto& shifter : shifters)
            shifter.reset();

        killWet();
    }

    void killWet()
    {
        for (auto& stage : delays)
            stage.reset();

        for (auto& stage : reverbs)
            stage.reset();
    }

    void knock (int reverbIndex, float pitchHz = 185.0f)
    {
        if (reverbIndex >= 0 && reverbIndex < (int) reverbs.size())
            reverbs[(size_t) reverbIndex].knock (pitchHz);
    }

    void process (juce::AudioBuffer<float>& audio, const LayerSettings& settings,
                  const juce::AudioBuffer<float>* drone = nullptr)
    {
        const int numSamples = audio.getNumSamples();
        if (drone != nullptr)
        {
            if (droneWet.getNumSamples() < numSamples)
                droneWet.setSize (2, numSamples, false, false, true);
            droneWet.clear (0, numSamples);
        }

        for (size_t i = 0; i < delays.size(); ++i)
        {
            const auto& stage = settings.delay[i];
            shifters[i].process (audio, stage.semitones);
            delays[i].setParameters (stage.delayMs, stage.feedback, stage.dryDb, stage.wetDb, stage.on,
                                      stage.flutter, stage.flutterMs);
            delays[i].process (audio, drone, drone != nullptr ? &droneWet : nullptr);

            bool laterDelay = false;
            for (size_t later = i + 1; later < delays.size(); ++later)
                if (settings.delay[later].on)
                    laterDelay = true;

            const auto& room = settings.reverb[i];
            reverbs[i].setParameters (room.dryDb, room.wetDb, room.decaySeconds, room.type, room.on,
                                      room.shiftSemitones, room.diffusion, room.lowCutHz, room.highCutHz,
                                      room.sizePercent);
            reverbs[i].process (audio, laterDelay ? nullptr : drone);
        }

        if (drone == nullptr)
            return;

        const int channels = juce::jmin (audio.getNumChannels(), drone->getNumChannels(), droneWet.getNumChannels());
        for (int channel = 0; channel < channels; ++channel)
        {
            audio.addFrom (channel, 0, *drone, channel, 0, numSamples);
            audio.addFrom (channel, 0, droneWet, channel, 0, numSamples);
        }
    }

private:
    std::array<PitchShifter, 4> shifters;
    std::array<StereoDelay, 4> delays;
    std::array<CavernReverb, 4> reverbs;
    juce::AudioBuffer<float> droneWet;
};
