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

        for (auto& channel : droneBuffer)
            channel.assign ((size_t) size, 0.0f);

        timeMs.reset (sampleRate, 0.05);
        flutterMix.reset (sampleRate, 0.03);
        flutterDepth.reset (sampleRate, 0.05);
        flutterMix.setCurrentAndTargetValue (0.0f);
        flutterDepth.setCurrentAndTargetValue (0.0f);
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

        for (auto& channel : droneBuffer)
            std::fill (channel.begin(), channel.end(), 0.0f);

        write = 0;
        tone[0] = tone[1] = 0.0f;
        droneTone[0] = droneTone[1] = 0.0f;
    }

    void setParameters (float delayMs, float feedbackPercent, float dryDb, float wetDb, bool enabled,
                        bool flutterEnabled = false, float flutterIntervalMs = 0.0f)
    {
        timeMs.setTargetValue (juce::jlimit (1.0f, 2500.0f, delayMs));
        flutterMix.setTargetValue (flutterEnabled ? 1.0f : 0.0f);
        flutterDepth.setTargetValue (juce::jlimit (0.0f, 80.0f, flutterIntervalMs));
        feedback.setTargetValue (juce::jlimit (0.0f, 100.0f, feedbackPercent) * 0.01f);
        dry.setTargetValue (juce::Decibels::decibelsToGain (dryDb, -80.0f));
        wet.setTargetValue (juce::Decibels::decibelsToGain (wetDb, -80.0f));
        bypass.setTargetValue (enabled ? 1.0f : 0.0f);

        if (! primed)
        {
            timeMs.setCurrentAndTargetValue (timeMs.getTargetValue());
            flutterMix.setCurrentAndTargetValue (flutterMix.getTargetValue());
            flutterDepth.setCurrentAndTargetValue (flutterDepth.getTargetValue());
            feedback.setCurrentAndTargetValue (feedback.getTargetValue());
            dry.setCurrentAndTargetValue (dry.getTargetValue());
            wet.setCurrentAndTargetValue (wet.getTargetValue());
            bypass.setCurrentAndTargetValue (bypass.getTargetValue());
            primed = true;
        }
    }

    // `drone`, when present, is repeated in its own line and added to `droneWet`
    // so a held drone cannot recirculate through the next delay.
    void process (juce::AudioBuffer<float>& audio,
                  const juce::AudioBuffer<float>* drone = nullptr,
                  juce::AudioBuffer<float>* droneWet = nullptr)
    {
        const int numSamples = audio.getNumSamples();
        const int channels = juce::jmin (2, audio.getNumChannels());
        const float maxDelay = (float) (size - 4);

        for (int i = 0; i < numSamples; ++i)
        {
            const float depth = flutterDepth.getNextValue() * flutterMix.getNextValue();
            flutterPhase += (float) (juce::MathConstants<double>::twoPi * 5.6 / sr);
            flutterPhase2 += (float) (juce::MathConstants<double>::twoPi * 9.1 / sr);
            if (flutterPhase > juce::MathConstants<float>::twoPi)
                flutterPhase -= juce::MathConstants<float>::twoPi;
            if (flutterPhase2 > juce::MathConstants<float>::twoPi)
                flutterPhase2 -= juce::MathConstants<float>::twoPi;

            const float wobble = std::sin (flutterPhase) * 0.78f + std::sin (flutterPhase2) * 0.22f;
            const float delayMsNow = juce::jlimit (1.0f, 2500.0f, timeMs.getNextValue() + wobble * depth);
            const float delaySamples = juce::jlimit (1.0f, maxDelay, (float) (delayMsNow * 0.001 * sr));
            const float fb = feedback.getNextValue();
            const float dryGain = dry.getNextValue();
            const float wetGain = wet.getNextValue();
            const float mix = bypass.getNextValue();

            for (int ch = 0; ch < channels; ++ch)
            {
                const float in = audio.getSample (ch, i);
                float delayed = read (buffer[(size_t) ch], delaySamples);
                if (! std::isfinite (delayed))
                    delayed = 0.0f;

                tone[(size_t) ch] += (delayed - tone[(size_t) ch]) * 0.32f;
                if (! std::isfinite (tone[(size_t) ch]))
                    tone[(size_t) ch] = 0.0f;

                const float loop = fb * std::tanh (tone[(size_t) ch]);
                const float recorded = mix > 0.001f ? in + loop : loop * 0.9995f;
                buffer[(size_t) ch][(size_t) write] = contain (recorded);

                const float effected = contain (in * dryGain + delayed * wetGain);
                audio.setSample (ch, i, in + (effected - in) * mix);

                if (drone != nullptr && droneWet != nullptr
                    && ch < drone->getNumChannels() && i < drone->getNumSamples()
                    && ch < droneWet->getNumChannels() && i < droneWet->getNumSamples()
                    && (size_t) ch < droneBuffer.size() && ! droneBuffer[(size_t) ch].empty())
                {
                    float send = drone->getSample (ch, i) * (1.0f - fb);
                    float echoed = read (droneBuffer[(size_t) ch], delaySamples);
                    if (! std::isfinite (echoed))
                        echoed = 0.0f;

                    droneTone[(size_t) ch] += (echoed - droneTone[(size_t) ch]) * 0.32f;
                    const float droneLoop = fb * std::tanh (droneTone[(size_t) ch]);
                    const float droneRecorded = mix > 0.001f ? send + droneLoop : droneLoop * 0.9995f;
                    droneBuffer[(size_t) ch][(size_t) write] = contain (droneRecorded);
                    droneWet->addSample (ch, i, echoed * wetGain * mix);
                }
            }

            if (++write >= size)
                write = 0;
        }
    }

private:
    float read (const std::vector<float>& line, float delaySamples) const
    {
        if (line.empty() || size <= 0)
            return 0.0f;

        float readPos = (float) write - delaySamples;
        const float length = (float) size;
        readPos = std::fmod (readPos, length);
        if (readPos < 0.0f)
            readPos += length;

        const int i1 = (int) readPos;
        const float frac = readPos - (float) i1;

        auto at = [this, &line] (int index)
        {
            index %= size;
            if (index < 0)
                index += size;
            return line[(size_t) index];
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
    float droneTone[2] { 0.0f, 0.0f };
    float flutterPhase = 0.0f;
    float flutterPhase2 = 0.0f;
    juce::SmoothedValue<float> timeMs, flutterMix, flutterDepth, feedback, dry, wet, bypass;
    std::array<std::vector<float>, 2> buffer;
    std::array<std::vector<float>, 2> droneBuffer;
};
