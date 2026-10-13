#include "PluginEditor.h"
#include "Ui/LearnControls.h"

namespace
{
constexpr int faceHeight = 927;

struct Slot
{
    int x, y, w, h;
};

constexpr Slot faders[] = {
    { 98, 106, 36, 170 },
    { 202, 106, 36, 170 },
    { 308, 106, 36, 170 },
    { 420, 106, 36, 170 },
    { 635, 106, 36, 170 },
    { 763, 106, 36, 170 },
    { 888, 106, 36, 170 },
};

constexpr Slot octaveSlot { 495, 744, 36, 104 };
const juce::Rectangle<int> ledLeft { 182, 692, 34, 34 };
const juce::Rectangle<int> ledRight { 812, 692, 34, 34 };
const juce::Rectangle<int> switchLeft { 150, 786, 78, 78 };
const juce::Rectangle<int> switchRight { 791, 786, 78, 78 };

class BarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const bool selected = button.getToggleState();
        const bool isNew = button.getComponentID() == "new";

        juce::Colour fill = juce::Colour (0xff1a1e24);
        if (isNew)
            fill = juce::Colour (0xff1c6eae);
        if (selected)
            fill = juce::Colour (0xff2a84c4);
        if (highlighted)
            fill = fill.brighter (0.12f);
        if (down)
            fill = fill.darker (0.15f);
        if (! button.isEnabled())
            fill = fill.withMultipliedAlpha (0.45f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (juce::Colour (0xffe6d7b8).withAlpha (selected || isNew ? 0.85f : 0.35f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return juce::Font (juce::FontOptions (14.0f));
    }
};
}

struct PluginEditor::Content : public juce::Component,
                                private juce::AudioProcessorValueTreeState::Listener,
                                private juce::Timer
{
    explicit Content (PluginProcessor& proc)
        : processor (proc)
    {
        faceplate = juce::ImageFileFormat::loadFrom (BinaryData::faceplate_png, BinaryData::faceplate_pngSize);

        setLookAndFeel (&barLook);
        setOpaque (true);

        const char* suffixes[] = { " ms", " %", " dB", " dB", " dB", " dB", " s" };
        const int decimals[] = { 0, 0, 1, 1, 1, 1, 2 };

        for (int i = 0; i < 7; ++i)
        {
            auto& slider = fader[i];
            addAndMakeVisible (slider);
            slider.setBounds (faders[i].x, faders[i].y, faders[i].w, faders[i].h);
            slider.setPopupDisplayEnabled (true, false, this);
            const auto suffix = juce::String (suffixes[i]);
            const int places = decimals[i];
            slider.textFromValueFunction = [suffix, places] (double value)
            {
                return juce::String (value, places) + suffix;
            };
            wireSlider (slider);
        }

        addAndMakeVisible (octave);
        octave.setBounds (octaveSlot.x, octaveSlot.y, octaveSlot.w, octaveSlot.h);
        octave.setPopupDisplayEnabled (true, false, this);
        octave.textFromValueFunction = [] (double value)
        {
            const char* labels[] = { "-2", "-1", "Off", "+1", "+2" };
            return juce::String (labels[juce::jlimit (0, 4, (int) std::round (value))]);
        };
        wireSlider (octave);

        addAndMakeVisible (delaySwitch);
        addAndMakeVisible (reverbSwitch);
        delaySwitch.setBounds (switchLeft);
        reverbSwitch.setBounds (switchRight);
        wireSwitch (delaySwitch);
        wireSwitch (reverbSwitch);

        addAndMakeVisible (delayLed);
        addAndMakeVisible (reverbLed);
        delayLed.setBounds (ledLeft);
        reverbLed.setBounds (ledRight);

        addAndMakeVisible (status);
        status.setJustificationType (juce::Justification::centredLeft);
        status.setColour (juce::Label::textColourId, juce::Colour (0xffe6d7b8));
        status.setFont (juce::Font (juce::FontOptions (13.0f)));
        status.setInterceptsMouseClicks (false, false);

        addAndMakeVisible (levelLabel);
        levelLabel.setText ("LEVEL", juce::dontSendNotification);
        levelLabel.setJustificationType (juce::Justification::centredRight);
        levelLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe6d7b8));
        levelLabel.setFont (juce::Font (juce::FontOptions (13.0f)));

