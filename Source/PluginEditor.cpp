#include "PluginEditor.h"
#include "Ui/LearnControls.h"

namespace
{
constexpr int imageBand = 108;
constexpr int stageTop = 116;
constexpr int stageH = 584;
constexpr int barTop = 708;
constexpr int margin = 16;
constexpr int colGap = 12;
constexpr int colW = 340;

constexpr int faderW = 58;
constexpr int delayFaderX = 30;
constexpr int delayFaderPitch = 74;
constexpr int delayFaderY = 170;
constexpr int delayFaderH = 92;

constexpr int reverbFaderX = 55;
constexpr int reverbFaderPitch = 86;
constexpr int reverbFaderY = 486;
constexpr int reverbFaderH = 84;

constexpr int toneKnobX = 30;
constexpr int toneKnobPitch = 78;
constexpr int toneKnobY = 402;
constexpr int toneKnobSize = 46;
constexpr int toneLedY = 450;

int columnX (int index)
{
    return margin + index * (colW + colGap);
}

class BarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const auto id = button.getComponentID();
        const bool selected = button.getToggleState();

        if (id == "step")
        {
            auto fill = selected ? juce::Colour (0xff1d6a38) : juce::Colour (0xff171b16);
            if (highlighted)
                fill = fill.brighter (0.16f);
            if (down)
                fill = fill.darker (0.14f);

            g.setColour (fill);
            g.fillRoundedRectangle (bounds, 3.0f);
            g.setColour (juce::Colour (0xff8dff9a).withAlpha (selected ? 0.95f : 0.4f));
            g.drawRoundedRectangle (bounds, 3.0f, selected ? 1.6f : 1.0f);
            return;
        }

        if (id == "kill")
        {
            auto fill = juce::Colour (0xff6a1c1c);
            if (highlighted)
                fill = fill.brighter (0.12f);
            if (down || selected)
                fill = fill.brighter (0.22f);

            g.setColour (fill);
            g.fillRoundedRectangle (bounds, 5.0f);
            g.setColour (juce::Colour (0xffffb4a8));
            g.drawRoundedRectangle (bounds, 5.0f, 1.2f);
            return;
        }

        const bool isNew = id == "new";
        juce::Colour fill = isNew ? juce::Colour (0xff1c6eae) : juce::Colour (0xff1a1e24);
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

    juce::Font getTextButtonFont (juce::TextButton& button, int) override
    {
        const float size = button.getComponentID() == "step" ? 12.0f : 13.0f;
        return juce::Font (juce::FontOptions (size));
    }
};

void styleChip (juce::TextButton& button)
{
    button.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd7cbb6));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void styleScrollBar (juce::ScrollBar& bar)
{
    bar.setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xffcbbfa6));
    bar.setColour (juce::ScrollBar::backgroundColourId, juce::Colour (0xff121418));
}
}

