#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    numLayers = apvts.getRawParameterValue ("numLayers");

    for (int i = 0; i < kMaxLayers; ++i)
    {
        layerLevel[i] = apvts.getRawParameterValue (layerParam ("level", i));

        for (int delay = 0; delay < kDelayCount; ++delay)
        {
            delayTime[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("delayTime", delay).toRawUTF8(), i));
            delayFeedback[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("delayFeedback", delay).toRawUTF8(), i));
            delayDry[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("delayDry", delay).toRawUTF8(), i));
            delayWet[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("delayWet", delay).toRawUTF8(), i));
            delayOn[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("delayOn", delay).toRawUTF8(), i));
            flutterOn[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("flutterOn", delay).toRawUTF8(), i));
            flutterMs[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("flutterMs", delay).toRawUTF8(), i));
            octave[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("octave", delay).toRawUTF8(), i));
            reverbDry[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("reverbDry", delay).toRawUTF8(), i));
            reverbWet[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("reverbWet", delay).toRawUTF8(), i));
            reverbDecay[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("reverbDecay", delay).toRawUTF8(), i));
            reverbOn[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("reverbOn", delay).toRawUTF8(), i));
            reverbType[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("reverbType", delay).toRawUTF8(), i));
            reverbShift[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("reverbShift", delay).toRawUTF8(), i));
            reverbDiffusion[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("reverbDiffusion", delay).toRawUTF8(), i));
            reverbLow[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("reverbLow", delay).toRawUTF8(), i));
            reverbHigh[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("reverbHigh", delay).toRawUTF8(), i));
            reverbSize[delay][i] = apvts.getRawParameterValue (layerParam (delayFieldName ("reverbSize", delay).toRawUTF8(), i));
        }
    }

    kill = apvts.getRawParameterValue ("kill");

    for (int layer = 0; layer < kMaxLayers; ++layer)
    {
        toneKey[layer] = apvts.getRawParameterValue (toneLayerParam ("toneKey", layer));
        toneOctave[layer] = apvts.getRawParameterValue (toneLayerParam ("toneOctave", layer));
        toneOvertones[layer] = apvts.getRawParameterValue (toneLayerParam ("toneOvertones", layer));
        toneNoise[layer] = apvts.getRawParameterValue (toneLayerParam ("toneNoise", layer));
        toneLength[layer] = apvts.getRawParameterValue (toneLayerParam ("toneLength", layer));
        tonePitch[layer] = apvts.getRawParameterValue (toneLayerParam ("tonePitch", layer));
        fadeRelease[layer] = apvts.getRawParameterValue (toneLayerParam ("fadeRelease", layer));
        kickPitch[layer] = apvts.getRawParameterValue (toneLayerParam ("kickPitch", layer));
        droneOn[layer] = apvts.getRawParameterValue (toneLayerParam ("droneOn", layer));
        droneMode[layer] = apvts.getRawParameterValue (toneLayerParam ("droneMode", layer));

        for (int degree = 0; degree < kDroneDegreeCount; ++degree)
            droneDegree[layer][degree] = apvts.getRawParameterValue (
                toneLayerParam (droneDegreeParam (degree).toRawUTF8(), layer));
    }
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    scratch.setSize (2, samplesPerBlock, false, true, true);
    droneBus.setSize (2, samplesPerBlock, false, true, true);
    sum.setSize (2, samplesPerBlock, false, true, true);

    for (int i = 0; i < kMaxLayers; ++i)
    {
        voices[(size_t) i].prepare (sampleRate);
        droneTones[(size_t) i].prepare (sampleRate);
        level[(size_t) i].reset (sampleRate, 0.03);
        level[(size_t) i].setCurrentAndTargetValue (juce::Decibels::decibelsToGain (layerLevel[i]->load(), -100.0f));
        fadeGain[(size_t) i].reset (sampleRate, 0.05);
        fadeGain[(size_t) i].setCurrentAndTargetValue (1.0f);
        layerFade[(size_t) i] = LayerFade::idle;
    }

    prepared = true;
}

