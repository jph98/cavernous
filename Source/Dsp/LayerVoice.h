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
};

struct ReverbSettings
{
    float dryDb = 0.0f;
    float wetDb = 0.0f;
    float decaySeconds = 4.0f;
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

    void process (juce::AudioBuffer<float>& audio, const LayerSettings& settings)
    {
        for (size_t i = 0; i < delays.size(); ++i)
        {
            const auto& stage = settings.delay[i];
            shifters[i].process (audio, stage.semitones);
            delays[i].setParameters (stage.delayMs, stage.feedback, stage.dryDb, stage.wetDb, stage.on);
            delays[i].process (audio);

            const auto& room = settings.reverb[i];
            reverbs[i].setParameters (room.dryDb, room.wetDb, room.decaySeconds, room.type, room.on,
                                      room.shiftSemitones, room.diffusion, room.lowCutHz, room.highCutHz);
            reverbs[i].process (audio);
        }
    }

private:
    std::array<PitchShifter, 4> shifters;
    std::array<StereoDelay, 4> delays;
    std::array<CavernReverb, 4> reverbs;
};
