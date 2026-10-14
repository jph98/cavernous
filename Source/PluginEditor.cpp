#include "PluginEditor.h"
#include "Ui/LearnControls.h"

namespace
{
constexpr int imageBand = 108;
constexpr int stageTop = 116;
constexpr int stageH = 584;
constexpr int toneTop = 704;
constexpr int droneTop = 744;
constexpr int barTop = 788;
constexpr int margin = 16;
constexpr int colGap = 12;
constexpr int colW = 340;

constexpr int faderW = 58;
constexpr int delayFaderX = 30;
constexpr int delayFaderPitch = 74;
constexpr int delayFaderY = 170;
constexpr int delayFaderH = 92;

constexpr int reverbFaderX = 30;
constexpr int reverbFaderPitch = 74;
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

const char* toneKeyShortcut (int key)
{
    static constexpr const char* shortcuts[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "+"
    };
    return shortcuts[(size_t) juce::jlimit (0, kToneKeyCount - 1, key)];
}

juce::juce_wchar shortcutCharacter (const juce::KeyPress& key)
{
    const auto typed = key.getTextCharacter();
    return typed != 0 ? typed : (juce::juce_wchar) key.getKeyCode();
}

int toneKeyForShortcut (juce::juce_wchar character)
{
    static constexpr juce::juce_wchar keys[] = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '+'
    };

    for (int i = 0; i < kToneKeyCount; ++i)
        if (character == keys[i])
            return i;

    return -1;
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

        if (id == "step" || id == "tone")
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

        if (id == "fade")
        {
            auto fill = juce::Colour (0xff3d3420);
            if (highlighted)
                fill = fill.brighter (0.14f);
            if (down || selected)
                fill = fill.brighter (0.2f);

            g.setColour (fill);
            g.fillRoundedRectangle (bounds, 5.0f);
            g.setColour (juce::Colour (0xffe6d7b8));
            g.drawRoundedRectangle (bounds, 5.0f, 1.2f);
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
        const auto id = button.getComponentID();
        const float size = (id == "step" || id == "tone") ? 12.0f : 13.0f;
        return juce::Font (juce::FontOptions (size));
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool down) override
    {
        const auto shortcut = button.getProperties()["shortcut"].toString();

        if (shortcut.isEmpty())
        {
            juce::LookAndFeel_V4::drawButtonText (g, button, highlighted, down);
            return;
        }

        auto colour = button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                                  : juce::TextButton::textColourOffId);
        if (! button.isEnabled())
            colour = colour.withMultipliedAlpha (0.5f);

        g.setColour (colour);

        if (button.getWidth() < 48)
        {
            auto area = button.getLocalBounds().reduced (1, 1);
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawFittedText (button.getButtonText(), area.removeFromTop (15), juce::Justification::centred, 1);
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.setColour (juce::Colour (0xff8dff9a).withAlpha (button.getToggleState() ? 1.0f : 0.72f));
            g.drawFittedText (shortcut, area, juce::Justification::centred, 1);
            return;
        }

        auto area = button.getLocalBounds().reduced (4, 0);
        auto hint = area.removeFromRight (18);
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.drawFittedText (button.getButtonText(), area, juce::Justification::centred, 1);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.setColour (juce::Colour (0xff8dff9a).withAlpha (0.9f));
        g.drawFittedText (shortcut, hint, juce::Justification::centred, 1);
    }
};

void styleCaption (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredRight);
    label.setColour (juce::Label::textColourId, juce::Colour (0xffe6d7b8));
    label.setFont (juce::Font (juce::FontOptions (11.0f)));
    label.setInterceptsMouseClicks (false, false);
}

