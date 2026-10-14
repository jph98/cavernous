#pragma once

#include <JuceHeader.h>
#include "Dsp/LayerVoice.h"
#include "Dsp/ToneSource.h"
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
    void armMidiAction (int action);
    void cancelMidiLearn();
    bool isMidiArmed() const { return armedParam.load() >= 0 || armedAction.load() >= 0; }
    int getArmedParameterIndex() const { return armedParam.load(); }
    int getArmedAction() const { return armedAction.load(); }
    juce::String getArmedParameterName() const;
    void clearMidiMapping (const juce::String& parameterId);
    void clearMidiAction (int action);
    int getMappedController (const juce::String& parameterId) const;
    int getMappedAction (int action) const;
    void triggerTestTone();
    void killTrails();
    void knockTanks();
    void killAll();
    void fadeOut();
    void fadeAll();

private:
    struct MidiSlot
    {
        std::atomic<int> paramIndex { -1 };
        std::atomic<int> action { -1 };
        std::atomic<int> channel { 0 };
    };

    LayerSettings settingsForLayer (int layer) const;
    void handleMidi (const juce::MidiBuffer& midi);
    void applyAction (int action);
    void setChoice (const juce::String& parameterId, int index);
    void setInt (const juce::String& parameterId, int value);
    void clearMappingForIndex (int parameterIndex);
    void retargetMapping (int fromIndex, int toIndex);
    static float softClip (float sample);

    std::array<LayerVoice, kMaxLayers> voices;
    std::array<juce::SmoothedValue<float>, kMaxLayers> level;
    juce::AudioBuffer<float> scratch;
    juce::AudioBuffer<float> droneBus;
    juce::AudioBuffer<float> sum;

    std::atomic<float>* numLayers = nullptr;
    std::atomic<float>* octave[kDelayCount][kMaxLayers] {};
    std::atomic<float>* delayTime[kDelayCount][kMaxLayers] {};
    std::atomic<float>* delayFeedback[kDelayCount][kMaxLayers] {};
    std::atomic<float>* delayDry[kDelayCount][kMaxLayers] {};
    std::atomic<float>* delayWet[kDelayCount][kMaxLayers] {};
    std::atomic<float>* delayOn[kDelayCount][kMaxLayers] {};
    std::atomic<float>* flutterOn[kDelayCount][kMaxLayers] {};
    std::atomic<float>* flutterMs[kDelayCount][kMaxLayers] {};
    std::atomic<float>* reverbDry[kDelayCount][kMaxLayers] {};
    std::atomic<float>* reverbWet[kDelayCount][kMaxLayers] {};
    std::atomic<float>* reverbDecay[kDelayCount][kMaxLayers] {};
    std::atomic<float>* reverbOn[kDelayCount][kMaxLayers] {};
    std::atomic<float>* reverbType[kDelayCount][kMaxLayers] {};
    std::atomic<float>* reverbShift[kDelayCount][kMaxLayers] {};
    std::atomic<float>* reverbDiffusion[kDelayCount][kMaxLayers] {};
    std::atomic<float>* reverbLow[kDelayCount][kMaxLayers] {};
    std::atomic<float>* reverbHigh[kDelayCount][kMaxLayers] {};
    std::atomic<float>* reverbSize[kDelayCount][kMaxLayers] {};
    std::atomic<float>* layerLevel[kMaxLayers] {};
    std::atomic<float>* kill = nullptr;
    std::atomic<float>* toneKey[kMaxLayers] {};
    std::atomic<float>* toneOctave[kMaxLayers] {};
    std::atomic<float>* toneOvertones[kMaxLayers] {};
    std::atomic<float>* toneNoise[kMaxLayers] {};
    std::atomic<float>* toneLength[kMaxLayers] {};
    std::atomic<float>* tonePitch[kMaxLayers] {};
    std::atomic<float>* fadeRelease[kMaxLayers] {};
    std::atomic<float>* kickPitch[kMaxLayers] {};
    std::atomic<float>* droneOn[kMaxLayers] {};
    std::atomic<float>* droneMode[kMaxLayers] {};
    std::atomic<float>* droneDegree[kMaxLayers][kDroneDegreeCount] {};

    std::array<MidiSlot, 128> midiSlots;
    std::array<int, 128> lastCc {};
    std::atomic<int> armedParam { -1 };
    std::atomic<int> armedAction { -1 };
    std::atomic<int> selectedLayer { 0 };
    std::atomic<int> stateGeneration { 0 };
    double currentSampleRate = 44100.0;
    bool prepared = false;
    std::array<std::atomic<bool>, kMaxLayers> toneArmed {};
    std::atomic<bool> trailsArmed { false };
    std::atomic<bool> killAllArmed { false };
    std::array<std::atomic<bool>, kMaxLayers> fadeArmed {};
    enum class LayerFade { idle, falling, returning };
    std::array<LayerFade, kMaxLayers> layerFade {};
    std::array<juce::SmoothedValue<float>, kMaxLayers> fadeGain;
    std::array<ToneBurst, kMaxLayers> toneBurst;
    std::array<DroneTones, kMaxLayers> droneTones;

    void renderLayerSource (int layer, juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& droneOut);
    void silenceLayer (int layer);
    void cancelLayerFade (int layer);
    void beginLayerFade (int layer);
    void beginLayerReturn (int layer);
    void restoreTrailParameters();

    static constexpr const char* layerParameterNames[] = {
        "octave", "delayTime", "delayFeedback", "delayDry", "delayWet", "delayOn",
        "reverbDry", "reverbWet", "reverbDecay", "reverbOn", "level",
        "delayTime2", "delayFeedback2", "delayDry2", "delayWet2", "delayOn2",
        "delayTime3", "delayFeedback3", "delayDry3", "delayWet3", "delayOn3",
        "delayTime4", "delayFeedback4", "delayDry4", "delayWet4", "delayOn4",
        "octave2", "octave3", "octave4",
        "reverbType",
        "reverbDry2", "reverbWet2", "reverbDecay2", "reverbOn2", "reverbType2",
        "reverbDry3", "reverbWet3", "reverbDecay3", "reverbOn3", "reverbType3",
        "reverbDry4", "reverbWet4", "reverbDecay4", "reverbOn4", "reverbType4",
        "reverbShift", "reverbDiffusion", "reverbLow", "reverbHigh",
        "reverbShift2", "reverbDiffusion2", "reverbLow2", "reverbHigh2",
        "reverbShift3", "reverbDiffusion3", "reverbLow3", "reverbHigh3",
        "reverbShift4", "reverbDiffusion4", "reverbLow4", "reverbHigh4",
        "reverbSize", "reverbSize2", "reverbSize3", "reverbSize4",
        "flutterOn", "flutterMs", "flutterOn2", "flutterMs2",
        "flutterOn3", "flutterMs3", "flutterOn4", "flutterMs4",
        "kickPitch",
        "toneKey", "toneOctave", "toneOvertones", "toneNoise", "toneLength",
        "tonePitch", "fadeRelease",
        "droneMode", "droneOn",
        "drone1", "drone2", "drone3", "drone4", "drone5", "drone6", "drone7"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
