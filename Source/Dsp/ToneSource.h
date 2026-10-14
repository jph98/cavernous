#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdint>

inline constexpr int kToneKeyCount = 12;
inline constexpr int kToneOctaveMin = 1;
inline constexpr int kToneOctaveMax = 6;
inline constexpr int kToneOvertoneMax = 8;
inline constexpr int kNoiseColourCount = 5;
inline constexpr int kDroneDegreeCount = 7;
inline constexpr int kDroneModeCount = 3;

inline const char* toneKeyName (int key)
{
    static constexpr const char* names[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    return names[(size_t) juce::jlimit (0, kToneKeyCount - 1, key)];
}

inline const char* noiseColourName (int colour)
{
    static constexpr const char* names[] = { "Off", "White", "Pink", "Brown", "Blue" };
    return names[(size_t) juce::jlimit (0, kNoiseColourCount - 1, colour)];
}

inline const char* droneModeName (int mode)
{
    static constexpr const char* names[] = { "Major", "Minor", "Dorian" };
    return names[(size_t) juce::jlimit (0, kDroneModeCount - 1, mode)];
}

inline int droneScaleSemitone (int mode, int degree)
{
    static constexpr int scales[kDroneModeCount][kDroneDegreeCount] = {
        { 0, 2, 4, 5, 7, 9, 11 },
        { 0, 2, 3, 5, 7, 8, 10 },
        { 0, 2, 3, 5, 7, 9, 10 }
    };

    return scales[(size_t) juce::jlimit (0, kDroneModeCount - 1, mode)]
                 [(size_t) juce::jlimit (0, kDroneDegreeCount - 1, degree)];
}

// Fundamental in Hz for a scientific-pitch octave. Octave 4, key C is middle C.
// semitones shifts the note lower (negative) or higher (positive).
inline float toneFrequencyHz (int key, int octave, float semitones = 0.0f)
{
    const float midi = (float) ((juce::jlimit (kToneOctaveMin, kToneOctaveMax, octave) + 1) * 12
                                + juce::jlimit (0, kToneKeyCount - 1, key))
                       + semitones;
    return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f);
}

inline float droneNoteHz (int key, int octave, int mode, int degree, float semitones = 0.0f)
{
    const int semis = juce::jlimit (0, kToneKeyCount - 1, key) + droneScaleSemitone (mode, degree);
    const int wrapped = semis % 12;
    const int noteOctave = juce::jlimit (kToneOctaveMin, kToneOctaveMax, octave + semis / 12);
    return toneFrequencyHz (wrapped, noteOctave, semitones);
}

// Short struck tone plus optional coloured noise, mixed into the plugin input.
class ToneBurst
{
public:
    void trigger (double sampleRate, int key, int octave, int overtones, int noise, float lengthSeconds = 1.0f,
                  float semitones = 0.0f)
    {
        sr = juce::jmax (1.0, sampleRate);
        partials = 1 + juce::jlimit (0, kToneOvertoneMax, overtones);
        noiseColour = juce::jlimit (0, kNoiseColourCount - 1, noise);
        noteKey = juce::jlimit (0, kToneKeyCount - 1, key);
        noteOctave = juce::jlimit (kToneOctaveMin, kToneOctaveMax, octave);
        transpose = semitones;
        fundamental = toneFrequencyHz (noteKey, noteOctave, transpose);

        weight = 0.0f;
        for (int harmonic = 1; harmonic <= partials; ++harmonic)
            weight += 1.0f / (float) harmonic;

        const float length = juce::jlimit (0.05f, 60.0f, lengthSeconds);
        samplesLeft = juce::jmax (1, (int) std::round (length * sr));
        total = samplesLeft;
        attackSamples = juce::jmax (1, (int) std::round (0.008 * sr));
        releaseSamples = juce::jmax (1, (int) std::round (0.045 * sr));

        if (attackSamples + releaseSamples > total)
        {
            attackSamples = juce::jmax (1, total / 5);
            releaseSamples = juce::jmax (1, total / 4);
        }

        for (auto& phase : phases)
            phase = 0.0;

        for (auto& stage : pink)
            stage = 0.0f;
        brown = 0.0f;
        previous = 0.0f;
        rng = 0xA341316Cu;
    }

    void stop() noexcept
    {
        samplesLeft = 0;
    }

    void setTranspose (float semitones) noexcept
    {
        transpose = semitones;
        fundamental = toneFrequencyHz (noteKey, noteOctave, transpose);
    }

    void addTo (juce::AudioBuffer<float>& buffer)
    {
        if (samplesLeft <= 0 || sr <= 0.0)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        const double step = juce::MathConstants<double>::twoPi * (double) fundamental / sr;
        const float gain = 0.34f / juce::jmax (0.001f, weight);

        for (int i = 0; i < numSamples && samplesLeft > 0; ++i)
        {
            const int elapsed = total - samplesLeft;
            float env = 1.0f;
            if (elapsed < attackSamples)
                env = (float) elapsed / (float) attackSamples;
            if (samplesLeft < releaseSamples)
                env = juce::jmin (env, (float) samplesLeft / (float) releaseSamples);
            float tone = 0.0f;

            for (int harmonic = 1; harmonic <= partials; ++harmonic)
            {
                tone += std::sin ((float) phases[(size_t) harmonic]) / (float) harmonic;
                phases[(size_t) harmonic] += step * (double) harmonic;
                if (phases[(size_t) harmonic] > juce::MathConstants<double>::twoPi)
                    phases[(size_t) harmonic] -= juce::MathConstants<double>::twoPi;
            }

            const float sample = env * (tone * gain + 0.22f * nextNoise());

            for (int channel = 0; channel < numChannels; ++channel)
                buffer.addSample (channel, i, sample);

            --samplesLeft;
        }
    }