void styleValueSlider (LearnSlider& slider, const juce::String& name, const juce::String& tip)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 18);
    slider.setNumDecimalPlacesToDisplay (2);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff8dff9a));
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff16361f));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff041208));
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffe6d7b8));
    slider.setColour (juce::Slider::trackColourId, juce::Colour (0xff1d6a38));
    slider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff14181e));
    slider.setName (name);
    slider.setTooltip (tip);
}

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
        const char* reverbNames[] = { "reverbSize", "reverbDry", "reverbWet", "reverbDecay" };
        const char* reverbSuffixes[] = { " %", " dB", " dB", " s" };
        const char* reverbTips[] = { "Size", "Dry", "Wet", "Decay" };

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

            auto& flutter = flutterButton[delay];
            addAndMakeVisible (flutter);
            flutter.setBounds (x + 196, y + 76, 128, 24);
            flutter.setComponentID ("tone");
            flutter.setButtonText ("Flutter");
            flutter.setClickingTogglesState (false);
            styleChip (flutter);
            flutter.onClick = [this, delay]
            {
                toggleBool (layerParam (delayFieldName ("flutterOn", delay).toRawUTF8(), selected));
            };
            flutter.setTooltip ("Move this delay's time up and down by the interval");
            wireParamButton (flutter);

            auto& interval = flutterSlider[delay];
            addAndMakeVisible (interval);
            interval.setBounds (x + 196, y + 104, 128, 26);
            styleValueSlider (interval, "Delay " + juce::String (delay + 1) + " flutter interval",
                              "How far the delay time moves up and down");
            interval.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 16);
            wireSlider (interval);

            for (int control = 0; control < 4; ++control)
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
                tone.setVisible (false);
                toneLed[delay][control].setVisible (false);
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

        auto addToneChip = [this] (LearnButton& button, const juce::String& text)
        {
            addAndMakeVisible (button);
            button.setButtonText (text);
            button.setComponentID ("tone");
            button.setClickingTogglesState (false);
            styleChip (button);
            wireAction (button);
        };

        addAndMakeVisible (keyLabel);
        styleCaption (keyLabel, "KEY");
        for (int key = 0; key < kToneKeyCount; ++key)
        {
            addToneChip (keyButton[key], toneKeyName (key));
            keyButton[key].action = toneKeyAction (key);
            keyButton[key].getProperties().set ("shortcut", toneKeyShortcut (key));
            keyButton[key].onClick = [this, key] { setLayerChoice ("toneKey", key); };
            keyButton[key].setTooltip (juce::String (toneKeyName (key)) + "  (" + toneKeyShortcut (key) + ")");
        }

        addAndMakeVisible (octaveLabel);
        styleCaption (octaveLabel, "OCT");
        for (int octave = kToneOctaveMin; octave <= kToneOctaveMax; ++octave)
        {
            const int index = octave - kToneOctaveMin;
            addToneChip (octaveButton[index], juce::String (octave));
            octaveButton[index].action = toneOctaveAction (octave);
            octaveButton[index].onClick = [this, octave] { setLayerInt ("toneOctave", octave); };
            octaveButton[index].setTooltip ("Octave " + juce::String (octave) + " of the generated tone");
        }

        addAndMakeVisible (overtoneLabel);
        styleCaption (overtoneLabel, "OVERTONES");
        for (int count = 0; count <= kToneOvertoneMax; ++count)
        {
            addToneChip (overtoneButton[count], juce::String (count));
            overtoneButton[count].action = toneOvertonesAction (count);
            overtoneButton[count].onClick = [this, count] { setLayerInt ("toneOvertones", count); };
            overtoneButton[count].setTooltip (juce::String (count) + " harmonics above the fundamental");
        }

        addAndMakeVisible (noiseLabel);
        styleCaption (noiseLabel, "NOISE");
        for (int colour = 0; colour < kNoiseColourCount; ++colour)
        {
            addToneChip (noiseButton[colour], noiseColourName (colour));
            noiseButton[colour].action = toneNoiseAction (colour);
            noiseButton[colour].onClick = [this, colour] { setLayerChoice ("toneNoise", colour); };
            noiseButton[colour].setTooltip (juce::String (noiseColourName (colour)) + " noise with the generated tone");
        }

        addAndMakeVisible (testButton);
        testButton.setButtonText ("Generate Tone");
        styleChip (testButton);
        testButton.action = testToneAction;
        testButton.getProperties().set ("shortcut", "G");
        testButton.onClick = [this] { processor.triggerTestTone(); };
        testButton.setTooltip ("Play this layer's key for the note length (G)");
        wireAction (testButton);

        addAndMakeVisible (lengthLabel);
        styleCaption (lengthLabel, "LENGTH");
        addAndMakeVisible (lengthSlider);
        styleValueSlider (lengthSlider, "Note length", "Length of the generated tone");
        wireSlider (lengthSlider);

        addAndMakeVisible (pitchLabel);
        styleCaption (pitchLabel, "PITCH");
        addAndMakeVisible (pitchSlider);
        styleValueSlider (pitchSlider, "Pitch", "Shift this layer's tone and drone lower or higher");
        wireSlider (pitchSlider);

        addAndMakeVisible (releaseLabel);
        styleCaption (releaseLabel, "RELEASE");
        addAndMakeVisible (releaseSlider);
        styleValueSlider (releaseSlider, "Release", "How long this layer takes to fade out");
        wireSlider (releaseSlider);

        addAndMakeVisible (knockPitchLabel);
        styleCaption (knockPitchLabel, "KICK");
        addAndMakeVisible (knockPitchSlider);
        styleValueSlider (knockPitchSlider, "Kick pitch", "Pitch of the spring tank kick");
        wireSlider (knockPitchSlider);

        addAndMakeVisible (modeLabel);
        styleCaption (modeLabel, "MODE");
        for (int mode = 0; mode < kDroneModeCount; ++mode)
        {
            addToneChip (modeButton[mode], droneModeName (mode));
            modeButton[mode].action = droneModeAction (mode);
            modeButton[mode].onClick = [this, mode] { setLayerChoice ("droneMode", mode); };
            modeButton[mode].setTooltip (juce::String (droneModeName (mode)) + " scale for the drone");
        }

        addAndMakeVisible (degreeLabel);
        styleCaption (degreeLabel, "DEGREES");
        for (int degree = 0; degree < kDroneDegreeCount; ++degree)
        {
            addToneChip (degreeButton[degree], juce::String (degree + 1));
            degreeButton[degree].paramId = droneDegreeParam (degree);
            degreeButton[degree].onClick = [this, degree]
            {
                toggleBool (toneLayerParam (droneDegreeParam (degree).toRawUTF8(), selected));
            };
            degreeButton[degree].setTooltip ("Drone scale degree " + juce::String (degree + 1));
            wireParamButton (degreeButton[degree]);
        }

        addAndMakeVisible (droneButton);
        droneButton.setButtonText ("Drone");
        droneButton.setComponentID ("tone");
        droneButton.setClickingTogglesState (false);
        styleChip (droneButton);
        droneButton.paramId = "droneOn";
        droneButton.getProperties().set ("shortcut", "D");
        droneButton.onClick = [this] { toggleBool (droneButton.paramId); };
        droneButton.setTooltip ("Hold this layer's scale degrees (D)");
        wireParamButton (droneButton);

        addAndMakeVisible (knockButton);
        knockButton.setButtonText ("Kick Tank");
        styleChip (knockButton);
        knockButton.action = knockAction;
        knockButton.getProperties().set ("shortcut", "K");
        knockButton.onClick = [this] { processor.knockTanks(); };
        knockButton.setTooltip ("Kick this layer's reverb tanks (K)");
        wireAction (knockButton);

        addAndMakeVisible (killTrailsButton);
        killTrailsButton.setComponentID ("kill");
        killTrailsButton.setButtonText ("Kill Trails");
        styleChip (killTrailsButton);
        killTrailsButton.action = killTrailsAction;
        killTrailsButton.onClick = [this] { processor.killTrails(); };
        killTrailsButton.setTooltip ("Clear the tails and return feedback and decay to the middle");
        wireAction (killTrailsButton);

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

        addAndMakeVisible (killAllButton);
        killAllButton.setComponentID ("kill");
        killAllButton.setButtonText ("Kill Em All");
        styleChip (killAllButton);
        killAllButton.action = killAllAction;
        killAllButton.onClick = [this] { processor.killAll(); };
        killAllButton.setTooltip ("Stop every layer's drone, notes, and tails");
        wireAction (killAllButton);

        addAndMakeVisible (fadeButton);
        fadeButton.setComponentID ("fade");
        fadeButton.setButtonText ("Fade Out");
        styleChip (fadeButton);
        fadeButton.action = fadeAction;
        fadeButton.onClick = [this] { processor.fadeOut(); };
        fadeButton.setTooltip ("Fade this layer to silence over its release");
        wireAction (fadeButton);

        addAndMakeVisible (fadeAllButton);
        fadeAllButton.setComponentID ("fade");
        fadeAllButton.setButtonText ("Fade All");
        styleChip (fadeAllButton);
        fadeAllButton.action = fadeAllAction;
        fadeAllButton.onClick = [this] { processor.fadeAll(); };
        fadeAllButton.setTooltip ("Fade every layer to silence over its release");
        wireAction (fadeAllButton);

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

        addAndMakeVisible (aboutButton);
        aboutButton.setButtonText ("About");
        styleChip (aboutButton);
        aboutButton.onClick = [this]
        {
            if (onAbout)
                onAbout();
        };

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

        juce::ColourGradient veil (juce::Colours::transparentBlack, 0.0f, (float) imageBand - 20.0f,
                                   juce::Colour (0xff07080b).withAlpha (0.28f), 0.0f, (float) stageTop, false);
        g.setGradientFill (veil);
        g.fillRect (0, imageBand - 20, getWidth(), getHeight() - imageBand + 20);

        g.setColour (juce::Colour (0xff07080b).withAlpha (0.42f));
        g.fillRect (0, toneTop - 8, getWidth(), getHeight() - toneTop + 8);

        g.setColour (juce::Colour (0xffe6d7b8).withAlpha (0.28f));
        g.drawLine (16.0f, (float) toneTop - 2.0f, (float) getWidth() - 16.0f, (float) toneTop - 2.0f, 1.0f);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (18.0f, 28.0f, 230.0f, 44.0f, 6.0f);
        g.setColour (juce::Colour (0xfff3ecdf));
        g.setFont (juce::Font (juce::FontOptions (26.0f)));
        g.drawText ("CAVERNOUS", 30, 32, 206, 36, juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xffe6d7b8).withAlpha (0.45f));
        g.drawLine (16.0f, (float) imageBand, (float) getWidth() - 16.0f, (float) imageBand, 1.0f);

        const char* delayCaps[] = { "TIME", "FEED", "DRY", "WET" };
        const char* reverbCaps[] = { "SIZE", "DRY", "WET", "DECAY" };
        const char* toneCaps[] = { "SHIFT", "DIFF", "LOW", "HIGH" };

        for (int stage = 0; stage < kDelayCount; ++stage)
        {
            const int x = columnX (stage);
            auto card = juce::Rectangle<float> ((float) x, (float) stageTop, (float) colW, (float) stageH);
            g.setColour (juce::Colour (0xff07080b).withAlpha (0.46f));
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

            for (int control = 0; control < 4; ++control)
                g.drawText (reverbCaps[control],
                            x + reverbFaderX + control * reverbFaderPitch, stageTop + reverbFaderY - 16, faderW, 14,
                            juce::Justification::centred);

            const auto typeId = layerParam (delayFieldName ("reverbType", stage).toRawUTF8(), selected);
            const int typeValue = (int) std::round (processor.apvts.getRawParameterValue (typeId)->load());

            if (typeValue == kShimmerType)
            {
                for (int control = 0; control < 4; ++control)
                    g.drawText (toneCaps[control],
                                x + toneKnobX + control * toneKnobPitch, stageTop + toneKnobY - 14, toneKnobSize, 14,
                                juce::Justification::centred);
            }
        }
    }

    void resized() override
    {
        layoutTone();
        layoutDrone();
        layoutBar();
    }

    std::function<void()> onAbout;

    bool performShortcut (const juce::KeyPress& key)
    {
        const auto character = shortcutCharacter (key);
        const int toneKey = toneKeyForShortcut (character);

        if (toneKey >= 0)
        {
            setLayerChoice ("toneKey", toneKey);
            return true;
        }

        const auto letter = juce::CharacterFunctions::toLowerCase (character);

        if (letter == 'g')
        {
            processor.triggerTestTone();
            return true;
        }

        if (letter == 'd')
        {
            toggleBool (toneLayerParam ("droneOn", selected));
            return true;
        }

        if (letter == 'k')
        {
            processor.knockTanks();
            return true;
        }

        return false;
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

    void setLayerChoice (const char* name, int index)
    {
        if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (
                processor.apvts.getParameter (toneLayerParam (name, selected))))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
            parameter->endChangeGesture();
        }
    }

    void setLayerInt (const char* name, int value)
    {
        if (auto* parameter = dynamic_cast<juce::AudioParameterInt*> (
                processor.apvts.getParameter (toneLayerParam (name, selected))))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) value));
            parameter->endChangeGesture();
        }
    }

    void setGlobalChoice (const char* name, int index)
    {
        if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (name)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) index));
            parameter->endChangeGesture();
        }
    }

    void toggleBool (const juce::String& name)
    {
        if (auto* parameter = dynamic_cast<juce::AudioParameterBool*> (processor.apvts.getParameter (name)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->get() ? 0.0f : 1.0f);
            parameter->endChangeGesture();
        }
    }

    void setGlobalInt (const char* name, int value)
    {
        if (auto* parameter = dynamic_cast<juce::AudioParameterInt*> (processor.apvts.getParameter (name)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) value));
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

            for (int control = 0; control < 4; ++control)
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
            flutterButton[delay].paramId = layerParam (delayFieldName ("flutterOn", delay).toRawUTF8(), selected);
            attachSlider (flutterSlider[delay], layerParam (delayFieldName ("flutterMs", delay).toRawUTF8(), selected));

            for (int type = 0; type < kReverbTypeCount; ++type)
                reverbType[delay][type].action = reverbTypeAction (selected, delay, type);
        }

        attachSlider (level, layerParam ("level", selected));
        attachSlider (lengthSlider, toneLayerParam ("toneLength", selected));
        attachSlider (pitchSlider, toneLayerParam ("tonePitch", selected));
        attachSlider (releaseSlider, toneLayerParam ("fadeRelease", selected));
        attachSlider (knockPitchSlider, toneLayerParam ("kickPitch", selected));
        droneButton.paramId = toneLayerParam ("droneOn", selected);

        for (int degree = 0; degree < kDroneDegreeCount; ++degree)
            degreeButton[degree].paramId = toneLayerParam (droneDegreeParam (degree).toRawUTF8(), selected);

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
        layoutTone();
        layoutDrone();
        layoutBar();
    }

    void layoutTone()
    {
        const int y = toneTop + 6;
        const int h = 28;
        int x = 16;

        keyLabel.setBounds (x, y, 32, h);
        x += 34;

        for (int key = 0; key < kToneKeyCount; ++key)
        {
            keyButton[key].setBounds (x, y, 26, h);
            x += 28;
        }

        x += 6;
        octaveLabel.setBounds (x, y, 32, h);
        x += 34;

        for (int index = 0; index <= kToneOctaveMax - kToneOctaveMin; ++index)
        {
            octaveButton[index].setBounds (x, y, 22, h);
            x += 24;
        }

        x += 6;
        overtoneLabel.setBounds (x, y, 78, h);
        x += 80;

        for (int count = 0; count <= kToneOvertoneMax; ++count)
        {
            overtoneButton[count].setBounds (x, y, 22, h);
            x += 24;
        }

        x += 6;
        noiseLabel.setBounds (x, y, 44, h);
        x += 46;

        const int noiseWidth[] = { 34, 50, 42, 52, 40 };
        for (int colour = 0; colour < kNoiseColourCount; ++colour)
        {
            noiseButton[colour].setBounds (x, y, noiseWidth[colour], h);
            x += noiseWidth[colour] + 3;
        }

        testButton.setBounds (getWidth() - 164, y, 148, h);
    }

    void layoutDrone()
    {
        const int y = droneTop + 4;
        const int h = 28;
        int x = 16;

        lengthLabel.setBounds (x, y, 58, h);
        lengthSlider.setBounds (x + 60, y, 230, h);
        x = 308;

        modeLabel.setBounds (x, y, 46, h);
        x += 48;

        const int modeWidth[] = { 62, 58, 66 };
        for (int mode = 0; mode < kDroneModeCount; ++mode)
        {
            modeButton[mode].setBounds (x, y, modeWidth[mode], h);
            x += modeWidth[mode] + 4;
        }

        x += 10;
        degreeLabel.setBounds (x, y, 68, h);
        x += 70;

        for (int degree = 0; degree < kDroneDegreeCount; ++degree)
        {
            degreeButton[degree].setBounds (x, y, 28, h);
            x += 32;
        }

        x += 10;
        pitchLabel.setBounds (x, y, 44, h);
        pitchSlider.setBounds (x + 46, y, 112, h);
        x += 166;

        releaseLabel.setBounds (x, y, 64, h);
        releaseSlider.setBounds (x + 66, y, 112, h);

        knockPitchLabel.setBounds (getWidth() - 430, y, 42, h);
        knockPitchSlider.setBounds (getWidth() - 386, y, 128, h);
        knockButton.setBounds (getWidth() - 246, y, 118, h);
        droneButton.setBounds (getWidth() - 120, y, 104, h);
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
        killTrailsButton.setBounds (x + 294, y, 108, 32);
        killButton.setBounds (x + 410, y, 72, 32);
        killAllButton.setBounds (x + 490, y, 118, 32);
        fadeButton.setBounds (x + 618, y, 100, 32);
        fadeAllButton.setBounds (x + 726, y, 100, 32);
        aboutButton.setBounds (getWidth() - 108, 30, 80, 32);

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
        markSlider (lengthSlider);
        markSlider (pitchSlider);
        markSlider (releaseSlider);
        markSlider (knockPitchSlider);
        for (auto& slider : flutterSlider)
            markSlider (slider);

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
        {
            const int cc = processor.getMappedAction (testToneAction);
            const juce::String name ("Generate Tone (G)");
            testButton.setTooltip (cc >= 0 ? name + "  —  MIDI CC " + juce::String (cc)
                                           : name + "  —  right-click to MIDI learn");
        }
        markAction (knockButton);
        {
            const int cc = processor.getMappedAction (knockAction);
            const juce::String name ("Kick Tank (K)");
            knockButton.setTooltip (cc >= 0 ? name + "  —  MIDI CC " + juce::String (cc)
                                            : name + "  —  right-click to MIDI learn");
        }
        markAction (killAllButton);
        killAllButton.setTooltip ("Kill Em All  —  stop every layer's drone, notes, and tails");
        markAction (fadeButton);
        fadeButton.setTooltip ("Fade Out  —  fade this layer to silence over its release");
        markAction (fadeAllButton);
        fadeAllButton.setTooltip ("Fade All  —  fade every layer to silence over its release");
        markAction (killTrailsButton);
        killTrailsButton.setTooltip ("Kill Trails  —  clear the tails and centre feedback and decay");
        for (int key = 0; key < kToneKeyCount; ++key)
        {
            markAction (keyButton[key]);
            const int cc = processor.getMappedAction (keyButton[key].action);
            const auto name = juce::String (toneKeyName (key)) + "  (" + toneKeyShortcut (key) + ")";
            keyButton[key].setTooltip (cc >= 0 ? name + "  —  MIDI CC " + juce::String (cc)
                                               : name + "  —  right-click to MIDI learn");
        }
        for (auto& button : octaveButton)
            markAction (button);
        for (auto& button : overtoneButton)
            markAction (button);
        for (auto& button : noiseButton)
            markAction (button);
        for (auto& button : modeButton)
            markAction (button);

        auto markParam = [this, armed] (LearnButton& button, const juce::String& tip)
        {
            const int cc = processor.getMappedController (button.paramId);
            button.mapped = cc >= 0;
            auto* parameter = processor.apvts.getParameter (button.paramId);
            button.learning = armed >= 0 && parameter != nullptr
                              && parameter->getParameterIndex() == armed;
            button.setTooltip (cc >= 0 ? tip + "  —  MIDI CC " + juce::String (cc)
                                       : tip + "  —  right-click to MIDI learn");
            button.repaint();
        };

        for (int degree = 0; degree < kDroneDegreeCount; ++degree)
            markParam (degreeButton[degree], "Drone degree " + juce::String (degree + 1));
        markParam (droneButton, "Hold the selected degrees in the current key (D)");
        for (int delay = 0; delay < kDelayCount; ++delay)
            markParam (flutterButton[delay], "Flutter delay " + juce::String (delay + 1));

        const int killCc = processor.getMappedController (killButton.paramId);
        killButton.mapped = killCc >= 0;
        auto* killParameter = processor.apvts.getParameter (killButton.paramId);
        killButton.learning = armed >= 0 && killParameter != nullptr
                               && killParameter->getParameterIndex() == armed;
        killButton.setTooltip (killCc >= 0 ? "Kill  —  MIDI CC " + juce::String (killCc)
                                           : "Stop this layer's drone, notes, and tails");
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

            const bool shimmer = typeValue == kShimmerType;
            if (toneKnob[delay][0].isVisible() != shimmer)
            {
                for (int control = 0; control < 4; ++control)
                {
                    toneKnob[delay][control].setVisible (shimmer);
                    toneLed[delay][control].setVisible (shimmer);
                }

                repaint();
            }

            const double shiftValue = toneKnob[delay][0].getValue();
            auto shiftText = juce::String (shiftValue, 2);
            if (shiftValue > 0.005)
                shiftText = "+" + shiftText;
            toneLed[delay][0].setText (shiftText);
            toneLed[delay][1].setText (juce::String (toneKnob[delay][1].getValue(), 2));
            toneLed[delay][2].setText (juce::String (toneKnob[delay][2].getValue(), toneKnob[delay][2].getValue() >= 100.0 ? 0 : 2));
            toneLed[delay][3].setText (juce::String (toneKnob[delay][3].getValue(), toneKnob[delay][3].getValue() >= 100.0 ? 0 : 2));

            delayOnLed[delay].setText (delaySwitch[delay].getToggleState() ? "ON" : "OFF");
            const auto flutterId = layerParam (delayFieldName ("flutterOn", delay).toRawUTF8(), selected);
            flutterButton[delay].setToggleState (
                processor.apvts.getRawParameterValue (flutterId)->load() >= 0.5f,
                juce::dontSendNotification);
            reverbOnLed[delay].setText (reverbSwitch[delay].getToggleState() ? "ON" : "OFF");
        }

        const auto chosen = [this] (const char* name)
        {
            return (int) std::round (processor.apvts.getRawParameterValue (toneLayerParam (name, selected))->load());
        };

        const int key = chosen ("toneKey");
        for (int i = 0; i < kToneKeyCount; ++i)
            keyButton[i].setToggleState (i == key, juce::dontSendNotification);

        const int octave = chosen ("toneOctave");
        for (int i = 0; i <= kToneOctaveMax - kToneOctaveMin; ++i)
            octaveButton[i].setToggleState (i + kToneOctaveMin == octave, juce::dontSendNotification);

        const int overtones = chosen ("toneOvertones");
        for (int i = 0; i <= kToneOvertoneMax; ++i)
            overtoneButton[i].setToggleState (i == overtones, juce::dontSendNotification);

        const int noise = chosen ("toneNoise");
        for (int i = 0; i < kNoiseColourCount; ++i)
            noiseButton[i].setToggleState (i == noise, juce::dontSendNotification);

        const int mode = chosen ("droneMode");
        for (int i = 0; i < kDroneModeCount; ++i)
            modeButton[i].setToggleState (i == mode, juce::dontSendNotification);

        for (int degree = 0; degree < kDroneDegreeCount; ++degree)
        {
            const bool on = processor.apvts.getRawParameterValue (
                                toneLayerParam (droneDegreeParam (degree).toRawUTF8(), selected))->load() >= 0.5f;
            degreeButton[degree].setToggleState (on, juce::dontSendNotification);
        }

        droneButton.setToggleState (
            processor.apvts.getRawParameterValue (toneLayerParam ("droneOn", selected))->load() >= 0.5f,
            juce::dontSendNotification);
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
    MetalSlider reverbFader[kDelayCount][4];
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
    LearnButton killTrailsButton;
    LearnButton killButton;
    LearnButton killAllButton;
    LearnButton fadeButton;
    LearnButton fadeAllButton;
    LearnButton keyButton[kToneKeyCount];
    LearnButton octaveButton[kToneOctaveMax - kToneOctaveMin + 1];
    LearnButton overtoneButton[kToneOvertoneMax + 1];
    LearnButton noiseButton[kNoiseColourCount];
    LearnButton modeButton[kDroneModeCount];
    LearnButton degreeButton[kDroneDegreeCount];
    LearnButton droneButton;
    LearnButton knockButton;
    LearnSlider lengthSlider;
    LearnSlider pitchSlider;
    LearnSlider releaseSlider;
    LearnSlider knockPitchSlider;
    LearnSlider flutterSlider[kDelayCount];
    juce::Label keyLabel, octaveLabel, overtoneLabel, noiseLabel;
    juce::Label lengthLabel, modeLabel, degreeLabel, pitchLabel, releaseLabel, knockPitchLabel;
    LearnButton flutterButton[kDelayCount];
    juce::String delayParam[kDelayCount][4];
    juce::String reverbParam[kDelayCount][4];
    juce::String toneParam[kDelayCount][4];

    juce::Label status, levelLabel;
    LearnButton newLayerButton, removeButton;
    juce::TextButton midiButton;
    juce::TextButton aboutButton;
    juce::OwnedArray<LearnButton> layerButtons;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    int selected = 0;
    int stateGeneration = 0;
    bool clickArm = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Content)
};