void PluginProcessor::releaseResources()
{
    for (auto& voice : voices)
        voice.reset();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

LayerSettings PluginProcessor::settingsForLayer (int layer) const
{
    LayerSettings settings;

    for (int delay = 0; delay < kDelayCount; ++delay)
    {
        const int octaveIndex = juce::jlimit (0, kOctaveCount - 1, (int) std::round (octave[delay][layer]->load()));
        settings.delay[(size_t) delay].semitones = octaveSemitones (octaveIndex);
        settings.delay[(size_t) delay].delayMs = delayTime[delay][layer]->load();
        settings.delay[(size_t) delay].feedback = delayFeedback[delay][layer]->load();
        settings.delay[(size_t) delay].dryDb = delayDry[delay][layer]->load();
        settings.delay[(size_t) delay].wetDb = delayWet[delay][layer]->load();
        settings.delay[(size_t) delay].on = delayOn[delay][layer]->load() >= 0.5f;
        settings.delay[(size_t) delay].flutter = flutterOn[delay][layer]->load() >= 0.5f;
        settings.delay[(size_t) delay].flutterMs = flutterMs[delay][layer]->load();
        settings.reverb[(size_t) delay].dryDb = reverbDry[delay][layer]->load();
        settings.reverb[(size_t) delay].wetDb = reverbWet[delay][layer]->load();
        settings.reverb[(size_t) delay].decaySeconds = reverbDecay[delay][layer]->load();
        settings.reverb[(size_t) delay].on = reverbOn[delay][layer]->load() >= 0.5f;
        settings.reverb[(size_t) delay].type = juce::jlimit (0, kReverbTypeCount - 1,
                                                             (int) std::round (reverbType[delay][layer]->load()));
        settings.reverb[(size_t) delay].shiftSemitones = reverbShift[delay][layer]->load();
        settings.reverb[(size_t) delay].diffusion = reverbDiffusion[delay][layer]->load();
        settings.reverb[(size_t) delay].lowCutHz = reverbLow[delay][layer]->load();
        settings.reverb[(size_t) delay].highCutHz = reverbHigh[delay][layer]->load();
        settings.reverb[(size_t) delay].sizePercent = reverbSize[delay][layer]->load();
    }

    return settings;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    if (! prepared)
        return;

    handleMidi (midi);

    if (kill != nullptr && kill->load() >= 0.5f)
    {
        const int layer = (int) selectedLayer.load();
        silenceLayer (layer);
        cancelLayerFade (layer);

        if (auto* parameter = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("kill")))
            parameter->setValueNotifyingHost (0.0f);
    }

    if (killAllArmed.exchange (false))
    {
        for (int layer = 0; layer < kMaxLayers; ++layer)
        {
            silenceLayer (layer);
            cancelLayerFade (layer);
        }
    }

    if (trailsArmed.exchange (false))
    {
        for (auto& voice : voices)
            voice.killWet();
    }

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    if (numSamples > scratch.getNumSamples())
    {
        scratch.setSize (2, numSamples, false, false, true);
        droneBus.setSize (2, numSamples, false, false, true);
        sum.setSize (2, numSamples, false, false, true);
    }

    sum.clear (0, numSamples);

    const int activeLayers = getNumLayers();

    for (int layer = 0; layer < activeLayers; ++layer)
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            const int source = juce::jmin (channel, numChannels - 1);
            scratch.copyFrom (channel, 0, buffer, source, 0, numSamples);
        }

        if (fadeArmed[(size_t) layer].exchange (false))
            beginLayerFade (layer);

        renderLayerSource (layer, scratch, droneBus);
        voices[(size_t) layer].process (scratch, settingsForLayer (layer), &droneBus);

        const float fadeStart = fadeGain[(size_t) layer].getCurrentValue();
        fadeGain[(size_t) layer].skip (numSamples);
        const float fadeEnd = fadeGain[(size_t) layer].getCurrentValue();

        if (fadeStart < 0.9995f || fadeEnd < 0.9995f)
        {
            scratch.applyGainRamp (0, 0, numSamples, fadeStart, fadeEnd);
            scratch.applyGainRamp (1, 0, numSamples, fadeStart, fadeEnd);
        }

        if (layerFade[(size_t) layer] == LayerFade::falling && ! fadeGain[(size_t) layer].isSmoothing())
        {
            silenceLayer (layer);
            beginLayerReturn (layer);
        }
        else if (layerFade[(size_t) layer] == LayerFade::returning && ! fadeGain[(size_t) layer].isSmoothing())
        {
            layerFade[(size_t) layer] = LayerFade::idle;
        }

        level[(size_t) layer].setTargetValue (juce::Decibels::decibelsToGain (layerLevel[layer]->load(), -100.0f));
        const float start = level[(size_t) layer].getCurrentValue();
        level[(size_t) layer].skip (numSamples);
        const float end = level[(size_t) layer].getCurrentValue();

        sum.addFromWithRamp (0, 0, scratch.getReadPointer (0), numSamples, start, end);
        sum.addFromWithRamp (1, 0, scratch.getReadPointer (1), numSamples, start, end);
    }

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* output = buffer.getWritePointer (channel);
        const auto* mixed = sum.getReadPointer (juce::jmin (channel, 1));

        for (int i = 0; i < numSamples; ++i)
            output[i] = softClip (mixed[i]);
    }
}

