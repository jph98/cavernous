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
        octave[i] = apvts.getRawParameterValue (layerParam ("octave", i));
        delayTime[i] = apvts.getRawParameterValue (layerParam ("delayTime", i));
        delayFeedback[i] = apvts.getRawParameterValue (layerParam ("delayFeedback", i));
        delayDry[i] = apvts.getRawParameterValue (layerParam ("delayDry", i));
        delayWet[i] = apvts.getRawParameterValue (layerParam ("delayWet", i));
        delayOn[i] = apvts.getRawParameterValue (layerParam ("delayOn", i));
        reverbDry[i] = apvts.getRawParameterValue (layerParam ("reverbDry", i));
        reverbWet[i] = apvts.getRawParameterValue (layerParam ("reverbWet", i));
        reverbDecay[i] = apvts.getRawParameterValue (layerParam ("reverbDecay", i));
        reverbOn[i] = apvts.getRawParameterValue (layerParam ("reverbOn", i));
        layerLevel[i] = apvts.getRawParameterValue (layerParam ("level", i));
    }
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    scratch.setSize (2, samplesPerBlock, false, true, true);
    sum.setSize (2, samplesPerBlock, false, true, true);

    for (int i = 0; i < kMaxLayers; ++i)
    {
        voices[(size_t) i].prepare (sampleRate);
        level[(size_t) i].reset (sampleRate, 0.03);
        level[(size_t) i].setCurrentAndTargetValue (juce::Decibels::decibelsToGain (layerLevel[i]->load(), -100.0f));
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
    const int octaveIndex = juce::jlimit (0, 4, (int) std::round (octave[layer]->load()));

    LayerSettings settings;
    settings.semitones = (octaveIndex - 2) * 12;
    settings.delayMs = delayTime[layer]->load();
    settings.feedback = delayFeedback[layer]->load();
    settings.delayDryDb = delayDry[layer]->load();
    settings.delayWetDb = delayWet[layer]->load();
    settings.delayOn = delayOn[layer]->load() >= 0.5f;
    settings.reverbDryDb = reverbDry[layer]->load();
    settings.reverbWetDb = reverbWet[layer]->load();
    settings.decaySeconds = reverbDecay[layer]->load();
    settings.reverbOn = reverbOn[layer]->load() >= 0.5f;
    return settings;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    if (! prepared)
        return;

    handleMidi (midi);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    if (numSamples > scratch.getNumSamples())
    {
        scratch.setSize (2, numSamples, false, false, true);
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

        voices[(size_t) layer].process (scratch, settingsForLayer (layer));

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

float PluginProcessor::softClip (float sample)
{
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

        const int armed = armedParam.load();
        if (armed >= 0)
        {
            for (auto& slot : midiSlots)
                if (slot.paramIndex.load() == armed)
                    slot.paramIndex.store (-1);

            midiSlots[(size_t) cc].paramIndex.store (armed);
            midiSlots[(size_t) cc].channel.store (0);
            armedParam.store (-1);
        }

        const int target = midiSlots[(size_t) cc].paramIndex.load();
        auto& parameters = getParameters();

        if (target >= 0 && target < parameters.size())
            parameters[target]->setValueNotifyingHost ((float) message.getControllerValue() / 127.0f);
    }
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

    auto parameterIndex = [this] (const char* name, int layer)
    {
        return apvts.getParameter (layerParam (name, layer))->getParameterIndex();
    };

    for (int destination = index; destination < count - 1; ++destination)
    {
        for (const char* name : layerParameterNames)
        {
            auto* source = apvts.getParameter (layerParam (name, destination + 1));
            auto* dest = apvts.getParameter (layerParam (name, destination));
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
    if (auto* parameter = apvts.getParameter (parameterId))
        armedParam.store (parameter->getParameterIndex());
}

void PluginProcessor::cancelMidiLearn()
{
    armedParam.store (-1);
}

juce::String PluginProcessor::getArmedParameterName() const
{
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

    if (auto parameters = apvts.copyState().createXml())
        root.addChildElement (parameters.release());

    auto* midi = root.createNewChildElement ("midi");

    for (int cc = 0; cc < 128; ++cc)
    {
        const int parameterIndex = midiSlots[(size_t) cc].paramIndex.load();
        if (parameterIndex < 0)
            continue;

        auto* mapping = midi->createNewChildElement ("map");
        mapping->setAttribute ("cc", cc);
        mapping->setAttribute ("param", parameterIndex);
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
        apvts.replaceState (juce::ValueTree::fromXml (*parameters));

    for (auto& slot : midiSlots)
        slot.paramIndex.store (-1);

    if (auto* midi = root->getChildByName ("midi"))
    {
        const int parameterCount = getParameters().size();

        for (auto* mapping : midi->getChildIterator())
        {
            const int cc = mapping->getIntAttribute ("cc", -1);
            const int parameterIndex = mapping->getIntAttribute ("param", -1);

            if (cc >= 0 && cc < 128 && parameterIndex >= 0 && parameterIndex < parameterCount)
                midiSlots[(size_t) cc].paramIndex.store (parameterIndex);
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
