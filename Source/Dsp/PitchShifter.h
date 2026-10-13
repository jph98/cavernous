#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>

// Dual-window delay pitch shifter. Semitone 0 is a true bypass.
class PitchShifter
{
public:
    void prepare (double sampleRate)
    {
        window = juce::jmax (32, (int) std::round (0.045 * sampleRate));
        const int needed = window * 2 + 16;
        size = juce::nextPowerOfTwo (needed);
        mask = size - 1;

        for (auto& channel : buffer)
            channel.assign ((size_t) size, 0.0f);

        mix.reset (sampleRate, 0.02);
        reset();
    }

    void reset()
    {
        for (auto& channel : buffer)
            std::fill (channel.begin(), channel.end(), 0.0f);

        write = 0;
        phase = 0.0f;
        currentSemitones = 0;
        pendingSemitones = 0;
        mix.setCurrentAndTargetValue (0.0f);
    }

    void process (juce::AudioBuffer<float>& audio, int semitones)
    {
        semitones = juce::jlimit (-24, 24, semitones);

        if (semitones != pendingSemitones)
        {
            pendingSemitones = semitones;
            if (pendingSemitones != currentSemitones)
                mix.setTargetValue (0.0f);
            else
                mix.setTargetValue (currentSemitones == 0 ? 0.0f : 1.0f);
        }

        const int numSamples = audio.getNumSamples();
        const int channels = juce::jmin (2, audio.getNumChannels());

        for (int i = 0; i < numSamples; ++i)
        {
            if (currentSemitones != pendingSemitones
                && mix.getCurrentValue() < 0.001f
                && mix.getTargetValue() < 0.001f)
            {
                currentSemitones = pendingSemitones;
                phase = 0.0f;
                if (currentSemitones != 0)
                    mix.setTargetValue (1.0f);
            }

            for (int ch = 0; ch < channels; ++ch)
                buffer[(size_t) ch][(size_t) write] = audio.getSample (ch, i);

            float wet[2] { audio.getSample (0, i), channels > 1 ? audio.getSample (1, i) : 0.0f };

            if (currentSemitones != 0)
            {
                const float ratio = std::pow (2.0f, (float) currentSemitones / 12.0f);
                phase += (1.0f - ratio) / (float) window;
                if (phase >= 1.0f) phase -= 1.0f;
                if (phase < 0.0f)  phase += 1.0f;

                float phaseB = phase + 0.5f;
                if (phaseB >= 1.0f) phaseB -= 1.0f;

                const float wA = hann (phase);
                const float wB = hann (phaseB);
                const float norm = juce::jmax (1.0e-5f, wA + wB);

                for (int ch = 0; ch < channels; ++ch)
                    wet[ch] = (read (ch, phase) * wA + read (ch, phaseB) * wB) / norm;
            }

            const float amount = mix.getNextValue();

            for (int ch = 0; ch < channels; ++ch)
            {
                const float dry = audio.getSample (ch, i);
                audio.setSample (ch, i, dry + (wet[ch] - dry) * amount);
            }

            write = (write + 1) & mask;
        }
    }

private:
    static float hann (float p)
    {
        return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p);
    }

    float read (int channel, float p) const
    {
        const float delay = p * (float) window;
        float readPos = (float) write - delay;
        const float length = (float) size;
        readPos = std::fmod (readPos, length);
        if (readPos < 0.0f)
            readPos += length;

        const int i0 = (int) readPos & mask;
        const int i1 = (i0 + 1) & mask;
        const float frac = readPos - std::floor (readPos);
        const float a = buffer[(size_t) channel][(size_t) i0];
        const float b = buffer[(size_t) channel][(size_t) i1];
        return a + frac * (b - a);
    }

    int window = 2048;
    int size = 4096;
    int mask = 4095;
    int write = 0;
    float phase = 0.0f;
    int currentSemitones = 0;
    int pendingSemitones = 0;
    juce::SmoothedValue<float> mix;
    std::array<std::vector<float>, 2> buffer;
};