void PluginProcessor::triggerTestTone()
{
    const int layer = juce::jlimit (0, kMaxLayers - 1, (int) selectedLayer.load());
    toneArmed[(size_t) layer].store (true);
}

void PluginProcessor::killAll()
{
    killAllArmed.store (true);
}

void PluginProcessor::fadeOut()
{
    const int layer = juce::jlimit (0, kMaxLayers - 1, (int) selectedLayer.load());
    fadeArmed[(size_t) layer].store (true);
}

void PluginProcessor::fadeAll()
{
    const int count = getNumLayers();

    for (int layer = 0; layer < count; ++layer)
        fadeArmed[(size_t) layer].store (true);
}

void PluginProcessor::cancelLayerFade (int layer)
{
    layer = juce::jlimit (0, kMaxLayers - 1, layer);
    fadeArmed[(size_t) layer].store (false);
    fadeGain[(size_t) layer].setCurrentAndTargetValue (1.0f);
    layerFade[(size_t) layer] = LayerFade::idle;
}

void PluginProcessor::beginLayerFade (int layer)
{
    const float release = fadeRelease[layer] != nullptr ? fadeRelease[layer]->load() : 2.0f;
    const float seconds = juce::jlimit (0.05f, 30.0f, release);
    const float current = fadeGain[(size_t) layer].getCurrentValue();
    fadeGain[(size_t) layer].reset (currentSampleRate, (double) seconds);
    fadeGain[(size_t) layer].setCurrentAndTargetValue (current);
    fadeGain[(size_t) layer].setTargetValue (0.0f);
    layerFade[(size_t) layer] = LayerFade::falling;
}

void PluginProcessor::beginLayerReturn (int layer)
{
    fadeGain[(size_t) layer].reset (currentSampleRate, 0.05);
    fadeGain[(size_t) layer].setCurrentAndTargetValue (0.0f);
    fadeGain[(size_t) layer].setTargetValue (1.0f);
    layerFade[(size_t) layer] = LayerFade::returning;
}

void PluginProcessor::silenceLayer (int layer)
{
    layer = juce::jlimit (0, kMaxLayers - 1, layer);
    toneArmed[(size_t) layer].store (false);
    toneBurst[(size_t) layer].stop();
    droneTones[(size_t) layer].silence();
    voices[(size_t) layer].killWet();

    if (auto* parameter = dynamic_cast<juce::AudioParameterBool*> (
            apvts.getParameter (toneLayerParam ("droneOn", layer))))
        parameter->setValueNotifyingHost (0.0f);
}