struct PluginEditor::Content : public juce::Component,
                                private juce::AudioProcessorValueTreeState::Listener,
                                private juce::Timer
{
    explicit Content (PluginProcessor& proc)
        : processor (proc)
    {
        setSize (PluginEditor::designWidth, PluginEditor::designHeight);
        faceplate = juce::ImageFileFormat::loadFrom (BinaryData::faceplate_png, BinaryData::faceplate_pngSize);

        setLookAndFeel (&barLook);
        setOpaque (true);

        const char* delayNames[] = { "delayTime", "delayFeedback", "delayDry", "delayWet" };
        const char* delaySuffixes[] = { " ms", " %", " dB", " dB" };
        const char* delayTips[] = { "Time", "Feedback", "Dry", "Wet" };
        const char* reverbNames[] = { "reverbDry", "reverbWet", "reverbDecay" };
        const char* reverbSuffixes[] = { " dB", " dB", " s" };
        const char* reverbTips[] = { "Dry", "Wet", "Decay" };

        for (int delay = 0; delay < kDelayCount; ++delay)
        {
            const int x = columnX (delay);
            const int y = stageTop;

            for (int control = 0; control < 4; ++control)
            {
                auto& slider = delayFader[delay][control];
                addAndMakeVisible (slider);
                slider.setBounds (x + delayFaderX + control * delayFaderPitch, y + delayFaderY, faderW, delayFaderH);
                slider.setPopupDisplayEnabled (true, false, this);
                const auto suffix = juce::String (delaySuffixes[control]);
                slider.setNumDecimalPlacesToDisplay (2);
                slider.textFromValueFunction = [suffix] (double value)
                {
                    return juce::String (value, 2) + suffix;
                };
                slider.setName ("Delay " + juce::String (delay + 1) + " " + delayTips[control]);
                wireSlider (slider);
                delayParam[delay][control] = delayFieldName (delayNames[control], delay);
            }

            auto& knob = octaveKnob[delay];
            addAndMakeVisible (knob);
            knob.setBounds (x + 16, y + 76, 70, 70);
            knob.setPopupDisplayEnabled (true, false, this);
            knob.setName ("Delay " + juce::String (delay + 1) + " octave");
            wireSlider (knob);

            addAndMakeVisible (octaveLed[delay]);
            octaveLed[delay].setBounds (x + 98, y + 100, 86, 22);

            auto& button = delaySwitch[delay];
            addAndMakeVisible (button);
            button.setBounds (x + colW - 60, y + 10, 48, 48);
            button.setTooltip ("Delay " + juce::String (delay + 1) + " on/off");
            wireSwitch (button);

            addAndMakeVisible (delayOnLed[delay]);
            delayOnLed[delay].setBounds (x + colW - 124, y + 24, 56, 18);

            for (int control = 0; control < 3; ++control)
            {
                auto& slider = reverbFader[delay][control];
                addAndMakeVisible (slider);
                slider.setBounds (x + reverbFaderX + control * reverbFaderPitch, y + reverbFaderY, faderW, reverbFaderH);
                slider.setPopupDisplayEnabled (true, false, this);
                const auto suffix = juce::String (reverbSuffixes[control]);
                slider.setNumDecimalPlacesToDisplay (2);
                slider.textFromValueFunction = [suffix] (double value)
                {
                    return juce::String (value, 2) + suffix;
                };
                slider.setName ("Reverb " + juce::String (delay + 1) + " " + reverbTips[control]);
                wireSlider (slider);
                reverbParam[delay][control] = delayFieldName (reverbNames[control], delay);
            }

            const char* toneNames[] = { "reverbShift", "reverbDiffusion", "reverbLow", "reverbHigh" };
            const char* toneTips[] = { "Shift", "Diffusion", "Low cut", "High cut" };

            for (int control = 0; control < 4; ++control)
            {
                auto& tone = toneKnob[delay][control];
                addAndMakeVisible (tone);
                const int knobX = x + toneKnobX + control * toneKnobPitch;
                tone.setBounds (knobX, y + toneKnobY, toneKnobSize, toneKnobSize);
                tone.setPopupDisplayEnabled (true, false, this);
                tone.setName ("Reverb " + juce::String (delay + 1) + " " + toneTips[control]);
                wireSlider (tone);
                toneParam[delay][control] = delayFieldName (toneNames[control], delay);

                addAndMakeVisible (toneLed[delay][control]);
                toneLed[delay][control].setBounds (knobX + (toneKnobSize - 72) / 2, y + toneLedY, 72, 16);
            }

            for (int type = 0; type < kReverbTypeCount; ++type)
            {
                auto& typeButton = reverbType[delay][type];
                addAndMakeVisible (typeButton);
                const int col = type % 3;
                const int row = type / 3;
                typeButton.setBounds (x + 14 + col * 106, y + 336 + row * 26, 98, 22);
                typeButton.setComponentID ("step");
                typeButton.setButtonText (reverbTypeName (type));
                styleChip (typeButton);
                typeButton.setName ("Reverb " + juce::String (delay + 1) + " " + reverbTypeName (type));
                typeButton.onClick = [this, delay, type]
                {
                    setChoice (delayFieldName ("reverbType", delay).toRawUTF8(), type);
                };
                wireAction (typeButton);
            }

            addAndMakeVisible (typeLed[delay]);
            typeLed[delay].setBounds (x + 100, y + 294, 104, 20);

            auto& room = reverbSwitch[delay];
            addAndMakeVisible (room);
            room.setBounds (x + colW - 60, y + 280, 48, 48);
            room.setTooltip ("Reverb " + juce::String (delay + 1) + " on/off");
            wireSwitch (room);

            addAndMakeVisible (reverbOnLed[delay]);
            reverbOnLed[delay].setBounds (x + colW - 124, y + 294, 56, 18);
        }

        addAndMakeVisible (testButton);
        testButton.setButtonText ("Test");
        styleChip (testButton);
        testButton.action = testToneAction;
        testButton.onClick = [this] { processor.triggerTestTone(); };
        testButton.setTooltip ("Play a short tone through the pedal");
        wireAction (testButton);

        addAndMakeVisible (killButton);
        killButton.setComponentID ("kill");
        killButton.setButtonText ("Kill");
        styleChip (killButton);
        killButton.paramId = "kill";
        killButton.onClick = [this]
        {
            if (auto* parameter = processor.apvts.getParameter ("kill"))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (1.0f);
                parameter->endChangeGesture();
            }
        };
        wireParamButton (killButton);

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
        level.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 18);
        level.setNumDecimalPlacesToDisplay (2);
        level.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff8dff9a));
        level.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff16361f));
        level.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff041208));
        level.setColour (juce::Slider::thumbColourId, juce::Colour (0xffe6d7b8));
        level.setColour (juce::Slider::trackColourId, juce::Colour (0xff1d6a38));
        level.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff14181e));
        level.setPopupDisplayEnabled (true, false, this);
        level.textFromValueFunction = [] (double value) { return juce::String (value, 2) + " dB"; };
        wireSlider (level);

        addAndMakeVisible (newLayerButton);
        newLayerButton.setButtonText ("+ New Layer");
        newLayerButton.setComponentID ("new");
        styleChip (newLayerButton);
        newLayerButton.action = addLayerAction;
        newLayerButton.onClick = [this] { addLayer(); };
        wireAction (newLayerButton);

        addAndMakeVisible (removeButton);
        removeButton.setButtonText ("Remove");
        styleChip (removeButton);
        removeButton.action = removeLayerAction;
        removeButton.onClick = [this] { removeSelectedLayer(); };
        wireAction (removeButton);

        addAndMakeVisible (midiButton);
        midiButton.setButtonText ("MIDI Learn");
        styleChip (midiButton);
        midiButton.onClick = [this]
        {
            if (clickArm || processor.isMidiArmed())
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
        g.fillAll (juce::Colour (0xff07080b));

        if (faceplate.isValid())
        {
            const float scale = (float) getWidth() / (float) juce::jmax (1, faceplate.getWidth());
            const int srcVisible = juce::jlimit (1, faceplate.getHeight(),
                                                  (int) std::round ((float) getHeight() / scale));
            const int srcY = juce::jlimit (0, faceplate.getHeight() - srcVisible, 340);
            g.drawImage (faceplate,
                         0, 0, getWidth(), getHeight(),
                         0, srcY, faceplate.getWidth(), srcVisible);
        }

        juce::ColourGradient veil (juce::Colours::transparentBlack, 0.0f, (float) imageBand - 28.0f,
                                   juce::Colour (0xff07080b).withAlpha (0.84f), 0.0f, (float) stageTop, false);
        g.setGradientFill (veil);
        g.fillRect (0, imageBand - 28, getWidth(), getHeight() - imageBand + 28);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (18.0f, 28.0f, 230.0f, 44.0f, 6.0f);
        g.setColour (juce::Colour (0xfff3ecdf));
        g.setFont (juce::Font (juce::FontOptions (26.0f)));
        g.drawText ("CAVERNOUS", 30, 32, 206, 36, juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xffe6d7b8).withAlpha (0.45f));
        g.drawLine (16.0f, (float) imageBand, (float) getWidth() - 16.0f, (float) imageBand, 1.0f);

        const char* delayCaps[] = { "TIME", "FEED", "DRY", "WET" };
        const char* reverbCaps[] = { "DRY", "WET", "DECAY" };
        const char* toneCaps[] = { "SHIFT", "DIFF", "LOW", "HIGH" };

        for (int stage = 0; stage < kDelayCount; ++stage)
        {
            const int x = columnX (stage);
            auto card = juce::Rectangle<float> ((float) x, (float) stageTop, (float) colW, (float) stageH);
            g.setColour (juce::Colour (0xff07080b).withAlpha (0.78f));
            g.fillRoundedRectangle (card, 8.0f);
            g.setColour (juce::Colour (0xffe6d7b8).withAlpha (0.32f));
            g.drawRoundedRectangle (card.reduced (0.5f), 8.0f, 1.2f);

            g.setColour (juce::Colour (0xffe6d7b8).withAlpha (0.2f));
            g.drawLine ((float) x + 14.0f, (float) stageTop + 272.0f, (float) x + colW - 14.0f, (float) stageTop + 272.0f, 1.0f);

            g.setColour (juce::Colour (0xfff3ecdf));
            g.setFont (juce::Font (juce::FontOptions (15.0f)));
            g.drawText ("DELAY " + juce::String (stage + 1), x + 16, stageTop + 16, 150, 22, juce::Justification::centredLeft);
            g.drawText ("REVERB", x + 16, stageTop + 292, 78, 22, juce::Justification::centredLeft);

            g.setColour (juce::Colour (0xffe6d7b8).withAlpha (0.7f));
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText ("OCTAVE", x + 98, stageTop + 78, 86, 16, juce::Justification::centredLeft);

            for (int control = 0; control < 4; ++control)
                g.drawText (delayCaps[control],
                            x + delayFaderX + control * delayFaderPitch, stageTop + delayFaderY - 16, faderW, 14,
                            juce::Justification::centred);

            for (int control = 0; control < 3; ++control)
                g.drawText (reverbCaps[control],
                            x + reverbFaderX + control * reverbFaderPitch, stageTop + reverbFaderY - 16, faderW, 14,
                            juce::Justification::centred);

            for (int control = 0; control < 4; ++control)
                g.drawText (toneCaps[control],
                            x + toneKnobX + control * toneKnobPitch, stageTop + toneKnobY - 14, toneKnobSize, 14,
                            juce::Justification::centred);
        }
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

    void wireAction (LearnButton& button)
    {
        button.onLearn = [this] (LearnButton& source) { processor.armMidiAction (source.action); clickArm = false; };
        button.onClear = [this] (LearnButton& source) { processor.clearMidiAction (source.action); };
        button.consumeClick = [this, &button]
        {
            if (! clickArm)
                return false;
            clickArm = false;
            processor.armMidiAction (button.action);
            return true;
        };
    }

    void wireParamButton (LearnButton& button)
    {
        button.onLearn = [this] (LearnButton& source) { processor.armMidiLearn (source.paramId); clickArm = false; };
        button.onClear = [this] (LearnButton& source) { processor.clearMidiMapping (source.paramId); };
        button.consumeClick = [this, &button]
        {
            if (! clickArm)
                return false;
            clickArm = false;
            processor.armMidiLearn (button.paramId);
            return true;
        };
    }

    void setChoice (const char* name, int index)
    {
        if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (
                processor.apvts.getParameter (layerParam (name, selected))))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
            parameter->endChangeGesture();
        }
    }

    void bindControls()
    {
        sliderAttachments.clear();
        buttonAttachments.clear();

        auto attachSlider = [this] (LearnSlider& slider, const juce::String& id)
        {
            slider.paramId = id;
            sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processor.apvts, id, slider));
        };

        for (int delay = 0; delay < kDelayCount; ++delay)
        {
            for (int control = 0; control < 4; ++control)
                attachSlider (delayFader[delay][control], layerParam (delayParam[delay][control].toRawUTF8(), selected));

            attachSlider (octaveKnob[delay], layerParam (delayFieldName ("octave", delay).toRawUTF8(), selected));

            for (int control = 0; control < 3; ++control)
                attachSlider (reverbFader[delay][control], layerParam (reverbParam[delay][control].toRawUTF8(), selected));

            for (int control = 0; control < 4; ++control)
                attachSlider (toneKnob[delay][control], layerParam (toneParam[delay][control].toRawUTF8(), selected));

            auto attachButton = [this] (MetalSwitch& button, const char* name)
            {
                const auto id = layerParam (name, selected);
                button.paramId = id;
                buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    processor.apvts, id, button));
            };

            attachButton (delaySwitch[delay], delayFieldName ("delayOn", delay).toRawUTF8());
            attachButton (reverbSwitch[delay], delayFieldName ("reverbOn", delay).toRawUTF8());

            for (int type = 0; type < kReverbTypeCount; ++type)
                reverbType[delay][type].action = reverbTypeAction (selected, delay, type);
        }

        attachSlider (level, layerParam ("level", selected));
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
            auto* button = layerButtons.add (new LearnButton());
            addAndMakeVisible (button);
            button->setButtonText (juce::String (i + 1));
            styleChip (*button);
            button->setClickingTogglesState (false);
            button->setToggleState (i == selected, juce::dontSendNotification);
            button->action = selectLayerAction (i);
            button->onClick = [this, i] { selectLayer (i); };
            button->setTooltip ("Show layer " + juce::String (i + 1));
            wireAction (*button);
        }

        newLayerButton.setEnabled (count < kMaxLayers);
        removeButton.setEnabled (count > 1);
        layoutBar();
    }

    void layoutBar()
    {
        auto bar = juce::Rectangle<int> (16, barTop, getWidth() - 32, getHeight() - barTop - 8);
        status.setBounds (bar.removeFromTop (18));

        auto row = bar.withTrimmedTop (6);
        int x = row.getX();
        const int y = row.getY();

        for (auto* button : layerButtons)
        {
            button->setBounds (x, y, 32, 32);
            x += 36;
        }

        newLayerButton.setBounds (x + 8, y, 104, 32);
        removeButton.setBounds (x + 116, y, 70, 32);
        midiButton.setBounds (x + 190, y, 96, 32);
        testButton.setBounds (x + 294, y, 64, 32);
        killButton.setBounds (x + 366, y, 72, 32);

        levelLabel.setBounds (getWidth() - 268, y, 52, 32);
        level.setBounds (getWidth() - 212, y + 2, 188, 28);
    }

    void refreshStatus()
    {
        if (processor.isMidiArmed())
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
        const int armedAction = processor.getArmedAction();

        auto markSlider = [this, armed] (LearnSlider& slider)
        {
            const int cc = processor.getMappedController (slider.paramId);
            slider.mapped = cc >= 0;
            auto* parameter = processor.apvts.getParameter (slider.paramId);
            slider.learning = armed >= 0 && parameter != nullptr && parameter->getParameterIndex() == armed;
            const auto typing = "  Click to type a value.  Right-click to MIDI learn.";
            slider.setTooltip (cc >= 0 ? slider.getName() + "  —  MIDI CC " + juce::String (cc) + typing
                                       : slider.getName() + "  —" + typing);
            slider.repaint();
        };

        for (auto& column : delayFader)
            for (auto& slider : column)
                markSlider (slider);

        for (auto& knob : octaveKnob)
            markSlider (knob);

        for (auto& column : reverbFader)
            for (auto& slider : column)
                markSlider (slider);

        for (auto& column : toneKnob)
            for (auto& knob : column)
                markSlider (knob);

        markSlider (level);

        auto markSwitch = [this, armed] (MetalSwitch& button)
        {
            const int cc = processor.getMappedController (button.paramId);
            button.mapped = cc >= 0;
            auto* parameter = processor.apvts.getParameter (button.paramId);
            button.learning = armed >= 0 && parameter != nullptr && parameter->getParameterIndex() == armed;
            button.setTooltip (cc >= 0 ? "MIDI CC " + juce::String (cc) + "  —  right-click to change"
                                       : "Right-click to MIDI learn");
            button.repaint();
        };

        for (auto& button : delaySwitch)
            markSwitch (button);
        for (auto& button : reverbSwitch)
            markSwitch (button);

        auto markAction = [this, armedAction] (LearnButton& button)
        {
            const int cc = processor.getMappedAction (button.action);
            button.mapped = cc >= 0;
            button.learning = armedAction == button.action;
            const auto name = button.getButtonText().isNotEmpty() ? button.getButtonText() : button.getName();
            button.setTooltip (cc >= 0 ? name + "  —  MIDI CC " + juce::String (cc)
                                       : name + "  —  right-click to MIDI learn");
            button.repaint();
        };

        for (auto& row : reverbType)
            for (auto& button : row)
                markAction (button);
        for (auto* button : layerButtons)
            markAction (*button);
        markAction (newLayerButton);
        markAction (removeButton);
        markAction (testButton);

        const int killCc = processor.getMappedController (killButton.paramId);
        killButton.mapped = killCc >= 0;
        auto* killParameter = processor.apvts.getParameter (killButton.paramId);
        killButton.learning = armed >= 0 && killParameter != nullptr
                               && killParameter->getParameterIndex() == armed;
        killButton.setTooltip (killCc >= 0 ? "Kill  —  MIDI CC " + juce::String (killCc)
                                           : "Kill the delay and reverb tails");
        killButton.repaint();
    }

    void syncChoices()
    {
        for (int delay = 0; delay < kDelayCount; ++delay)
        {
            const auto octaveId = layerParam (delayFieldName ("octave", delay).toRawUTF8(), selected);
            const auto octaveValue = (int) std::round (processor.apvts.getRawParameterValue (octaveId)->load());
            octaveLed[delay].setText (octaveStepName (octaveValue));

            const auto typeId = layerParam (delayFieldName ("reverbType", delay).toRawUTF8(), selected);
            const auto typeValue = (int) std::round (processor.apvts.getRawParameterValue (typeId)->load());

            for (int type = 0; type < kReverbTypeCount; ++type)
                reverbType[delay][type].setToggleState (type == typeValue, juce::dontSendNotification);

            typeLed[delay].setText (juce::String (reverbTypeName (typeValue)).toUpperCase());

            const double shiftValue = toneKnob[delay][0].getValue();
            auto shiftText = juce::String (shiftValue, 2);
            if (shiftValue > 0.005)
                shiftText = "+" + shiftText;
            toneLed[delay][0].setText (shiftText);
            toneLed[delay][1].setText (juce::String (toneKnob[delay][1].getValue(), 2));
            toneLed[delay][2].setText (juce::String (toneKnob[delay][2].getValue(), toneKnob[delay][2].getValue() >= 100.0 ? 0 : 2));
            toneLed[delay][3].setText (juce::String (toneKnob[delay][3].getValue(), toneKnob[delay][3].getValue() >= 100.0 ? 0 : 2));

            delayOnLed[delay].setText (delaySwitch[delay].getToggleState() ? "ON" : "OFF");
            reverbOnLed[delay].setText (reverbSwitch[delay].getToggleState() ? "ON" : "OFF");
        }
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

        syncChoices();
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

    MetalSlider delayFader[kDelayCount][4];
    MetalKnob octaveKnob[kDelayCount];
    MetalSlider reverbFader[kDelayCount][3];
    MetalKnob toneKnob[kDelayCount][4];
    LedReadout toneLed[kDelayCount][4];
    LearnSlider level;
    MetalSwitch delaySwitch[kDelayCount];
    MetalSwitch reverbSwitch[kDelayCount];
    LearnButton reverbType[kDelayCount][kReverbTypeCount];
    LedReadout octaveLed[kDelayCount];
    LedReadout typeLed[kDelayCount];
    LedReadout delayOnLed[kDelayCount];
    LedReadout reverbOnLed[kDelayCount];
    LearnButton testButton;
    LearnButton killButton;
    juce::String delayParam[kDelayCount][4];
    juce::String reverbParam[kDelayCount][3];
    juce::String toneParam[kDelayCount][4];

    juce::Label status, levelLabel;
    LearnButton newLayerButton, removeButton;
    juce::TextButton midiButton;
    juce::OwnedArray<LearnButton> layerButtons;

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
    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, true);
    viewport.setScrollBarThickness (12);
    styleScrollBar (viewport.getVerticalScrollBar());
    styleScrollBar (viewport.getHorizontalScrollBar());
    addAndMakeVisible (viewport);

    setResizable (true, true);
    setResizeLimits (760, 380, designWidth, designHeight);

    int initialW = designWidth;
    int initialH = designHeight;

    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userBounds.toNearestInt();
        initialW = juce::jlimit (760, designWidth, area.getWidth() - 24);
        initialH = juce::jlimit (380, designHeight, area.getHeight() - 48);
    }

    setSize (initialW, initialH);
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff07080b));
}

void PluginEditor::resized()
{
    viewport.setBounds (getLocalBounds());
}