struct PluginEditor::AboutPanel : public juce::Component
{
    AboutPanel()
    {
        faceplate = juce::ImageFileFormat::loadFrom (BinaryData::faceplate_png, BinaryData::faceplate_pngSize);
        setWantsKeyboardFocus (true);

        closeButton.setButtonText ("Close");
        closeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1a1e24));
        closeButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xfff3ecdf));
        closeButton.onClick = [this] { setVisible (false); };
        addAndMakeVisible (closeButton);

        link.setButtonText ("www.audiomonkey.co.uk");
        link.setURL (juce::URL ("https://www.audiomonkey.co.uk"));
        link.setFont (juce::Font (juce::FontOptions (16.0f)), false, juce::Justification::centredLeft);
        link.setColour (juce::HyperlinkButton::textColourId, juce::Colour (0xfff3ecdf));
        addAndMakeVisible (link);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff07080b));

        const auto image = fittedImage();
        if (faceplate.isValid())
            g.drawImage (faceplate, image);

        g.setColour (juce::Colour (0xffe6d7b8).withAlpha (0.55f));
        g.drawRect (image, 1.0f);

        auto plate = textBlock().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.62f));
        g.fillRoundedRectangle (plate, 8.0f);
        g.setColour (juce::Colour (0xffe6d7b8).withAlpha (0.4f));
        g.drawRoundedRectangle (plate, 8.0f, 1.0f);

        auto text = textBlock().reduced (16, 12);
        g.setColour (juce::Colour (0xfff3ecdf));
        g.setFont (juce::Font (juce::FontOptions (22.0f)));
        g.drawText ("CAVERNOUS", text.removeFromTop (30), juce::Justification::centredLeft);
        g.setFont (juce::Font (juce::FontOptions (15.0f)));
        g.setColour (juce::Colour (0xffe6d7b8));
        g.drawFittedText ("An audio plugin made by Jonathan Holloway.",
                          text.removeFromTop (40), juce::Justification::centredLeft, 2);
        g.drawText ("For support", text.removeFromTop (22), juce::Justification::centredLeft);
    }

    void resized() override
    {
        closeButton.setBounds (getWidth() - 96, 16, 76, 30);

        auto text = textBlock().reduced (16, 12);
        text.removeFromTop (30 + 40 + 22);
        link.setBounds (text.removeFromTop (24));
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            setVisible (false);
            return true;
        }

        return false;
    }

