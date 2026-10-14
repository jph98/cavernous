#pragma once

#include "PitchShifter.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
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
            activeLength[i] = lengths[i];
            tank[(size_t) i].assign ((size_t) lengths[i] * 3, 0.0f);
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

        earlyLength = (int) std::round (0.30 * sampleRate) + 8;
        early[0].assign ((size_t) earlyLength, 0.0f);
        early[1].assign ((size_t) earlyLength, 0.0f);
        earlyPos = 0;

        static constexpr float tapMs[6] = { 23.0f, 37.0f, 51.0f, 68.0f, 89.0f, 113.0f };
        for (int i = 0; i < 6; ++i)
            taps[i] = juce::jlimit (1, earlyLength - 2, (int) std::round (tapMs[i] * 0.001 * sampleRate));

        for (int i = 0; i < 6; ++i)
            activeTaps[i] = taps[i];

        dry.reset (sampleRate, 0.03);
        wet.reset (sampleRate, 0.03);
        bypass.reset (sampleRate, 0.015);
        decay.reset (sampleRate, 0.08);
        shimmerShift.prepare (sampleRate, 0.09);
        shiftSmooth.reset (sampleRate, 0.05);
        diffusionSmooth.reset (sampleRate, 0.05);
        lowCutSmooth.reset (sampleRate, 0.08);
        highCutSmooth.reset (sampleRate, 0.08);
        shiftSmooth.setCurrentAndTargetValue (12.0f);
        diffusionSmooth.setCurrentAndTargetValue (80.0f);
        lowCutSmooth.setCurrentAndTargetValue (150.0f);
        highCutSmooth.setCurrentAndTargetValue (8000.0f);
        primed = false;
        currentType = -1;
        currentSize = -1.0f;
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
        shimmerShift.reset();
        shimmerFeedbackL = shimmerFeedbackR = 0.0f;
        knockArmed.store (false);
        knockLeft = 0;
        knockChirp = 0.0;
        knockThud = 0.0;
        knockSpring[0] = knockSpring[1] = knockSpring[2] = 0.0;
        for (int i = 0; i < 2; ++i)
        {
            feedbackHpL[i] = feedbackHpR[i] = 0.0f;
            feedbackLpL[i] = feedbackLpR[i] = 0.0f;
        }
    }

    void setParameters (float dryDb, float wetDb, float decaySeconds, int type, bool enabled,
                        float shiftSemitones, float diffusionPercent, float lowCutHz, float highCutHz,
                        float sizePercent = 100.0f)
    {
        applySpace (type, sizePercent);
        shiftSmooth.setTargetValue (juce::jlimit (-12.0f, 12.0f, shiftSemitones));
        diffusionSmooth.setTargetValue (juce::jlimit (0.0f, 100.0f, diffusionPercent));
        lowCutSmooth.setTargetValue (juce::jlimit (20.0f, 2000.0f, lowCutHz));
        highCutSmooth.setTargetValue (juce::jlimit (200.0f, 16000.0f, highCutHz));
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
            shiftSmooth.setCurrentAndTargetValue (shiftSmooth.getTargetValue());
            diffusionSmooth.setCurrentAndTargetValue (diffusionSmooth.getTargetValue());
            lowCutSmooth.setCurrentAndTargetValue (lowCutSmooth.getTargetValue());
            highCutSmooth.setCurrentAndTargetValue (highCutSmooth.getTargetValue());
            updateTargets (decay.getCurrentValue());
            for (int i = 0; i < 8; ++i)
                gain[i] = targetGain[i];
            primed = true;
        }
    }

    // Kick the spring tank at a chosen pitch. Safe from the UI thread.
    void knock (float pitchHz = 185.0f) noexcept
    {
        knockPitch.store (juce::jlimit (40.0f, 1200.0f, pitchHz));
        knockArmed.store (true);
    }

    // A held drone is mixed into the tank only, scaled so the loop settles
    // instead of running away. It is not added to the dry feed.
    void process (juce::AudioBuffer<float>& audio, const juce::AudioBuffer<float>* drone = nullptr)
    {
        const int numSamples = audio.getNumSamples();
        auto* left = audio.getWritePointer (0);
        auto* right = audio.getNumChannels() > 1 ? audio.getWritePointer (1) : left;

        for (int i = 0; i < numSamples; ++i)
        {
            if (modDepth > 0.0f)
                modPhase += 0.0017f;

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
            const float dryGain = dry.getNextValue();
            const float wetGain = wet.getNextValue();
            const float mix = bypass.getNextValue();

            if (knockArmed.exchange (false))
            {
                knockHz = juce::jlimit (40.0f, 1200.0f, knockPitch.load());
                knockDetune = 1.013f + nextKnockNoise() * 0.006f;
                knockLeft = juce::jmax (1, (int) std::round (0.46 * sr));
                knockTotal = knockLeft;
                knockChirp = knockThud = 0.0;
                knockSpring[0] = knockSpring[1] = knockSpring[2] = 0.0;
            }

            float knockSample = 0.0f;
            if (knockLeft > 0)
            {
                const float elapsed = (float) (knockTotal - knockLeft) / (float) sr;
                const float impact = nextKnockNoise() * std::exp (-elapsed * 280.0f);
                const float chirpT = juce::jlimit (0.0f, 1.0f, elapsed / 0.075f);
                const float chirpHz = knockHz * std::pow (4.2f, 1.0f - chirpT);
                knockChirp += juce::MathConstants<double>::twoPi * (double) chirpHz / sr;
                const float chirp = std::sin ((float) knockChirp) * std::exp (-elapsed * 16.0f);

                const float partialHz[3] = { knockHz, knockHz * knockDetune, knockHz * 2.73f };
                constexpr float partialGain[3] = { 0.62f, 0.48f, 0.22f };
                float ring = 0.0f;

                for (int partial = 0; partial < 3; ++partial)
                {
                    knockSpring[partial] += juce::MathConstants<double>::twoPi * (double) partialHz[partial] / sr;
                    if (knockSpring[partial] > juce::MathConstants<double>::twoPi)
                        knockSpring[partial] -= juce::MathConstants<double>::twoPi;
                    ring += std::sin ((float) knockSpring[partial]) * partialGain[partial];
                }

                const float bounce = 0.82f + 0.18f * std::sin (elapsed * 78.0f);
                ring *= std::exp (-elapsed * 5.8f) * bounce;

                knockThud += juce::MathConstants<double>::twoPi * (double) knockHz * 0.48 / sr;
                const float thud = std::sin ((float) knockThud) * std::exp (-elapsed * 26.0f);
                knockSample = impact * 0.9f + chirp * 0.62f + ring * 0.5f + thud * 0.4f;

                if (! std::isfinite (knockSample))
                    knockSample = 0.0f;

                --knockLeft;
            }

            float exciteL = 0.0f;
            float exciteR = 0.0f;
            if (drone != nullptr && i < drone->getNumSamples() && drone->getNumChannels() > 0)
            {
                exciteL = drone->getSample (0, i);
                exciteR = drone->getNumChannels() > 1 ? drone->getSample (1, i) : exciteL;
            }

            const float srcL = inL + shimmerFeedbackL;
            const float srcR = inR + shimmerFeedbackR;

            float diffused = processAllpass (0, 0.5f * (srcL + srcR), coeff[0]);
            diffused = processAllpass (1, diffused, coeff[1]);
            float diffusedR = processAllpass (2, srcR, coeff[2]);
            diffusedR = processAllpass (3, diffusedR, coeff[3]);

            float delayed[8];
            for (int line = 0; line < 8; ++line)
            {
                float raw = readTank (line);
                if (! std::isfinite (raw))
                    raw = 0.0f;

                lowpass[line] += (raw - lowpass[line]) * damping;
                if (! std::isfinite (lowpass[line]))
                    lowpass[line] = 0.0f;
                delayed[line] = lowpass[line];
            }

            float mixed[8];
            for (int line = 0; line < 8; ++line)
                mixed[line] = delayed[line];
            hadamard (mixed);

            for (int line = 0; line < 8; ++line)
            {
                const float injection = ((line & 1) != 0 ? diffusedR : diffused) * 0.32f;
                const float room = juce::jlimit (0.0f, 1.0f, 1.0f - targetGain[line]) * mix;
                const float excite = ((line & 1) != 0 ? exciteR : exciteL) * room * 0.45f;
                static constexpr float knockSign[8] = { 1.0f, -0.85f, 0.7f, -1.0f, 0.9f, -0.6f, 0.75f, -0.8f };
                float written = mixed[line] * gain[line] + injection + excite + knockSample * knockSign[line];
                if (softLimit)
                    written = softClip (written);
                tank[(size_t) line][(size_t) position[line]] = contain (written);
                if (++position[line] >= activeLength[line])
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
                int index = earlyPos - activeTaps[tap];
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

            float wetL = tankL + earlyL * earlyMix + knockSample * 0.9f;
            float wetR = tankR + earlyR * earlyMix + knockSample * 0.75f;
            shimmerFeedbackL = 0.0f;
            shimmerFeedbackR = 0.0f;

            const float shiftNow = shiftSmooth.getNextValue();
            const float diffuseNow = diffusionSmooth.getNextValue() * 0.01f;
            const float lowHz = lowCutSmooth.getNextValue();
            const float highHz = juce::jmax (lowHz + 80.0f, highCutSmooth.getNextValue());

            if (shimmer > 0.0f)
            {
                const float amount = juce::jlimit (0.0f, 1.0f, diffuseNow);
                const float weights[4] = { 1.0f, 0.92f, 0.97f, 0.84f };
                const float coeffBase = 0.015f + amount * 0.74f;
                for (int n = 0; n < 4; ++n)
                    coeff[n] = juce::jmin (0.84f, coeffBase * weights[n]);
                earlyMix = 0.08f + amount * 0.8f;

                const float open = std::log (highHz / 400.0f) / std::log (16000.0f / 400.0f);
                damping = 0.08f + 0.78f * juce::jlimit (0.0f, 1.0f, open);

                const float hpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * lowHz / (float) sr);
                const float lpCoeff = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * highHz / (float) sr);

                float upL = wetL;
                float upR = wetR;
                shimmerShift.shiftBy (upL, upR, shiftNow);
                upL = filterFeedback (upL, hpCoeff, lpCoeff, feedbackHpL, feedbackLpL);
                upR = filterFeedback (upR, hpCoeff, lpCoeff, feedbackHpR, feedbackLpR);

                const float decayNorm = std::log (juce::jmax (0.5f, decaySeconds) / 0.5f) / std::log (20.0f);
                const float regen = 0.1f + 0.34f * juce::jlimit (0.0f, 1.0f, decayNorm);
                shimmerFeedbackL = upL * regen;
                shimmerFeedbackR = upR * regen;

                if (std::abs (shiftNow) >= 0.02f)
                {
                    wetL += upL * 0.48f;
                    wetR += upR * 0.48f;
                }
            }
            const float outL = inL * dryGain + wetL * wetGain;
            const float outR = inR * dryGain + wetR * wetGain;

            left[i] = inL + (outL - inL) * mix;
            right[i] = inR + (outR - inR) * mix;
        }
    }

