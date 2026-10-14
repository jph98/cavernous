#pragma once

#include <JuceHeader.h>

inline constexpr int kMaxLayers = 8;
inline constexpr int kDelayCount = 4;
inline constexpr int kReverbTypeCount = 6;

inline constexpr int addLayerAction = 1;
inline constexpr int removeLayerAction = 2;
inline constexpr int testToneAction = 3;

inline int selectLayerAction (int layer) { return 10 + layer; }
inline int octaveStepAction (int layer, int delay, int step)
{
    if (delay <= 0)
        return 100 + layer * 8 + step;

    return 420 + layer * 24 + (delay - 1) * 8 + step;
}
inline int reverbTypeAction (int layer, int delay, int type)
{
    if (delay <= 0)
        return 300 + layer * 8 + type;

    return 640 + layer * 24 + (delay - 1) * 8 + type;
}

inline const char* reverbTypeName (int index)
{
    static constexpr const char* names[] = { "Cavern", "Plate", "Hall", "Room", "Spring", "Shimmer" };
    return names[(size_t) juce::jlimit (0, kReverbTypeCount - 1, index)];
}

inline constexpr int kOctaveCount = 9;
inline constexpr int kOctaveOff = 4;

inline const char* octaveStepName (int step)
{
    static constexpr const char* names[] = { "-4", "-3", "-2", "-1", "Off", "+1", "+2", "+3", "+4" };
    return names[(size_t) juce::jlimit (0, kOctaveCount - 1, step)];
}

inline int octaveSemitones (int step)
{
    return (juce::jlimit (0, kOctaveCount - 1, step) - kOctaveOff) * 12;
}

// Older MIDI maps used five steps, -2..+2, stored as indexes 0..4.
inline int legacyOctaveIndex (int oldStep)
{
    return juce::jlimit (0, kOctaveCount - 1, oldStep + (kOctaveOff - 2));
}

inline juce::StringArray octaveChoiceNames()
{
    juce::StringArray names;
    for (int step = 0; step < kOctaveCount; ++step)
        names.add (octaveStepName (step));
    return names;
}

inline juce::AudioParameterChoiceAttributes octaveAttrs()
{
    using namespace juce;

    return AudioParameterChoiceAttributes()
        .withValueFromStringFunction ([] (const String& text)
        {
            const auto trimmed = text.trim();
            if (trimmed.equalsIgnoreCase ("off") || trimmed.isEmpty())
                return kOctaveOff;

            for (int step = 0; step < kOctaveCount; ++step)
                if (trimmed.equalsIgnoreCase (octaveStepName (step)))
                    return step;

            const int octaves = trimmed.getIntValue();
            if (octaves >= -4 && octaves <= 4)
                return octaves + kOctaveOff;

            return kOctaveOff;
        });
}

inline juce::String actionName (int action)
{
    if (action == addLayerAction)
        return "New Layer";
    if (action == removeLayerAction)
        return "Remove Layer";
    if (action == testToneAction)
        return "Test Tone";
    if (action >= 10 && action < 10 + kMaxLayers)
        return "Layer " + juce::String (action - 9);
    if (action >= 100 && action < 100 + kMaxLayers * 8)
    {
        const int step = (action - 100) % 8;
        return "Delay 1 Octave " + juce::String (octaveStepName (legacyOctaveIndex (step)));
    }
    if (action >= 420 && action < 420 + kMaxLayers * 24)
    {
        const int within = (action - 420) % 24;
        const int delay = 1 + within / 8;
        const int step = within % 8;
        return "Delay " + juce::String (delay + 1) + " Octave " + octaveStepName (legacyOctaveIndex (step));
    }
    if (action >= 300 && action < 300 + kMaxLayers * 8)
    {
        const int type = (action - 300) % 8;
        return "Delay 1 " + juce::String (reverbTypeName (type));
    }
    if (action >= 640 && action < 640 + kMaxLayers * 24)
    {
        const int within = (action - 640) % 24;
        const int delay = 1 + within / 8;
        const int type = within % 8;
        return "Delay " + juce::String (delay + 1) + " " + reverbTypeName (type);
    }

    return "Control";
}

inline juce::String delayFieldName (const char* field, int delayIndex)
{
    if (delayIndex <= 0)
        return field;

    return juce::String (field) + juce::String (delayIndex + 1);
}

inline juce::String layerParam (const char* name, int layer)
{
    return juce::String (name) + "_" + juce::String (layer);
}

