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
    juce::Viewport viewport;

    static constexpr int designWidth = 1428;
    static constexpr int designHeight = 776;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
