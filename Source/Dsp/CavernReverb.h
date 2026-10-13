#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>

// Eight-line feedback delay network with a decay time in seconds.
class CavernReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const double scale = sampleRate / 48000.0;

        static constexpr int baseDelays[8] = { 1789, 2131, 2557, 3001, 3463, 4001, 4621, 5303 };
        for (int i = 0; i < 8; ++i)
        {
            lengths[i] = juce::jmax (64, (int) std::round (baseDelays[i] * scale));
            tank[(size_t) i].assign ((size_t) lengths[i], 0.0f);
            position[i] = 0;
            lowpass[i] = 0.0f;
            gain[i] = 0.4f;
            targetGain[i] = 0.4f;
        }

        static constexpr int allpassBase[4] = { 241, 373, 509, 647 };
        for (int i = 0; i < 4; ++i)
        {
            allpassLength[i] = juce::jmax (16, (int) std::round (allpassBase[i] * scale));
            allpass[(size_t) i].assign ((size_t) allpassLength[i], 0.0f);
            allpassPos[i] = 0;
        }

        earlyLength = (int) std::round (0.16 * sampleRate) + 8;
        early[0].assign ((size_t) earlyLength, 0.0f);
        early[1].assign ((size_t) earlyLength, 0.0f);
        earlyPos = 0;

        static constexpr float tapMs[6] = { 23.0f, 37.0f, 51.0f, 68.0f, 89.0f, 113.0f };
        for (int i = 0; i < 6; ++i)
            taps[i] = juce::jlimit (1, earlyLength - 2, (int) std::round (tapMs[i] * 0.001 * sampleRate));

        dry.reset (sampleRate, 0.03);
        wet.reset (sampleRate, 0.03);
        bypass.reset (sampleRate, 0.015);
        decay.reset (sampleRate, 0.08);
        primed = false;
        reset();
    }

    void reset()
    {
        for (int i = 0; i < 8; ++i)
        {
            std::fill (tank[(size_t) i].begin(), tank[(size_t) i].end(), 0.0f);
            position[i] = 0;
            lowpass[i] = 0.0f;
        }

        for (int i = 0; i < 4; ++i)
        {
            std::fill (allpass[(size_t) i].begin(), allpass[(size_t) i].end(), 0.0f);
            allpassPos[i] = 0;
        }

        std::fill (early[0].begin(), early[0].end(), 0.0f);
        std::fill (early[1].begin(), early[1].end(), 0.0f);
        earlyPos = 0;
        lastDecay = -1.0f;
    }

    void setParameters (float dryDb, float wetDb, float decaySeconds, bool enabled)
    {
        dry.setTargetValue (juce::Decibels::decibelsToGain (dryDb, -80.0f));
        wet.setTargetValue (juce::Decibels::decibelsToGain (wetDb, -80.0f));
        bypass.setTargetValue (enabled ? 1.0f : 0.0f);
        decay.setTargetValue (juce::jlimit (0.2f, 20.0f, decaySeconds));

        if (! primed)
        {
            dry.setCurrentAndTargetValue (dry.getTargetValue());
            wet.setCurrentAndTargetValue (wet.getTargetValue());
            bypass.setCurrentAndTargetValue (bypass.getTargetValue());
            decay.setCurrentAndTargetValue (decay.getTargetValue());
            updateTargets (decay.getCurrentValue());
            for (int i = 0; i < 8; ++i)
                gain[i] = targetGain[i];
            primed = true;
        }
    }

    void process (juce::AudioBuffer<float>& audio)
    {
        const int numSamples = audio.getNumSamples();
        auto* left = audio.getWritePointer (0);
        auto* right = audio.getNumChannels() > 1 ? audio.getWritePointer (1) : left;

        for (int i = 0; i < numSamples; ++i)
        {
            const float decaySeconds = decay.getNextValue();
            if (std::abs (decaySeconds - lastDecay) > 0.01f)
            {
                lastDecay = decaySeconds;
                updateTargets (decaySeconds);
            }

            for (int line = 0; line < 8; ++line)
                gain[line] += (targetGain[line] - gain[line]) * 0.002f;

            const float inL = left[i];
            const float inR = right[i];

            float diffused = processAllpass (0, 0.5f * (inL + inR), 0.62f);
            diffused = processAllpass (1, diffused, 0.55f);
            float diffusedR = processAllpass (2, inR, 0.6f);
            diffusedR = processAllpass (3, diffusedR, 0.5f);

            float delayed[8];
            for (int line = 0; line < 8; ++line)
            {
                delayed[line] = tank[(size_t) line][(size_t) position[line]];
                lowpass[line] += (delayed[line] - lowpass[line]) * 0.42f;
                delayed[line] = lowpass[line];
            }

            float mixed[8];
            for (int line = 0; line < 8; ++line)
                mixed[line] = delayed[line];
            hadamard (mixed);

            for (int line = 0; line < 8; ++line)
            {
                const float injection = ((line & 1) != 0 ? diffusedR : diffused) * 0.32f;
                tank[(size_t) line][(size_t) position[line]] = mixed[line] * gain[line] + injection;
                if (++position[line] >= lengths[line])
                    position[line] = 0;
            }

            const float tankL = (delayed[0] + delayed[2] + delayed[4] + delayed[6]) * 0.35f;
            const float tankR = (delayed[1] - delayed[3] + delayed[5] - delayed[7]) * 0.35f;

            early[0][(size_t) earlyPos] = inL;
            early[1][(size_t) earlyPos] = inR;

            float earlyL = 0.0f;
            float earlyR = 0.0f;
            static constexpr float tapGain[6] = { 0.42f, -0.30f, 0.26f, -0.20f, 0.16f, -0.12f };

            for (int tap = 0; tap < 6; ++tap)
            {
                int index = earlyPos - taps[tap];
                if (index < 0)
                    index += earlyLength;

                const float g = tapGain[tap];
                if ((tap & 1) != 0)
                {
                    earlyL += g * early[1][(size_t) index];
                    earlyR += g * early[0][(size_t) index];
                }
                else
                {
                    earlyL += g * early[0][(size_t) index];
                    earlyR += g * early[1][(size_t) index];
                }
            }

            if (++earlyPos >= earlyLength)
                earlyPos = 0;

            const float wetL = tankL + earlyL * 0.65f;
            const float wetR = tankR + earlyR * 0.65f;
            const float dryGain = dry.getNextValue();
            const float wetGain = wet.getNextValue();
            const float mix = bypass.getNextValue();
            const float outL = inL * dryGain + wetL * wetGain;
            const float outR = inR * dryGain + wetR * wetGain;

            left[i] = inL + (outL - inL) * mix;
            right[i] = inR + (outR - inR) * mix;
        }
    }

