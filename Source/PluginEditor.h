#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <memory>

class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::KeyListener
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Content;
    struct AboutPanel;

    using juce::Component::keyPressed;
    using juce::Component::keyStateChanged;

    void parentHierarchyChanged() override;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    bool keyStateChanged (bool isKeyDown, juce::Component*) override;
    void attachShortcutListener();

    std::unique_ptr<Content> content;
    std::unique_ptr<AboutPanel> aboutPanel;
    juce::Viewport viewport;
    juce::Component::SafePointer<juce::Component> shortcutRoot;
    juce::Array<int> heldKeyCodes;

    static constexpr int designWidth = 1660;
    static constexpr int designHeight = 856;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