void PluginProcessor::knockTanks()
{
    const int layer = juce::jlimit (0, kMaxLayers - 1, (int) selectedLayer.load());
    const float pitch = kickPitch[layer] != nullptr ? kickPitch[layer]->load() : 185.0f;

    for (int delay = 0; delay < kDelayCount; ++delay)
        if (reverbOn[delay][layer] != nullptr && reverbOn[delay][layer]->load() >= 0.5f)
            voices[(size_t) layer].knock (delay, pitch);
}

void PluginProcessor::killTrails()
{
    trailsArmed.store (true);

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        restoreTrailParameters();
    else
        juce::MessageManager::callAsync ([this] { restoreTrailParameters(); });
}

void PluginProcessor::restoreTrailParameters()
{
    auto centre = [] (juce::RangedAudioParameter* parameter)
    {
        if (parameter == nullptr)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (0.5f);
        parameter->endChangeGesture();
    };

    for (int layer = 0; layer < kMaxLayers; ++layer)
    {
        for (int delay = 0; delay < kDelayCount; ++delay)
        {
            centre (apvts.getParameter (layerParam (delayFieldName ("delayFeedback", delay).toRawUTF8(), layer)));
            centre (apvts.getParameter (layerParam (delayFieldName ("reverbDecay", delay).toRawUTF8(), layer)));
        }
    }
}

void PluginProcessor::renderLayerSource (int layer, juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& droneOut)
{
    if (toneArmed[(size_t) layer].exchange (false))
    {
        const int key = toneKey[layer] != nullptr ? (int) std::round (toneKey[layer]->load()) : 0;
        const int octaveNumber = toneOctave[layer] != nullptr ? (int) std::round (toneOctave[layer]->load()) : 3;
        const int overtones = toneOvertones[layer] != nullptr ? (int) std::round (toneOvertones[layer]->load()) : 0;
        const int noise = toneNoise[layer] != nullptr ? (int) std::round (toneNoise[layer]->load()) : 0;
        const float length = toneLength[layer] != nullptr ? toneLength[layer]->load() : 1.0f;
        const float semitones = tonePitch[layer] != nullptr ? tonePitch[layer]->load() : 0.0f;
        toneBurst[(size_t) layer].trigger (currentSampleRate, key, octaveNumber, overtones, noise, length, semitones);
    }

    const float semitones = tonePitch[layer] != nullptr ? tonePitch[layer]->load() : 0.0f;
    toneBurst[(size_t) layer].setTranspose (semitones);
    toneBurst[(size_t) layer].addTo (buffer);

    bool degrees[kDroneDegreeCount] {};
    for (int degree = 0; degree < kDroneDegreeCount; ++degree)
        degrees[degree] = droneDegree[layer][degree] != nullptr && droneDegree[layer][degree]->load() >= 0.5f;

    const int key = toneKey[layer] != nullptr ? (int) std::round (toneKey[layer]->load()) : 0;
    const int octaveNumber = toneOctave[layer] != nullptr ? (int) std::round (toneOctave[layer]->load()) : 3;
    const int overtones = toneOvertones[layer] != nullptr ? (int) std::round (toneOvertones[layer]->load()) : 0;
    const int mode = droneMode[layer] != nullptr ? (int) std::round (droneMode[layer]->load()) : 0;
    const bool enabled = droneOn[layer] != nullptr && droneOn[layer]->load() >= 0.5f;

    droneTones[(size_t) layer].setNotes (key, octaveNumber, mode, overtones, degrees, enabled, semitones);
    droneOut.clear();
    droneTones[(size_t) layer].addTo (droneOut);
}

float PluginProcessor::softClip (float sample)
{
    if (! std::isfinite (sample))
        return 0.0f;

    constexpr float threshold = 0.92f;
    const float magnitude = std::abs (sample);

    if (magnitude <= threshold)
        return sample;

    const float sign = sample < 0.0f ? -1.0f : 1.0f;
    const float over = magnitude - threshold;
    return sign * (threshold + (1.0f - threshold) * std::tanh (over / (1.0f - threshold)));
}

void PluginProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (! message.isController())
            continue;

        const int cc = message.getControllerNumber();
        if (cc < 0 || cc > 127)
            continue;

        const int value = message.getControllerValue();
        const int armed = armedParam.load();
        const int armedAct = armedAction.load();

        if (armed >= 0 || armedAct >= 0)
        {
            if (armed >= 0)
            {
                for (auto& slot : midiSlots)
                    if (slot.paramIndex.load() == armed)
                        slot.paramIndex.store (-1);
            }

            if (armedAct >= 0)
            {
                for (auto& slot : midiSlots)
                    if (slot.action.load() == armedAct)
                        slot.action.store (-1);
            }

            auto& slot = midiSlots[(size_t) cc];
            slot.paramIndex.store (armed);
            slot.action.store (armedAct);
            slot.channel.store (0);
            armedParam.store (-1);
            armedAction.store (-1);
        }

        const int target = midiSlots[(size_t) cc].paramIndex.load();
        auto& parameters = getParameters();

        if (target >= 0 && target < parameters.size())
            parameters[target]->setValueNotifyingHost ((float) value / 127.0f);

        const int action = midiSlots[(size_t) cc].action.load();
        const int previous = lastCc[(size_t) cc];
        lastCc[(size_t) cc] = value;

        if (action >= 0 && value >= 64 && previous < 64)
            applyAction (action);
    }
}

void PluginProcessor::applyAction (int action)
{
    if (action == testToneAction)
    {
        triggerTestTone();
        return;
    }

    if (action == knockAction)
    {
        knockTanks();
        return;
    }

    if (action == killAllAction)
    {
        killAll();
        return;
    }

    if (action == fadeAction)
    {
        fadeOut();
        return;
    }

    if (action == fadeAllAction)
    {
        fadeAll();
        return;
    }

    if (action == killTrailsAction)
    {
        killTrails();
        return;
    }

    const int toneLayer = juce::jlimit (0, kMaxLayers - 1, (int) selectedLayer.load());

    if (action >= 800 && action < 800 + kToneKeyCount)
    {
        setChoice (toneLayerParam ("toneKey", toneLayer), action - 800);
        return;
    }

    if (action >= 820 && action < 820 + (kToneOctaveMax - kToneOctaveMin + 1))
    {
        setInt (toneLayerParam ("toneOctave", toneLayer), kToneOctaveMin + (action - 820));
        return;
    }

    if (action >= 830 && action < 830 + kToneOvertoneMax + 1)
    {
        setInt (toneLayerParam ("toneOvertones", toneLayer), action - 830);
        return;
    }

    if (action >= 840 && action < 840 + kNoiseColourCount)
    {
        setChoice (toneLayerParam ("toneNoise", toneLayer), action - 840);
        return;
    }

    if (action >= 850 && action < 850 + kDroneModeCount)
    {
        setChoice (toneLayerParam ("droneMode", toneLayer), action - 850);
        return;
    }

    if (action == addLayerAction || action == removeLayerAction)
    {
        juce::MessageManager::callAsync ([this, action]
        {
            if (action == addLayerAction)
                setNumLayers (getNumLayers() + 1);
            else
                removeLayer (getSelectedLayer());

            stateGeneration.fetch_add (1);
        });
        return;
    }

    if (action >= 10 && action < 10 + kMaxLayers)
    {
        selectedLayer.store (juce::jlimit (0, kMaxLayers - 1, action - 10));
        stateGeneration.fetch_add (1);
        return;
    }

    if (action >= 100 && action < 100 + kMaxLayers * 8)
    {
        const int layer = (action - 100) / 8;
        const int step = (action - 100) % 8;
        if (step <= 4)
            setChoice (layerParam ("octave", layer), legacyOctaveIndex (step));
        return;
    }

    if (action >= 420 && action < 420 + kMaxLayers * 24)
    {
        const int rel = action - 420;
        const int layer = rel / 24;
        const int within = rel % 24;
        const int delay = 1 + within / 8;
        const int step = within % 8;

        if (step <= 4 && delay < kDelayCount)
            setChoice (layerParam (delayFieldName ("octave", delay).toRawUTF8(), layer), legacyOctaveIndex (step));
        return;
    }

    if (action >= 300 && action < 300 + kMaxLayers * 8)
    {
        const int layer = (action - 300) / 8;
        const int type = (action - 300) % 8;
        if (type < kReverbTypeCount)
            setChoice (layerParam ("reverbType", layer), type);
        return;
    }

    if (action >= 640 && action < 640 + kMaxLayers * 24)
    {
        const int rel = action - 640;
        const int layer = rel / 24;
        const int within = rel % 24;
        const int delay = 1 + within / 8;
        const int type = within % 8;

        if (type < kReverbTypeCount && delay < kDelayCount)
            setChoice (layerParam (delayFieldName ("reverbType", delay).toRawUTF8(), layer), type);
    }
}