private:
    void splitFrame (juce::Rectangle<int>& imageArea, juce::Rectangle<int>& caption) const
    {
        auto frame = getLocalBounds().reduced (28, 24);
        frame.removeFromTop (28);

        if (frame.getWidth() > 860)
        {
            caption = frame.removeFromRight (340).reduced (16, 8).withHeight (168);
            imageArea = frame;
            return;
        }

        caption = frame.removeFromBottom (156).reduced (8, 8);
        imageArea = frame;
    }

    juce::Rectangle<float> fittedImage() const
    {
        if (! faceplate.isValid())
            return {};

        juce::Rectangle<int> imageArea, caption;
        splitFrame (imageArea, caption);
        auto area = imageArea.toFloat();
        const float scale = juce::jmin (area.getWidth() / (float) faceplate.getWidth(),
                                         area.getHeight() / (float) faceplate.getHeight());
        const float w = (float) faceplate.getWidth() * scale;
        const float h = (float) faceplate.getHeight() * scale;
        return { area.getX() + (area.getWidth() - w) * 0.5f,
                 area.getY() + (area.getHeight() - h) * 0.5f, w, h };
    }

    juce::Rectangle<int> textBlock() const
    {
        juce::Rectangle<int> imageArea, caption;
        splitFrame (imageArea, caption);
        return caption;
    }

    juce::Image faceplate;
    juce::TextButton closeButton;
    juce::HyperlinkButton link;
};

