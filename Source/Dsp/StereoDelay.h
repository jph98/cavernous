#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>

class StereoDelay
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) std::ceil (sampleRate * 2.5) + 8;

        for (auto& channel : buffer)
            channel.assign ((size_t) size, 0.0f);

        timeMs.reset (sampleRate, 0.05);
        feedback.reset (sampleRate, 0.03);
        dry.reset (sampleRate, 0.02);
        wet.reset (sampleRate, 0.02);
        bypass.reset (sampleRate, 0.015);
        primed = false;
        reset();
    }

    void reset()
    {
        for (auto& channel : buffer)
            std::fill (channel.begin(), channel.end(), 0.0f);

        write = 0;
        tone[0] = tone[1] = 0.0f;
    }

    void setParameters (float delayMs, float feedbackPercent, float dryDb, float wetDb, bool enabled)
    {
        timeMs.setTargetValue (juce::jlimit (1.0f, 2500.0f, delayMs));
        feedback.setTargetValue (juce::jlimit (0.0f, 100.0f, feedbackPercent) * 0.01f);
        dry.setTargetValue (juce::Decibels::decibelsToGain (dryDb, -80.0f));
        wet.setTargetValue (juce::Decibels::decibelsToGain (wetDb, -80.0f));
        bypass.setTargetValue (enabled ? 1.0f : 0.0f);

        if (! primed)
        {
            timeMs.setCurrentAndTargetValue (timeMs.getTargetValue());
            feedback.setCurrentAndTargetValue (feedback.getTargetValue());
            dry.setCurrentAndTargetValue (dry.getTargetValue());
            wet.setCurrentAndTargetValue (wet.getTargetValue());
            bypass.setCurrentAndTargetValue (bypass.getTargetValue());
            primed = true;
        }
    }

    void process (juce::AudioBuffer<float>& audio)
    {
        const int numSamples = audio.getNumSamples();
        const int channels = juce::jmin (2, audio.getNumChannels());
        const float maxDelay = (float) (size - 4);

        for (int i = 0; i < numSamples; ++i)
        {
            const float delaySamples = juce::jlimit (1.0f, maxDelay,
                                                     (float) (timeMs.getNextValue() * 0.001 * sr));
            const float fb = feedback.getNextValue();
            const float dryGain = dry.getNextValue();
            const float wetGain = wet.getNextValue();
            const float mix = bypass.getNextValue();

            for (int ch = 0; ch < channels; ++ch)
            {
                const float in = audio.getSample (ch, i);
                float delayed = read (ch, delaySamples);
                if (! std::isfinite (delayed))
                    delayed = 0.0f;

                tone[(size_t) ch] += (delayed - tone[(size_t) ch]) * 0.32f;
                if (! std::isfinite (tone[(size_t) ch]))
                    tone[(size_t) ch] = 0.0f;

                const float loop = fb * std::tanh (tone[(size_t) ch]);
                buffer[(size_t) ch][(size_t) write] = contain (in + loop);

                const float effected = contain (in * dryGain + delayed * wetGain);
                audio.setSample (ch, i, in + (effected - in) * mix);
            }

            if (++write >= size)
                write = 0;
        }
    }

private:
    float read (int channel, float delaySamples) const
    {
        float readPos = (float) write - delaySamples;
        const float length = (float) size;
        readPos = std::fmod (readPos, length);
        if (readPos < 0.0f)
            readPos += length;

        const int i1 = (int) readPos;
        const float frac = readPos - (float) i1;

        auto at = [this, channel] (int index)
        {
            index %= size;
            if (index < 0)
                index += size;
            return buffer[(size_t) channel][(size_t) index];
        };

        const float xm1 = at (i1 - 1);
        const float x0 = at (i1);
        const float x1 = at (i1 + 1);
        const float x2 = at (i1 + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * frac + c2) * frac + c1) * frac + x0;
    }

    // Leaves ordinary levels untouched and stops a feedback cascade climbing without bound.
    static float contain (float sample)
    {
        if (! std::isfinite (sample))
            return 0.0f;

        constexpr float threshold = 1.5f;
        const float magnitude = std::abs (sample);
        if (magnitude <= threshold)
            return sample;

        const float sign = sample < 0.0f ? -1.0f : 1.0f;
        return sign * (threshold + 0.5f * std::tanh ((magnitude - threshold) / 0.5f));
    }

    double sr = 48000.0;
    int size = 0;
    int write = 0;
    bool primed = false;
    float tone[2] { 0.0f, 0.0f };
    juce::SmoothedValue<float> timeMs, feedback, dry, wet, bypass;
    std::array<std::vector<float>, 2> buffer;
};