        addAndMakeVisible (level);
        level.setSliderStyle (juce::Slider::LinearHorizontal);
        level.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 20);
        level.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe6d7b8));
        level.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        level.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff12161b));
        level.setPopupDisplayEnabled (true, false, this);
        level.textFromValueFunction = [] (double value) { return juce::String (value, 1) + " dB"; };
        wireSlider (level);

        addAndMakeVisible (newLayerButton);
        newLayerButton.setButtonText ("+ New Layer");
        newLayerButton.setComponentID ("new");
        newLayerButton.onClick = [this] { addLayer(); };

        addAndMakeVisible (removeButton);
        removeButton.setButtonText ("Remove");
        removeButton.onClick = [this] { removeSelectedLayer(); };

        addAndMakeVisible (midiButton);
        midiButton.setButtonText ("MIDI Learn");
        midiButton.onClick = [this]
        {
            if (clickArm || processor.getArmedParameterIndex() >= 0)
            {
                clickArm = false;
                processor.cancelMidiLearn();
            }
            else
            {
                clickArm = true;
            }
            refreshStatus();
        };

        selected = juce::jlimit (0, processor.getNumLayers() - 1, processor.getSelectedLayer());
        bindControls();
        rebuildLayerButtons();

        processor.apvts.addParameterListener ("numLayers", this);
        stateGeneration = processor.getStateGeneration();
        startTimerHz (30);
    }

    ~Content() override
    {
        stopTimer();
        processor.apvts.removeParameterListener ("numLayers", this);
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0c0e11));

        if (faceplate.isValid())
            g.drawImage (faceplate, juce::Rectangle<float> (0.0f, 0.0f, 1024.0f, (float) faceHeight));
    }

    void resized() override
    {
        layoutBar();
    }