PluginEditor::PluginEditor (PluginProcessor& proc)
    : AudioProcessorEditor (proc),
      content (std::make_unique<Content> (proc)),
      aboutPanel (std::make_unique<AboutPanel>())
{
    content->onAbout = [this]
    {
        aboutPanel->setVisible (true);
        aboutPanel->toFront (true);
        aboutPanel->grabKeyboardFocus();
    };

    viewport.setViewedComponent (content.get(), false);
    viewport.setScrollBarsShown (true, true);
    viewport.setScrollBarThickness (12);
    styleScrollBar (viewport.getVerticalScrollBar());
    styleScrollBar (viewport.getHorizontalScrollBar());
    addAndMakeVisible (viewport);
    addChildComponent (*aboutPanel);

    setResizable (true, true);
    setResizeLimits (760, 380, designWidth, designHeight);
    setWantsKeyboardFocus (true);

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

PluginEditor::~PluginEditor()
{
    if (shortcutRoot != nullptr)
        shortcutRoot->removeKeyListener (this);
}

void PluginEditor::parentHierarchyChanged()
{
    attachShortcutListener();
}

void PluginEditor::attachShortcutListener()
{
    auto* top = getParentComponent() != nullptr || isOnDesktop() ? getTopLevelComponent() : nullptr;

    if (top == shortcutRoot.getComponent())
        return;

    if (shortcutRoot != nullptr)
        shortcutRoot->removeKeyListener (this);

    shortcutRoot = top;

    if (shortcutRoot != nullptr)
        shortcutRoot->addKeyListener (this);
}

bool PluginEditor::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (aboutPanel != nullptr && aboutPanel->isVisible())
        return false;

    if (dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) != nullptr)
        return false;

    const auto mods = key.getModifiers();
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown())
        return false;

    if (heldKeyCodes.contains (key.getKeyCode()))
        return true;

    if (content == nullptr || ! content->performShortcut (key))
        return false;

    heldKeyCodes.add (key.getKeyCode());
    return true;
}

bool PluginEditor::keyStateChanged (bool isKeyDown, juce::Component*)
{
    if (! isKeyDown)
        heldKeyCodes.clear();

    return false;
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff07080b));
}

void PluginEditor::resized()
{
    viewport.setBounds (getLocalBounds());

    if (aboutPanel != nullptr)
        aboutPanel->setBounds (getLocalBounds());
}