    // One sample of the selected noise colour, roughly in -1..1. Off returns silence.
    float nextNoise()
    {
        if (noiseColour == 0)
            return 0.0f;

        const float white = nextWhite();

        if (noiseColour == 1)
            return white;

        if (noiseColour == 2)
            return pinkNoise (white);

        if (noiseColour == 3)
        {
            brown = brown * 0.996f + white * 0.028f;
            return juce::jlimit (-1.0f, 1.0f, brown * 3.4f);
        }

        const float blue = juce::jlimit (-1.0f, 1.0f, (white - previous) * 0.7f);
        previous = white;
        return blue;
    }

private:
    float nextWhite()
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (float) ((int) (rng & 0x7fffffffu) / 1073741824.0) - 1.0f;
    }

    float pinkNoise (float white)
    {
        pink[0] = 0.99886f * pink[0] + white * 0.0555179f;
        pink[1] = 0.99332f * pink[1] + white * 0.0750759f;
        pink[2] = 0.96900f * pink[2] + white * 0.1538520f;
        pink[3] = 0.86650f * pink[3] + white * 0.3104856f;
        pink[4] = 0.55000f * pink[4] + white * 0.5329522f;
        pink[5] = -0.7616f * pink[5] - white * 0.0168980f;
        const float pinkSample = pink[0] + pink[1] + pink[2] + pink[3] + pink[4] + pink[5] + pink[6] + white * 0.5362f;
        pink[6] = white * 0.115926f;
        return juce::jlimit (-1.0f, 1.0f, pinkSample * 0.11f);
    }

    double sr = 48000.0;
    double fundamental = 261.6f;
    int noteKey = 0;
    int noteOctave = 4;
    float transpose = 0.0f;
    double phases[kToneOvertoneMax + 2] {};
    int partials = 1;
    int noiseColour = 0;
    int samplesLeft = 0;
    int total = 1;
    int attackSamples = 1;
    int releaseSamples = 1;
    float weight = 1.0f;
    float pink[7] {};
    float brown = 0.0f;
    float previous = 0.0f;
    uint32_t rng = 0xA341316Cu;
};

// Held scale tones in the chosen key. Degrees fade in and out so the drone can be reshaped live.
class DroneTones
{
public:
    void prepare (double sampleRate)
    {
        sr = juce::jmax (1.0, sampleRate);
        smoothing = 1.0f - std::exp (-1.0f / (float) (0.02 * sr));
    }

    void setNotes (int key, int noteOctave, int scaleMode, int overtones, const bool* degrees, bool enabled,
                   float semitones = 0.0f)
    {
        tonic = juce::jlimit (0, kToneKeyCount - 1, key);
        octave = juce::jlimit (kToneOctaveMin, kToneOctaveMax, noteOctave);
        transpose = semitones;
        mode = juce::jlimit (0, kDroneModeCount - 1, scaleMode);
        partials = 1 + juce::jlimit (0, kToneOvertoneMax, overtones);

        weight = 0.0f;
        for (int harmonic = 1; harmonic <= partials; ++harmonic)
            weight += 1.0f / (float) harmonic;

        for (int degree = 0; degree < kDroneDegreeCount; ++degree)
            target[(size_t) degree] = enabled && degrees != nullptr && degrees[degree] ? 1.0f : 0.0f;
    }

    void silence() noexcept
    {
        for (int degree = 0; degree < kDroneDegreeCount; ++degree)
        {
            target[(size_t) degree] = 0.0f;
            gain[(size_t) degree] = 0.0f;
        }
    }

    void addTo (juce::AudioBuffer<float>& buffer)
    {
        if (sr <= 0.0)
            return;

        float sum = 0.0f;
        double step[kDroneDegreeCount] {};

        for (int degree = 0; degree < kDroneDegreeCount; ++degree)
        {
            auto& level = gain[(size_t) degree];
            level += smoothing * (target[(size_t) degree] - level);
            if (level < 0.0001f)
                level = 0.0f;

            sum += level;
            const float hz = droneNoteHz (tonic, octave, mode, degree, transpose);
            step[degree] = juce::MathConstants<double>::twoPi * (double) hz / sr;
        }

        if (sum < 0.0001f)
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        const float master = 0.30f / (juce::jmax (1.0f, sum) * juce::jmax (0.001f, weight));

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = 0.0f;

            for (int degree = 0; degree < kDroneDegreeCount; ++degree)
            {
                const float level = gain[(size_t) degree];
                if (level <= 0.0f)
                    continue;

                float tone = 0.0f;
                for (int harmonic = 1; harmonic <= partials; ++harmonic)
                {
                    auto& phase = phases[(size_t) degree][(size_t) harmonic];
                    tone += std::sin ((float) phase) / (float) harmonic;
                    phase += step[degree] * (double) harmonic;
                    if (phase > juce::MathConstants<double>::twoPi)
                        phase -= juce::MathConstants<double>::twoPi;
                }

                sample += tone * level;
            }

            sample *= master;

            for (int channel = 0; channel < numChannels; ++channel)
                buffer.addSample (channel, i, sample);
        }
    }

private:
    double sr = 0.0;
    int tonic = 0;
    int octave = 3;
    int mode = 0;
    float transpose = 0.0f;
    int partials = 1;
    float weight = 1.0f;
    float smoothing = 0.01f;
    float gain[kDroneDegreeCount] {};
    float target[kDroneDegreeCount] {};
    double phases[kDroneDegreeCount][kToneOvertoneMax + 2] {};
};