private:
    void updateTargets (float decaySeconds)
    {
        const float denominator = juce::jmax (0.05f, decaySeconds) * (float) sr;
        for (int i = 0; i < 8; ++i)
            targetGain[i] = std::exp (-6.907755f * (float) lengths[i] / denominator);
    }

    static void hadamard (float* data)
    {
        for (int stride = 1; stride < 8; stride *= 2)
        {
            for (int i = 0; i < 8; i += stride * 2)
            {
                for (int j = 0; j < stride; ++j)
                {
                    const float a = data[i + j];
                    const float b = data[i + j + stride];
                    data[i + j] = a + b;
                    data[i + j + stride] = a - b;
                }
            }
        }

        constexpr float scale = 0.35355339f;
        for (int i = 0; i < 8; ++i)
            data[i] *= scale;
    }

    float processAllpass (int index, float input, float coefficient)
    {
        const float delayed = allpass[(size_t) index][(size_t) allpassPos[index]];
        const float output = -coefficient * input + delayed;
        allpass[(size_t) index][(size_t) allpassPos[index]] = input + coefficient * output;
        if (++allpassPos[index] >= allpassLength[index])
            allpassPos[index] = 0;
        return output;
    }

    double sr = 48000.0;
    int lengths[8] {};
    int position[8] {};
    float lowpass[8] {};
    float gain[8] {};
    float targetGain[8] {};
    std::array<std::vector<float>, 8> tank;

    int allpassLength[4] {};
    int allpassPos[4] {};
    std::array<std::vector<float>, 4> allpass;

    std::array<std::vector<float>, 2> early;
    int earlyLength = 0;
    int earlyPos = 0;
    int taps[6] {};

    float lastDecay = -1.0f;
    bool primed = false;
    juce::SmoothedValue<float> dry, wet, bypass, decay;
};
