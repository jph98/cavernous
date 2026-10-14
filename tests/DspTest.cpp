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
    delayOnly.delay[0].delayMs = 100.0f;
    delayOnly.delay[0].feedback = 0.0f;
    delayOnly.delay[0].dryDb = -80.0f;
    delayOnly.delay[0].wetDb = 0.0f;
    delayOnly.delay[0].on = true;
    delayOnly.reverb[0].dryDb = -80.0f;
    delayOnly.reverb[0].wetDb = -80.0f;
    delayOnly.reverb[0].on = false;

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
    bypassed.reverb[0].on = false;

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
    cave.reverb[0].on = true;
    cave.reverb[0].dryDb = -80.0f;
    cave.reverb[0].wetDb = 0.0f;
    cave.reverb[0].decaySeconds = 2.0f;

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

    voice.reset();
    voice.prepare (sampleRate);

    LayerSettings second;
    second.delay[1].delayMs = 150.0f;
    second.delay[1].feedback = 0.0f;
    second.delay[1].dryDb = -80.0f;
    second.delay[1].wetDb = 0.0f;
    second.delay[1].on = true;
    second.reverb[0].on = false;

    buffer.clear();
    buffer.setSample (0, 0, 0.8f);
    buffer.setSample (1, 0, 0.8f);
    voice.process (buffer, second);
    expect (peakAround (buffer, 7200, 120) > 0.2f, "delay 2 repeats at 150 ms");

    auto lateEnergyOf = [&] (float decay)
    {
        voice.reset();
        voice.prepare (sampleRate);
        LayerSettings ringing = cave;
        ringing.reverb[0].decaySeconds = decay;
        ringing.reverb[0].type = 0;
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);
        voice.process (buffer, ringing);

        float energy = 0.0f;
        for (int i = 36000; i < 44000; ++i)
            energy += std::abs (buffer.getSample (0, i));
        return energy;
    };

    const float held = lateEnergyOf (10.0f);
    const float shortTail = lateEnergyOf (2.0f);
    expect (held > shortTail * 1.5f, "long decay keeps more of the tail");

    for (int type = 0; type < 6; ++type)
    {
        voice.reset();
        voice.prepare (sampleRate);
        LayerSettings kind = cave;
        kind.reverb[0].type = type;
        kind.reverb[0].decaySeconds = 10.0f;
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);
        voice.process (buffer, kind);
        expect (finiteBuffer (buffer), "reverb type stays finite");
    }

    auto staysBounded = [&] (LayerSettings settings, const char* name, float seconds)
    {
        voice.reset();
        voice.prepare (sampleRate);
        juce::AudioBuffer<float> block (2, 512);
        float peak = 0.0f;
        bool finite = true;
        const int blocks = (int) (seconds * sampleRate / 512.0);

        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float t = (float) (b * 512 + i) / (float) sampleRate;
                const float sample = 0.25f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * t);
                block.setSample (0, i, sample);
                block.setSample (1, i, sample);
            }

            voice.process (block, settings);
            if (! finiteBuffer (block))
                finite = false;

            for (int i = 0; i < 512; ++i)
                peak = std::max (peak, std::abs (block.getSample (0, i)));
        }

        expect (finite, name);
        expect (peak < 6.0f, name);
    };

    LayerSettings shimmering;
    shimmering.reverb[0].on = true;
    shimmering.reverb[0].type = 5;
    shimmering.reverb[0].dryDb = 0.0f;
    shimmering.reverb[0].wetDb = 0.0f;
    shimmering.reverb[0].decaySeconds = 4.0f;
    staysBounded (shimmering, "shimmer stays bounded", 20.0f);

    voice.reset();
    voice.prepare (sampleRate);
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);
    voice.process (buffer, shimmering);
    expect (finiteBuffer (buffer), "shimmer impulse stays finite");
    float shimmerEarly = 0.0f;
    float shimmerLate = 0.0f;
    for (int i = 2000; i < 8000; ++i)
        shimmerEarly += std::abs (buffer.getSample (0, i));
    for (int i = 36000; i < 44000; ++i)
        shimmerLate += std::abs (buffer.getSample (0, i));
    expect (shimmerEarly > shimmerLate, "shimmer tail dies away instead of regenerating");

    LayerSettings hotDelays;
    hotDelays.reverb[0].on = false;
    for (int d = 0; d < 4; ++d)
    {
        hotDelays.delay[(size_t) d].on = true;
        hotDelays.delay[(size_t) d].feedback = 100.0f;
        hotDelays.delay[(size_t) d].wetDb = 6.0f;
        hotDelays.delay[(size_t) d].dryDb = 0.0f;
    }
    staysBounded (hotDelays, "stacked delays stay bounded", 12.0f);

    LayerSettings extreme;
    extreme.delay[0].on = true;
    extreme.delay[0].feedback = 0.0f;
    extreme.delay[0].dryDb = 0.0f;
    extreme.delay[0].wetDb = 0.0f;
    extreme.reverb[0].on = false;
    extreme.delay[0].semitones = 48;
    staysBounded (extreme, "plus four octaves stays finite", 2.0f);
    extreme.delay[0].semitones = -48;
    staysBounded (extreme, "minus four octaves stays finite", 2.0f);

    if (failures != 0)
    {
        std::cerr << failures << " failed\n";
        return 1;
    }

    std::cout << "dsp ok\n";
    return 0;
}
