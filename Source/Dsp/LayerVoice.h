#pragma once

#include "PitchShifter.h"
#include "StereoDelay.h"
#include "CavernReverb.h"

struct LayerSettings
{
    int semitones = 0;
    float delayMs = 500.0f;
    float feedback = 40.0f;
    float delayDryDb = 0.0f;
    float delayWetDb = 0.0f;
    bool delayOn = true;
    float reverbDryDb = 0.0f;
    float reverbWetDb = 0.0f;
    float decaySeconds = 4.0f;
    bool reverbOn = true;
};

class LayerVoice
{
public:
    void prepare (double sampleRate)
    {
        shifter.prepare (sampleRate);
        delay.prepare (sampleRate);
        reverb.prepare (sampleRate);
    }

    void reset()
    {
        shifter.reset();
        delay.reset();
        reverb.reset();
    }

    void process (juce::AudioBuffer<float>& audio, const LayerSettings& settings)
    {
        shifter.process (audio, settings.semitones);
        delay.setParameters (settings.delayMs, settings.feedback, settings.delayDryDb, settings.delayWetDb, settings.delayOn);
        delay.process (audio);
        reverb.setParameters (settings.reverbDryDb, settings.reverbWetDb, settings.decaySeconds, settings.reverbOn);
        reverb.process (audio);
    }

private:
    PitchShifter shifter;
    StereoDelay delay;
    CavernReverb reverb;
};