void PluginProcessor::setChoice (const juce::String& parameterId, int index)
{
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (parameterId)))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
}

void PluginProcessor::setInt (const juce::String& parameterId, int value)
{
    if (auto* parameter = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (parameterId)))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) value));
}

int PluginProcessor::getNumLayers() const
{
    return juce::jlimit (1, kMaxLayers, (int) std::round (numLayers->load()));
}

void PluginProcessor::setNumLayers (int count)
{
    if (auto* parameter = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter ("numLayers")))
    {
        const int clamped = juce::jlimit (1, kMaxLayers, count);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) clamped));
        parameter->endChangeGesture();
    }
}

void PluginProcessor::removeLayer (int index)
{
    const int count = getNumLayers();
    if (count <= 1)
        return;

    index = juce::jlimit (0, count - 1, index);

    auto parameterId = [] (const char* name, int layer)
    {
        return isToneLayerParameter (name) ? toneLayerParam (name, layer) : layerParam (name, layer);
    };

    auto parameterIndex = [this, &parameterId] (const char* name, int layer)
    {
        return apvts.getParameter (parameterId (name, layer))->getParameterIndex();
    };

    for (int destination = index; destination < count - 1; ++destination)
    {
        for (const char* name : layerParameterNames)
        {
            auto* source = apvts.getParameter (parameterId (name, destination + 1));
            auto* dest = apvts.getParameter (parameterId (name, destination));
            dest->setValueNotifyingHost (source->getValue());
            retargetMapping (parameterIndex (name, destination + 1), parameterIndex (name, destination));
        }
    }

    for (const char* name : layerParameterNames)
        clearMappingForIndex (parameterIndex (name, count - 1));

    setNumLayers (count - 1);

    if (selectedLayer.load() > count - 2)
        selectedLayer.store (juce::jmax (0, count - 2));
}

void PluginProcessor::armMidiLearn (const juce::String& parameterId)
{
    armedAction.store (-1);

    if (auto* parameter = apvts.getParameter (parameterId))
        armedParam.store (parameter->getParameterIndex());
}

void PluginProcessor::armMidiAction (int action)
{
    armedParam.store (-1);
    armedAction.store (action);
}

void PluginProcessor::cancelMidiLearn()
{
    armedParam.store (-1);
    armedAction.store (-1);
}

juce::String PluginProcessor::getArmedParameterName() const
{
    const int action = armedAction.load();
    if (action >= 0)
        return actionName (action);

    const int index = armedParam.load();
    auto& parameters = getParameters();

    if (index >= 0 && index < parameters.size())
        return parameters[index]->getName (48);

    return {};
}

void PluginProcessor::clearMidiMapping (const juce::String& parameterId)
{
    if (auto* parameter = apvts.getParameter (parameterId))
    {
        clearMappingForIndex (parameter->getParameterIndex());
        if (armedParam.load() == parameter->getParameterIndex())
            armedParam.store (-1);
    }
}