private:
    void wireSlider (LearnSlider& slider)
    {
        slider.onLearn = [this] (LearnSlider& source) { processor.armMidiLearn (source.paramId); clickArm = false; };
        slider.onClear = [this] (LearnSlider& source) { processor.clearMidiMapping (source.paramId); };
        slider.consumeClick = [this, &slider]
        {
            if (! clickArm)
                return false;
            clickArm = false;
            processor.armMidiLearn (slider.paramId);
            return true;
        };
    }

    void wireSwitch (MetalSwitch& button)
    {
        button.onLearn = [this] (MetalSwitch& source) { processor.armMidiLearn (source.paramId); clickArm = false; };
        button.onClear = [this] (MetalSwitch& source) { processor.clearMidiMapping (source.paramId); };
        button.consumeClick = [this, &button]
        {
            if (! clickArm)
                return false;
            clickArm = false;
            processor.armMidiLearn (button.paramId);
            return true;
        };
    }

    void bindControls()
    {
        sliderAttachments.clear();
        buttonAttachments.clear();

        auto attachSlider = [this] (LearnSlider& slider, const char* name)
        {
            const auto id = layerParam (name, selected);
            slider.paramId = id;
            sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processor.apvts, id, slider));
        };

        attachSlider (fader[0], "delayTime");
        attachSlider (fader[1], "delayFeedback");
        attachSlider (fader[2], "delayDry");
        attachSlider (fader[3], "delayWet");
        attachSlider (fader[4], "reverbDry");
        attachSlider (fader[5], "reverbWet");
        attachSlider (fader[6], "reverbDecay");
        attachSlider (octave, "octave");
        attachSlider (level, "level");

        auto attachButton = [this] (MetalSwitch& button, const char* name)
        {
            const auto id = layerParam (name, selected);
            button.paramId = id;
            buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                processor.apvts, id, button));
        };

        attachButton (delaySwitch, "delayOn");
        attachButton (reverbSwitch, "reverbOn");
        updateAssignmentMarks();
    }

    void selectLayer (int index)
    {
        selected = juce::jlimit (0, processor.getNumLayers() - 1, index);
        processor.setSelectedLayer (selected);
        bindControls();

        for (int i = 0; i < layerButtons.size(); ++i)
            layerButtons[i]->setToggleState (i == selected, juce::dontSendNotification);
    }

    void addLayer()
    {
        const int count = processor.getNumLayers();
        if (count >= kMaxLayers)
            return;

        processor.setNumLayers (count + 1);
        selectLayer (count);
        rebuildLayerButtons();
    }

    void removeSelectedLayer()
    {
        const int index = selected;
        processor.removeLayer (index);
        selected = juce::jlimit (0, processor.getNumLayers() - 1, index);
        processor.setSelectedLayer (selected);
        bindControls();
        rebuildLayerButtons();
    }

    void rebuildLayerButtons()
    {
        layerButtons.clear();
        const int count = processor.getNumLayers();

        for (int i = 0; i < count; ++i)
        {
            auto* button = layerButtons.add (new juce::TextButton (juce::String (i + 1)));
            addAndMakeVisible (button);
            button->setClickingTogglesState (false);
            button->setToggleState (i == selected, juce::dontSendNotification);
            button->onClick = [this, i] { selectLayer (i); };
            button->setTooltip ("Show layer " + juce::String (i + 1));
        }

        newLayerButton.setEnabled (count < kMaxLayers);
        removeButton.setEnabled (count > 1);
        layoutBar();
    }

    void layoutBar()
    {
        auto bar = juce::Rectangle<int> (16, faceHeight + 8, 992, 72);
        status.setBounds (bar.removeFromTop (18));

        auto row = bar.withTrimmedTop (6);
        int x = row.getX();
        const int y = row.getY();

        for (auto* button : layerButtons)
        {
            button->setBounds (x, y, 40, 36);
            x += 46;
        }

        newLayerButton.setBounds (x + 6, y, 132, 36);
        removeButton.setBounds (x + 146, y, 88, 36);
        midiButton.setBounds (x + 242, y, 110, 36);

        levelLabel.setBounds (760, y, 52, 36);
        level.setBounds (812, y + 4, 196, 28);
    }

    void refreshStatus()
    {
        if (processor.getArmedParameterIndex() >= 0)
        {
            status.setText ("Move a MIDI control to assign " + processor.getArmedParameterName()
                                + ".  Click MIDI Learn to cancel.",
                            juce::dontSendNotification);
            midiButton.setToggleState (true, juce::dontSendNotification);
            return;
        }

        if (clickArm)
        {
            status.setText ("Click a control, then move a MIDI CC.  Click MIDI Learn to cancel.",
                            juce::dontSendNotification);
            midiButton.setToggleState (true, juce::dontSendNotification);
            return;
        }

        midiButton.setToggleState (false, juce::dontSendNotification);
        status.setText ("Layer " + juce::String (selected + 1)
                            + " of " + juce::String (processor.getNumLayers())
                            + "    Right-click any control to MIDI-assign it.",
                        juce::dontSendNotification);
    }

    void updateAssignmentMarks()
    {
        const int armed = processor.getArmedParameterIndex();

        auto markSlider = [this, armed] (LearnSlider& slider)
        {
            const int cc = processor.getMappedController (slider.paramId);
            slider.mapped = cc >= 0;
            slider.learning = armed >= 0 && processor.apvts.getParameter (slider.paramId) != nullptr
                              && processor.apvts.getParameter (slider.paramId)->getParameterIndex() == armed;
            slider.setTooltip (cc >= 0 ? "MIDI CC " + juce::String (cc) + "  —  right-click to change"
                                       : "Right-click to MIDI learn");
            slider.repaint();
        };

        for (auto& slider : fader)
            markSlider (slider);
        markSlider (octave);
        markSlider (level);

        auto markSwitch = [this, armed] (MetalSwitch& button)
        {
            const int cc = processor.getMappedController (button.paramId);
            button.mapped = cc >= 0;
            button.learning = armed >= 0 && processor.apvts.getParameter (button.paramId) != nullptr
                              && processor.apvts.getParameter (button.paramId)->getParameterIndex() == armed;
            button.setTooltip (cc >= 0 ? "MIDI CC " + juce::String (cc) + "  —  right-click to change"
                                       : "Right-click to MIDI learn");
            button.repaint();
        };

        markSwitch (delaySwitch);
        markSwitch (reverbSwitch);
    }

    void timerCallback() override
    {
        const int generation = processor.getStateGeneration();
        if (generation != stateGeneration)
        {
            stateGeneration = generation;
            selected = juce::jlimit (0, processor.getNumLayers() - 1, processor.getSelectedLayer());
            bindControls();
            rebuildLayerButtons();
        }

        delayLed.setOn (processor.apvts.getRawParameterValue (layerParam ("delayOn", selected))->load() >= 0.5f);
        reverbLed.setOn (processor.apvts.getRawParameterValue (layerParam ("reverbOn", selected))->load() >= 0.5f);
        updateAssignmentMarks();
        refreshStatus();
    }

    void parameterChanged (const juce::String&, float) override
    {
        juce::Component::SafePointer<Content> safe (this);
        juce::MessageManager::callAsync ([safe]
        {
            if (safe == nullptr)
                return;

            const int count = safe->processor.getNumLayers();
            if (safe->selected >= count)
            {
                safe->selected = count - 1;
                safe->processor.setSelectedLayer (safe->selected);
                safe->bindControls();
            }

            safe->rebuildLayerButtons();
        });
    }

    PluginProcessor& processor;
    BarLookAndFeel barLook;
    juce::Image faceplate;

    MetalSlider fader[7];
    MetalSlider octave;
    LearnSlider level;
    MetalSwitch delaySwitch, reverbSwitch;
    GlowLed delayLed, reverbLed;

    juce::Label status, levelLabel;
    juce::TextButton newLayerButton, removeButton, midiButton;
    juce::OwnedArray<juce::TextButton> layerButtons;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    int selected = 0;
    int stateGeneration = 0;
    bool clickArm = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Content)
};

PluginEditor::PluginEditor (PluginProcessor& proc)
    : AudioProcessorEditor (proc),
      content (std::make_unique<Content> (proc))
{
    addAndMakeVisible (*content);
    setResizable (true, true);
    setResizeLimits (760, 753, 1280, 1269);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);
    setSize (900, 892);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c0e11));
}

void PluginEditor::resized()
{
    content->setBounds (0, 0, designWidth, designHeight);
    content->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth));
}
