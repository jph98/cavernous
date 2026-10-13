#include "Dsp/LayerVoice.h"
#include <cmath>
#include <iostream>

namespace
{
int failures = 0;

void expect (bool condition, const char* text)
{
    if (! condition)
    {
        std::cerr << "FAIL " << text << "\n";
        ++failures;
    }
}

bool finiteBuffer (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (channel, i)))
                return false;

    return true;
}

float peakAround (const juce::AudioBuffer<float>& buffer, int centre, int radius)
{
    float peak = 0.0f;
    const int start = juce::jmax (0, centre - radius);
    const int end = juce::jmin (buffer.getNumSamples(), centre + radius);

    for (int i = start; i < end; ++i)
        peak = std::max (peak, std::abs (buffer.getSample (0, i)));

    return peak;
}
}

int main()
{
    constexpr double sampleRate = 48000.0;
    LayerVoice voice;
    voice.prepare (sampleRate);

    LayerSettings delayOnly;
    delayOnly.semitones = 0;
    delayOnly.delayMs = 100.0f;
    delayOnly.feedback = 0.0f;
    delayOnly.delayDryDb = -80.0f;
    delayOnly.delayWetDb = 0.0f;
    delayOnly.delayOn = true;
    delayOnly.reverbDryDb = -80.0f;
    delayOnly.reverbWetDb = -80.0f;
    delayOnly.reverbOn = false;

    juce::AudioBuffer<float> buffer (2, (int) sampleRate);
    buffer.clear();
    buffer.setSample (0, 0, 0.8f);
    buffer.setSample (1, 0, 0.8f);
    voice.process (buffer, delayOnly);

    expect (finiteBuffer (buffer), "delay output is finite");
    expect (peakAround (buffer, 4800, 80) > 0.2f, "100 ms delay repeats the impulse");
    expect (std::abs (buffer.getSample (0, 100)) < 0.05f, "dry path is down when delay dry is -80 dB");

    voice.reset();
    voice.prepare (sampleRate);

    LayerSettings bypassed;
    bypassed.delayOn = false;
    bypassed.reverbOn = false;
    bypassed.semitones = 0;

    buffer.clear();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float sample = 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f * (float) i / (float) sampleRate);
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
    }

    juce::AudioBuffer<float> original;
    original.makeCopyOf (buffer);
    voice.process (buffer, bypassed);

    float error = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        error = std::max (error, std::abs (buffer.getSample (0, i) - original.getSample (0, i)));

    expect (error < 0.0001f, "bypassed octave, delay, and reverb leave the signal untouched");

    voice.reset();
    voice.prepare (sampleRate);

    LayerSettings cave;
    cave.delayOn = false;
    cave.reverbOn = true;
    cave.reverbDryDb = -80.0f;
    cave.reverbWetDb = 0.0f;
    cave.decaySeconds = 2.0f;
    cave.semitones = 0;

    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);
    voice.process (buffer, cave);

    expect (finiteBuffer (buffer), "reverb output is finite");

    float earlyEnergy = 0.0f;
    float lateEnergy = 0.0f;
    for (int i = 4000; i < 8000; ++i)
        earlyEnergy += std::abs (buffer.getSample (0, i));
    for (int i = 30000; i < 34000; ++i)
        lateEnergy += std::abs (buffer.getSample (0, i));

    expect (earlyEnergy > 1.0f, "reverb produces a tail");
    expect (earlyEnergy > lateEnergy, "reverb tail decays");

    if (failures != 0)
    {
        std::cerr << failures << " failed\n";
        return 1;
    }

    std::cout << "dsp ok\n";
    return 0;
}
