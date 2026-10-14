#include "Dsp/LayerVoice.h"
#include "Dsp/ToneSource.h"
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

float tonePower (const juce::AudioBuffer<float>& buffer, float frequency, double rate)
{
    double cosine = 0.0;
    double sine = 0.0;
    const double omega = juce::MathConstants<double>::twoPi * (double) frequency / rate;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const double sample = buffer.getSample (0, i);
        cosine += sample * std::cos (omega * i);
        sine += sample * std::sin (omega * i);
    }

    return (float) std::sqrt (cosine * cosine + sine * sine);
}

float noiseBalance (int colour)
{
    ToneBurst burst;
    burst.trigger (48000.0, 0, 4, 0, colour);
    float low = 0.0f;
    double lowEnergy = 0.0;
    double highEnergy = 0.0;

    for (int i = 0; i < 12000; ++i)
    {
        const float sample = burst.nextNoise();
        low += 0.02f * (sample - low);
        const float high = sample - low;
        lowEnergy += (double) low * (double) low;
        highEnergy += (double) high * (double) high;
    }

    return (float) (lowEnergy / juce::jmax (1.0e-12, highEnergy));
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

    auto tailCentre = [&] (float size)
    {
        voice.reset();
        voice.prepare (sampleRate);
        LayerSettings room = cave;
        room.reverb[0].sizePercent = size;
        room.reverb[0].decaySeconds = 4.0f;
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);
        voice.process (buffer, room);

        double weight = 0.0;
        double energy = 0.0;
        for (int i = 800; i < 16000; ++i)
        {
            const double magnitude = std::abs (buffer.getSample (0, i));
            weight += magnitude * i;
            energy += magnitude;
        }

        return energy > 0.0 ? weight / energy : 0.0;
    };

    expect (tailCentre (180.0f) > tailCentre (30.0f), "a larger reverb size spreads the tail later");

    voice.reset();
    voice.prepare (sampleRate);
    buffer.clear();
    voice.knock (0);
    voice.process (buffer, cave);
    expect (finiteBuffer (buffer), "a tank knock stays finite");
    float knockPeak = 0.0f;
    for (int i = 0; i < 8000; ++i)
        knockPeak = std::max (knockPeak, std::abs (buffer.getSample (0, i)));
    expect (knockPeak > 0.02f, "kicking the tank rings the reverb");

    auto kickPower = [&] (float pitch)
    {
        voice.reset();
        voice.prepare (sampleRate);
        buffer.clear();
        voice.knock (0, pitch);
        voice.process (buffer, cave);
        return tonePower (buffer, pitch, sampleRate);
    };

    const float lowKick = kickPower (140.0f);
    const float lowAtHigh = tonePower (buffer, 520.0f, sampleRate);
    const float highKick = kickPower (520.0f);
    const float highAtLow = tonePower (buffer, 140.0f, sampleRate);
    expect (lowKick > lowAtHigh, "a low tank kick rings at the low pitch");
    expect (highKick > highAtLow, "a high tank kick rings at the high pitch");

    voice.reset();
    voice.prepare (sampleRate);
    LayerSettings fluttering = delayOnly;
    fluttering.delay[0].delayMs = 200.0f;
    fluttering.delay[0].flutter = true;
    fluttering.delay[0].flutterMs = 30.0f;
    buffer.clear();
    buffer.setSample (0, 0, 0.8f);
    buffer.setSample (1, 0, 0.8f);
    voice.process (buffer, fluttering);
    expect (finiteBuffer (buffer), "fluttering delay stays finite");
    int flutterPeak = 0;
    float flutterLevel = 0.0f;
    for (int i = 7000; i < 13000; ++i)
    {
        const float level = std::abs (buffer.getSample (0, i));
        if (level > flutterLevel)
        {
            flutterLevel = level;
            flutterPeak = i;
        }
    }
    expect (flutterLevel > 0.05f, "flutter still repeats the impulse");
    expect (std::abs (flutterPeak - 9600) > 200, "flutter moves the delay time off its centre");

    voice.reset();
    voice.prepare (sampleRate);
    juce::AudioBuffer<float> droneSend (2, buffer.getNumSamples());
    for (int i = 0; i < droneSend.getNumSamples(); ++i)
    {
        const float sample = 0.22f * std::sin (2.0f * juce::MathConstants<float>::pi * 110.0f * (float) i / (float) sampleRate);
        droneSend.setSample (0, i, sample);
        droneSend.setSample (1, i, sample);
    }

    LayerSettings fed;
    fed.delay[0].on = true;
    fed.delay[0].delayMs = 180.0f;
    fed.delay[0].feedback = 92.0f;
    fed.delay[0].dryDb = -80.0f;
    fed.delay[0].wetDb = 0.0f;
    fed.delay[1].on = true;
    fed.delay[1].delayMs = 260.0f;
    fed.delay[1].feedback = 92.0f;
    fed.delay[1].dryDb = -80.0f;
    fed.delay[1].wetDb = 0.0f;
    fed.reverb[0].on = true;
    fed.reverb[0].decaySeconds = 8.0f;
    fed.reverb[0].dryDb = -80.0f;
    fed.reverb[0].wetDb = 0.0f;
    buffer.clear();
    voice.process (buffer, fed, &droneSend);
    expect (finiteBuffer (buffer), "a drone into high feedback stays finite");
    float earlyDrone = 0.0f;
    float lateDrone = 0.0f;
    const int lateStart = buffer.getNumSamples() - (int) (0.25 * sampleRate);
    for (int i = (int) (0.2 * sampleRate); i < (int) (0.45 * sampleRate); ++i)
        earlyDrone = std::max (earlyDrone, std::abs (buffer.getSample (0, i)));
    for (int i = lateStart; i < buffer.getNumSamples(); ++i)
        lateDrone = std::max (lateDrone, std::abs (buffer.getSample (0, i)));
    expect (earlyDrone > 0.02f, "the drone is still heard");
    expect (lateDrone < 1.25f, "the drone does not cascade through feedback");

    ToneBurst heldNote;
    heldNote.trigger (sampleRate, 0, 4, 0, 0, 2.0f);
    heldNote.stop();
    buffer.clear();
    heldNote.addTo (buffer);
    expect (std::abs (buffer.getSample (0, 100)) < 0.0001f, "stopping a note silences it");

    DroneTones liveDrone;
    liveDrone.prepare (sampleRate);
    const bool rootOnly[kDroneDegreeCount] = { true, false, false, false, false, false, false };
    liveDrone.setNotes (0, 4, 0, 0, rootOnly, true);
    buffer.clear();
    liveDrone.addTo (buffer);
    liveDrone.silence();
    buffer.clear();
    liveDrone.addTo (buffer);
    float leftover = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        leftover = std::max (leftover, std::abs (buffer.getSample (0, i)));
    expect (leftover < 0.0001f, "silencing a drone cuts the held notes");

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

    expect (std::abs (toneFrequencyHz (0, 4) - 261.6256f) < 0.05f, "C4 is middle C");
    expect (std::abs (toneFrequencyHz (7, 3) - 195.9977f) < 0.05f, "G3 is 196 Hz");
    expect (std::abs (toneFrequencyHz (0, 4, 12.0f) - toneFrequencyHz (0, 5)) < 0.05f,
            "raising the tone twelve semitones is one octave higher");
    expect (std::abs (toneFrequencyHz (0, 4, -12.0f) - toneFrequencyHz (0, 3)) < 0.05f,
            "lowering the tone twelve semitones is one octave lower");

    ToneBurst pure;
    pure.trigger (sampleRate, 0, 4, 0, 0);
    buffer.setSize (1, (int) (0.2 * sampleRate), false, true, true);
    buffer.clear();
    pure.addTo (buffer);
    expect (finiteBuffer (buffer), "generated tone stays finite");
    const float fundamental = toneFrequencyHz (0, 4);
    const float pureFundamental = tonePower (buffer, fundamental, sampleRate);
    const float pureSecond = tonePower (buffer, fundamental * 2.0f, sampleRate);
    expect (pureFundamental > 4.0f, "a pure tone has energy at the chosen pitch");
    expect (pureFundamental > pureSecond * 8.0f, "no overtones leaves the second harmonic quiet");

    ToneBurst rich;
    rich.trigger (sampleRate, 0, 4, 3, 0);
    buffer.clear();
    rich.addTo (buffer);
    const float secondHarmonic = tonePower (buffer, fundamental * 2.0f, sampleRate);
    const float thirdHarmonic = tonePower (buffer, fundamental * 3.0f, sampleRate);
    const float between = tonePower (buffer, fundamental * 2.5f, sampleRate);
    expect (secondHarmonic > between * 4.0f, "overtones add the second harmonic");
    expect (thirdHarmonic > between * 4.0f, "overtones add the third harmonic");

    const float brown = noiseBalance (3);
    const float pink = noiseBalance (2);
    const float white = noiseBalance (1);
    const float blue = noiseBalance (4);
    expect (brown > pink, "brown noise is darker than pink");
    expect (pink > white, "pink noise is darker than white");
    expect (white > blue, "blue noise is brighter than white");

    ToneBurst timed;
    timed.trigger (sampleRate, 0, 4, 0, 0, 0.15f);
    buffer.setSize (1, (int) (0.4 * sampleRate), false, false, true);
    buffer.clear();
    timed.addTo (buffer);
    float during = 0.0f;
    float after = 0.0f;
    for (int i = 0; i < (int) (0.12 * sampleRate); ++i)
        during += std::abs (buffer.getSample (0, i));
    for (int i = (int) (0.22 * sampleRate); i < buffer.getNumSamples(); ++i)
        after += std::abs (buffer.getSample (0, i));
    expect (during > 1.0f, "a note sounds for its length");
    expect (after < during * 0.02f, "the note ends when its length is over");

    DroneTones drone;
    drone.prepare (sampleRate);
    const bool tonicFifth[kDroneDegreeCount] = { true, false, false, false, true, false, false };
    drone.setNotes (0, 4, 0, 0, tonicFifth, true);
    buffer.setSize (1, (int) (0.3 * sampleRate), false, false, true);
    buffer.clear();
    drone.addTo (buffer);
    const float rootPower = tonePower (buffer, droneNoteHz (0, 4, 0, 0), sampleRate);
    const float fifthPower = tonePower (buffer, droneNoteHz (0, 4, 0, 4), sampleRate);
    const float thirdPower = tonePower (buffer, droneNoteHz (0, 4, 0, 2), sampleRate);
    expect (rootPower > thirdPower * 4.0f, "the drone holds the tonic");
    expect (fifthPower > thirdPower * 4.0f, "the drone holds the fifth");
    expect (std::abs (droneNoteHz (0, 4, 1, 2) - toneFrequencyHz (3, 4)) < 0.05f, "minor degree 3 is a minor third");
    expect (std::abs (droneNoteHz (0, 4, 0, 2) - toneFrequencyHz (4, 4)) < 0.05f, "major degree 3 is a major third");

    if (failures != 0)
    {
        std::cerr << failures << " failed\n";
        return 1;
    }

    std::cout << "dsp ok\n";
    return 0;
}
