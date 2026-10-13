#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <memory>

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Content;

    std::unique_ptr<Content> content;

    static constexpr int designWidth = 1024;
    static constexpr int designHeight = 1015;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
