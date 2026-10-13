#pragma once

#include <JuceHeader.h>

inline constexpr int kMaxLayers = 8;

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
            StringArray { "-2", "-1", "Off", "+1", "+2" },
            2));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("delayTime", i), 1 },
            "Delay Time" + suffix,
            logRange (60.0f, 2000.0f),
            500.0f,
            AudioParameterFloatAttributes().withLabel ("ms")));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("delayFeedback", i), 1 },
            "Delay Feedback" + suffix,
            NormalisableRange<float> (0.0f, 100.0f),
            40.0f,
            AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("delayDry", i), 1 },
            "Delay Dry" + suffix,
            NormalisableRange<float> (-20.0f, 6.0f),
            0.0f,
            AudioParameterFloatAttributes().withLabel ("dB")));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("delayWet", i), 1 },
            "Delay Wet" + suffix,
            NormalisableRange<float> (-20.0f, 6.0f),
            0.0f,
            AudioParameterFloatAttributes().withLabel ("dB")));

        params.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { layerParam ("delayOn", i), 1 },
            "Delay On" + suffix,
            true));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("reverbDry", i), 1 },
            "Reverb Dry" + suffix,
            NormalisableRange<float> (-20.0f, 6.0f),
            0.0f,
            AudioParameterFloatAttributes().withLabel ("dB")));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("reverbWet", i), 1 },
            "Reverb Wet" + suffix,
            NormalisableRange<float> (-20.0f, 6.0f),
            0.0f,
            AudioParameterFloatAttributes().withLabel ("dB")));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("reverbDecay", i), 1 },
            "Reverb Decay" + suffix,
            logRange (0.5f, 10.0f),
            4.0f,
            AudioParameterFloatAttributes().withLabel ("s")));

        params.push_back (std::make_unique<AudioParameterBool> (
            ParameterID { layerParam ("reverbOn", i), 1 },
            "Reverb On" + suffix,
            true));

        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { layerParam ("level", i), 1 },
            "Layer Level" + suffix,
            NormalisableRange<float> (-60.0f, 6.0f),
            levelDefault,
            AudioParameterFloatAttributes().withLabel ("dB")));
    }

    return { params.begin(), params.end() };
}