inline juce::NormalisableRange<float> logRange (float start, float end)
{
    jassert (start > 0.0f && end > start);

    return { start, end,
             [] (float s, float e, float proportion)
             {
                 return s * std::pow (e / s, proportion);
             },
             [] (float s, float e, float value)
             {
                 const auto v = juce::jlimit (s, e, value);
                 return std::log (v / s) / std::log (e / s);
             } };
}

inline juce::AudioParameterFloatAttributes floatAttrs (const char* label)
{
    using namespace juce;

    return AudioParameterFloatAttributes()
        .withLabel (label)
        .withStringFromValueFunction ([] (float value, int) { return String (value, 2); })
        .withValueFromStringFunction ([] (const String& text) { return text.getFloatValue(); });
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;

    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    params.push_back (std::make_unique<AudioParameterInt> (
        ParameterID { "numLayers", 1 }, "Layers", 1, kMaxLayers, 1));

    for (int i = 0; i < kMaxLayers; ++i)
    {
        const auto suffix = " " + String (i + 1);
        const float levelDefault = i == 0 ? 0.0f : -6.0f;

        params.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { layerParam ("octave", i), 1 },
            "Octave" + suffix,
            octaveChoiceNames(),
            kOctaveOff,
            octaveAttrs()));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("delayTime", i), 1 },
            "Delay Time" + suffix,
            logRange (60.0f, 2000.0f),
            500.0f,
            floatAttrs ("ms")));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("delayFeedback", i), 1 },
            "Delay Feedback" + suffix,
            NormalisableRange<float> (0.0f, 100.0f),
            40.0f,
            floatAttrs ("%")));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("delayDry", i), 1 },
            "Delay Dry" + suffix,
            NormalisableRange<float> (-20.0f, 6.0f),
            0.0f,
            floatAttrs ("dB")));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("delayWet", i), 1 },
            "Delay Wet" + suffix,
            NormalisableRange<float> (-20.0f, 6.0f),
            0.0f,
            floatAttrs ("dB")));

        params.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { layerParam ("delayOn", i), 1 },
            "Delay On" + suffix,
            true));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("reverbDry", i), 1 },
            "Reverb Dry" + suffix,
            NormalisableRange<float> (-20.0f, 6.0f),
            0.0f,
            floatAttrs ("dB")));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("reverbWet", i), 1 },
            "Reverb Wet" + suffix,
            NormalisableRange<float> (-20.0f, 6.0f),
            0.0f,
            floatAttrs ("dB")));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("reverbDecay", i), 1 },
            "Reverb Decay" + suffix,
            logRange (0.5f, 10.0f),
            4.0f,
            floatAttrs ("s")));

        params.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { layerParam ("reverbOn", i), 1 },
            "Reverb On" + suffix,
            true));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("level", i), 1 },
            "Layer Level" + suffix,
            NormalisableRange<float> (-60.0f, 6.0f),
            levelDefault,
            floatAttrs ("dB")));
    }

    static constexpr float extraDelayMs[] = { 750.0f, 320.0f, 1100.0f };
    static constexpr float extraFeedback[] = { 30.0f, 25.0f, 20.0f };

    for (int i = 0; i < kMaxLayers; ++i)
    {
        const auto suffix = " " + String (i + 1);

        for (int delay = 1; delay < kDelayCount; ++delay)
        {
            const auto label = "Delay " + String (delay + 1);

            params.push_back (std::make_unique<AudioParameterChoice> (
                ParameterID { layerParam (delayFieldName ("octave", delay).toRawUTF8(), i), 1 },
                label + " Octave" + suffix,
                octaveChoiceNames(),
                kOctaveOff,
                octaveAttrs()));
            const auto timeName = delayFieldName ("delayTime", delay);
            const auto feedbackName = delayFieldName ("delayFeedback", delay);
            const auto dryName = delayFieldName ("delayDry", delay);
            const auto wetName = delayFieldName ("delayWet", delay);
            const auto onName = delayFieldName ("delayOn", delay);

            params.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { layerParam (timeName.toRawUTF8(), i), 1 },
                label + " Time" + suffix,
                logRange (60.0f, 2000.0f),
                extraDelayMs[delay - 1],
                floatAttrs ("ms")));

            params.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { layerParam (feedbackName.toRawUTF8(), i), 1 },
                label + " Feedback" + suffix,
                NormalisableRange<float> (0.0f, 100.0f),
                extraFeedback[delay - 1],
                floatAttrs ("%")));

            params.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { layerParam (dryName.toRawUTF8(), i), 1 },
                label + " Dry" + suffix,
                NormalisableRange<float> (-20.0f, 6.0f),
                0.0f,
                floatAttrs ("dB")));

            params.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { layerParam (wetName.toRawUTF8(), i), 1 },
                label + " Wet" + suffix,
                NormalisableRange<float> (-20.0f, 6.0f),
                0.0f,
                floatAttrs ("dB")));

            params.push_back (std::make_unique<AudioParameterBool> (
                ParameterID { layerParam (onName.toRawUTF8(), i), 1 },
                label + " On" + suffix,
                false));

            const auto reverbLabel = "Reverb " + String (delay + 1);
            params.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { layerParam (delayFieldName ("reverbDry", delay).toRawUTF8(), i), 1 },
                reverbLabel + " Dry" + suffix,
                NormalisableRange<float> (-20.0f, 6.0f),
                0.0f,
                floatAttrs ("dB")));
            params.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { layerParam (delayFieldName ("reverbWet", delay).toRawUTF8(), i), 1 },
                reverbLabel + " Wet" + suffix,
                NormalisableRange<float> (-20.0f, 6.0f),
                0.0f,
                floatAttrs ("dB")));
            params.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { layerParam (delayFieldName ("reverbDecay", delay).toRawUTF8(), i), 1 },
                reverbLabel + " Decay" + suffix,
                logRange (0.5f, 10.0f),
                4.0f,
                floatAttrs ("s")));
            params.push_back (std::make_unique<AudioParameterBool> (
                ParameterID { layerParam (delayFieldName ("reverbOn", delay).toRawUTF8(), i), 1 },
                reverbLabel + " On" + suffix,
                false));

            StringArray stageTypes;
            for (int type = 0; type < kReverbTypeCount; ++type)
                stageTypes.add (reverbTypeName (type));

            params.push_back (std::make_unique<AudioParameterChoice> (
                ParameterID { layerParam (delayFieldName ("reverbType", delay).toRawUTF8(), i), 1 },
                reverbLabel + " Type" + suffix,
                stageTypes,
                0));
        }

        StringArray reverbTypes;
        for (int type = 0; type < kReverbTypeCount; ++type)
            reverbTypes.add (reverbTypeName (type));

        params.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { layerParam ("reverbType", i), 1 },
            "Reverb Type" + suffix,
            reverbTypes,
            0));
    }

    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { "kill", 1 }, "Kill Sound", false));

    const auto shiftAttrs = AudioParameterFloatAttributes()
                                .withLabel ("st")
                                .withStringFromValueFunction ([] (float value, int)
                                {
                                    const auto text = String (std::abs (value) < 0.005f ? 0.0f : value, 2);
                                    return value > 0.005f ? "+" + text : text;
                                })
                                .withValueFromStringFunction ([] (const String& text) { return text.getFloatValue(); });

    for (int i = 0; i < kMaxLayers; ++i)
    {
        const auto suffix = " " + String (i + 1);

        for (int delay = 0; delay < kDelayCount; ++delay)
        {
            const auto label = "Reverb " + String (delay + 1);

            params.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { layerParam (delayFieldName ("reverbShift", delay).toRawUTF8(), i), 1 },
                label + " Shift" + suffix,
                NormalisableRange<float> (-12.0f, 12.0f),
                12.0f,
                shiftAttrs));
            params.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { layerParam (delayFieldName ("reverbDiffusion", delay).toRawUTF8(), i), 1 },
                label + " Diffusion" + suffix,
                NormalisableRange<float> (0.0f, 100.0f),
                80.0f,
                floatAttrs ("%")));
            params.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { layerParam (delayFieldName ("reverbLow", delay).toRawUTF8(), i), 1 },
                label + " Low Cut" + suffix,
                logRange (20.0f, 2000.0f),
                150.0f,
                floatAttrs ("Hz")));
            params.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { layerParam (delayFieldName ("reverbHigh", delay).toRawUTF8(), i), 1 },
                label + " High Cut" + suffix,
                logRange (200.0f, 16000.0f),
                8000.0f,
                floatAttrs ("Hz")));
        }
    }

    return { params.begin(), params.end() };
}