private:
    void updateTargets (float decaySeconds)
    {
        const float seconds = juce::jmax (0.05f, decaySeconds);
        const float osc = juce::jlimit (0.0f, 1.0f, (seconds - 5.0f) / 5.0f);
        softLimit = osc > 0.001f;
        const float denominator = seconds * (float) sr;

        for (int i = 0; i < 8; ++i)
        {
            const float stable = std::exp (-6.907755f * (float) activeLength[i] / denominator);
            targetGain[i] = softLimit ? stable + osc * (1.08f - stable) : stable;
        }
    }

    void applySpace (int type, float sizePercent)
    {
        type = juce::jlimit (0, 5, type);
        const float size = juce::jlimit (0.2f, 2.0f, sizePercent * 0.01f);
        const bool typeChanged = type != currentType;
        const bool sizeChanged = std::abs (size - currentSize) >= 0.002f;

        if (! typeChanged && ! sizeChanged)
            return;

        currentSize = size;
        lastDecay = -1.0f;

        if (typeChanged)
        {
            currentType = type;
            typeScale = 1.0f;
            damping = 0.42f;
            earlyMix = 0.65f;
            shimmer = 0.0f;
            modDepth = 0.0f;
            float diffusion = 1.0f;

            switch (type)
            {
                case 1: typeScale = 0.42f; damping = 0.78f; earlyMix = 0.35f; diffusion = 1.15f; break;
                case 2: typeScale = 1.35f; damping = 0.30f; earlyMix = 0.90f; diffusion = 0.95f; break;
                case 3: typeScale = 0.28f; damping = 0.62f; earlyMix = 1.15f; diffusion = 1.05f; break;
                case 4: typeScale = 0.58f; damping = 0.74f; earlyMix = 0.22f; diffusion = 0.85f; modDepth = 14.0f; break;
                case 5: shimmer = 1.0f; typeScale = 1.22f; damping = 0.55f; earlyMix = 0.72f; modDepth = 8.0f; break;
                default: break;
            }

            const float bases[4] = { 0.62f, 0.55f, 0.6f, 0.5f };
            for (int i = 0; i < 4; ++i)
                coeff[i] = juce::jmin (0.9f, bases[i] * diffusion);
        }

        const float fitted = typeScale * size;

        for (int i = 0; i < 8; ++i)
        {
            const int capacity = (int) tank[(size_t) i].size();
            activeLength[i] = juce::jlimit (8, juce::jmax (8, capacity), (int) std::round ((float) lengths[i] * fitted));
            if (activeLength[i] > 0)
                position[i] %= activeLength[i];
        }

        for (int tap = 0; tap < 6; ++tap)
            activeTaps[tap] = juce::jlimit (1, juce::jmax (1, earlyLength - 2),
                                            (int) std::round ((float) taps[tap] * size));
    }

    float nextKnockNoise()
    {
        knockRng ^= knockRng << 13;
        knockRng ^= knockRng >> 17;
        knockRng ^= knockRng << 5;
        return (float) ((int) (knockRng & 0x7fffffffu) / 1073741824.0) - 1.0f;
    }

    float readTank (int line) const
    {
        if (modDepth <= 0.0f)
            return tank[(size_t) line][(size_t) position[line]];

        const float phase = (float) position[line] + modDepth * std::sin (modPhase + (float) line);
        int index = (int) std::floor (phase);
        const float frac = phase - (float) index;
        const int length = activeLength[line];
        index %= length;
        if (index < 0)
            index += length;
        const int next = (index + 1) % length;
        return tank[(size_t) line][(size_t) index] * (1.0f - frac)
             + tank[(size_t) line][(size_t) next] * frac;
    }

    static float filterFeedback (float sample, float hpCoeff, float lpCoeff, float* hpState, float* lpState)
    {
        hpState[0] += (sample - hpState[0]) * hpCoeff;
        float highpassed = sample - hpState[0];
        hpState[1] += (highpassed - hpState[1]) * hpCoeff;
        highpassed -= hpState[1];

        lpState[0] += (highpassed - lpState[0]) * lpCoeff;
        lpState[1] += (lpState[0] - lpState[1]) * lpCoeff;

        if (! std::isfinite (lpState[1]))
        {
            hpState[0] = hpState[1] = lpState[0] = lpState[1] = 0.0f;
            return 0.0f;
        }

        return lpState[1];
    }

    static float softClip (float sample)
    {
        constexpr float threshold = 0.9f;
        const float magnitude = std::abs (sample);
        if (magnitude <= threshold)
            return sample;
        const float sign = sample < 0.0f ? -1.0f : 1.0f;
        return sign * (threshold + (1.0f - threshold) * std::tanh ((magnitude - threshold) / (1.0f - threshold)));
    }

    static float contain (float sample)
    {
        if (! std::isfinite (sample))
            return 0.0f;

        constexpr float threshold = 2.0f;
        const float magnitude = std::abs (sample);
        if (magnitude <= threshold)
            return sample;

        const float sign = sample < 0.0f ? -1.0f : 1.0f;
        return sign * (threshold + std::tanh (magnitude - threshold));
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
    int activeLength[8] {};
    int position[8] {};
    int currentType = -1;
    float currentSize = -1.0f;
    float typeScale = 1.0f;
    float damping = 0.42f;
    float earlyMix = 0.65f;
    float shimmer = 0.0f;
    float modDepth = 0.0f;
    float modPhase = 0.0f;
    float coeff[4] { 0.62f, 0.55f, 0.6f, 0.5f };
    bool softLimit = false;
    float lowpass[8] {};
    float gain[8] {};
    PitchShifter shimmerShift;
    float shimmerFeedbackL = 0.0f;
    float shimmerFeedbackR = 0.0f;
    float feedbackHpL[2] {};
    float feedbackHpR[2] {};
    float feedbackLpL[2] {};
    float feedbackLpR[2] {};
    juce::SmoothedValue<float> shiftSmooth, diffusionSmooth, lowCutSmooth, highCutSmooth;
    float targetGain[8] {};
    std::array<std::vector<float>, 8> tank;

    int allpassLength[4] {};
    int allpassPos[4] {};
    std::array<std::vector<float>, 4> allpass;

    std::array<std::vector<float>, 2> early;
    int earlyLength = 0;
    int earlyPos = 0;
    int taps[6] {};
    int activeTaps[6] {};
    std::atomic<bool> knockArmed { false };
    std::atomic<float> knockPitch { 185.0f };
    int knockLeft = 0;
    int knockTotal = 1;
    float knockHz = 185.0f;
    float knockDetune = 1.013f;
    double knockChirp = 0.0;
    double knockThud = 0.0;
    double knockSpring[3] {};
    uint32_t knockRng = 0x51A7C3E1u;

    float lastDecay = -1.0f;
    bool primed = false;
    juce::SmoothedValue<float> dry, wet, bypass, decay;
};