void PluginProcessor::clearMidiAction (int action)
{
    for (auto& slot : midiSlots)
        if (slot.action.load() == action)
            slot.action.store (-1);

    if (armedAction.load() == action)
        armedAction.store (-1);
}

int PluginProcessor::getMappedAction (int action) const
{
    for (int cc = 0; cc < 128; ++cc)
        if (midiSlots[(size_t) cc].action.load() == action)
            return cc;

    return -1;
}

int PluginProcessor::getMappedController (const juce::String& parameterId) const
{
    auto* parameter = apvts.getParameter (parameterId);
    if (parameter == nullptr)
        return -1;

    const int index = parameter->getParameterIndex();

    for (int cc = 0; cc < 128; ++cc)
        if (midiSlots[(size_t) cc].paramIndex.load() == index)
            return cc;

    return -1;
}

void PluginProcessor::clearMappingForIndex (int parameterIndex)
{
    for (auto& slot : midiSlots)
        if (slot.paramIndex.load() == parameterIndex)
            slot.paramIndex.store (-1);
}

void PluginProcessor::retargetMapping (int fromIndex, int toIndex)
{
    clearMappingForIndex (toIndex);

    for (auto& slot : midiSlots)
        if (slot.paramIndex.load() == fromIndex)
            slot.paramIndex.store (toIndex);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("Cavernous");
    root.setAttribute ("selected", selectedLayer.load());
    root.setAttribute ("version", 2);

    if (auto parameters = apvts.copyState().createXml())
        root.addChildElement (parameters.release());

    auto* midi = root.createNewChildElement ("midi");

    for (int cc = 0; cc < 128; ++cc)
    {
        const int parameterIndex = midiSlots[(size_t) cc].paramIndex.load();
        const int action = midiSlots[(size_t) cc].action.load();
        if (parameterIndex < 0 && action < 0)
            continue;

        auto* mapping = midi->createNewChildElement ("map");
        mapping->setAttribute ("cc", cc);
        if (parameterIndex >= 0)
            mapping->setAttribute ("param", parameterIndex);
        if (action >= 0)
            mapping->setAttribute ("action", action);
    }

    copyXmlToBinary (root, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto root = getXmlFromBinary (data, sizeInBytes);
    if (root == nullptr || ! root->hasTagName ("Cavernous"))
        return;

    selectedLayer.store (root->getIntAttribute ("selected", 0));

    if (auto* parameters = root->getChildByName ("PARAMS"))
    {
        if (root->getIntAttribute ("version", 1) < 2)
        {
            for (auto* param : parameters->getChildIterator())
            {
                const auto id = param->getStringAttribute ("id");
                if (! id.startsWith ("octave") || ! param->hasAttribute ("value"))
                    continue;

                const int oldIndex = juce::jlimit (0, 4, (int) std::round (param->getDoubleAttribute ("value")));
                param->setAttribute ("value", legacyOctaveIndex (oldIndex));
            }
        }

        apvts.replaceState (juce::ValueTree::fromXml (*parameters));
    }

    for (auto& slot : midiSlots)
    {
        slot.paramIndex.store (-1);
        slot.action.store (-1);
    }

    if (auto* midi = root->getChildByName ("midi"))
    {
        const int parameterCount = getParameters().size();

        for (auto* mapping : midi->getChildIterator())
        {
            const int cc = mapping->getIntAttribute ("cc", -1);
            if (cc < 0 || cc >= 128)
                continue;

            const int parameterIndex = mapping->getIntAttribute ("param", -1);
            const int action = mapping->getIntAttribute ("action", -1);

            if (parameterIndex >= 0 && parameterIndex < parameterCount)
                midiSlots[(size_t) cc].paramIndex.store (parameterIndex);
            if (action > 0)
                midiSlots[(size_t) cc].action.store (action);
        }
    }

    stateGeneration.fetch_add (1);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
