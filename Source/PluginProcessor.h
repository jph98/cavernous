#pragma once

#include <JuceHeader.h>
#include "Dsp/LayerVoice.h"
#include "Parameters.h"
#include <array>
#include <atomic>

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cavernous"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    int getNumLayers() const;
    void setNumLayers (int count);
    void removeLayer (int index);

    int getSelectedLayer() const { return selectedLayer.load(); }
    void setSelectedLayer (int index) { selectedLayer.store (index); }
    int getStateGeneration() const { return stateGeneration.load(); }

    void armMidiLearn (const juce::String& parameterId);
    void cancelMidiLearn();
    int getArmedParameterIndex() const { return armedParam.load(); }
    juce::String getArmedParameterName() const;
    void clearMidiMapping (const juce::String& parameterId);
    int getMappedController (const juce::String& parameterId) const;

private:
    struct MidiSlot
    {
        std::atomic<int> paramIndex { -1 };
        std::atomic<int> channel { 0 };
    };

    LayerSettings settingsForLayer (int layer) const;
    void handleMidi (const juce::MidiBuffer& midi);
    void clearMappingForIndex (int parameterIndex);
    void retargetMapping (int fromIndex, int toIndex);
    static float softClip (float sample);

    std::array<LayerVoice, kMaxLayers> voices;
    std::array<juce::SmoothedValue<float>, kMaxLayers> level;
    juce::AudioBuffer<float> scratch;
    juce::AudioBuffer<float> sum;

    std::atomic<float>* numLayers = nullptr;
    std::atomic<float>* octave[kMaxLayers] {};
    std::atomic<float>* delayTime[kMaxLayers] {};
    std::atomic<float>* delayFeedback[kMaxLayers] {};
    std::atomic<float>* delayDry[kMaxLayers] {};
    std::atomic<float>* delayWet[kMaxLayers] {};
    std::atomic<float>* delayOn[kMaxLayers] {};
    std::atomic<float>* reverbDry[kMaxLayers] {};
    std::atomic<float>* reverbWet[kMaxLayers] {};
    std::atomic<float>* reverbDecay[kMaxLayers] {};
    std::atomic<float>* reverbOn[kMaxLayers] {};
    std::atomic<float>* layerLevel[kMaxLayers] {};

    std::array<MidiSlot, 128> midiSlots;
    std::atomic<int> armedParam { -1 };
    std::atomic<int> selectedLayer { 0 };
    std::atomic<int> stateGeneration { 0 };
    double currentSampleRate = 44100.0;
    bool prepared = false;

    static constexpr const char* layerParameterNames[] = {
        "octave", "delayTime", "delayFeedback", "delayDry", "delayWet", "delayOn",
        "reverbDry", "reverbWet", "reverbDecay", "reverbOn", "level"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
