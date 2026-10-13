#pragma once

#include <JuceHeader.h>
#include <functional>

class LearnSlider : public juce::Slider
{
public:
    std::function<void (LearnSlider&)> onLearn;
    std::function<void (LearnSlider&)> onClear;
    std::function<bool()> consumeClick;

    juce::String paramId;
    bool mapped = false;
    bool learning = false;

    LearnSlider()
    {
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setSliderSnapsToMousePosition (true);
        setPopupDisplayEnabled (true, false, nullptr);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
        {
            showAssignMenu();
            return;
        }

        if (consumeClick && consumeClick())
            return;

        juce::Slider::mouseDown (event);
    }

protected:
    void showAssignMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, learning ? "Waiting for MIDI..." : "MIDI Learn");
        menu.addItem (2, "Clear MIDI Assignment", mapped);

        juce::Component::SafePointer<LearnSlider> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safe] (int result)
                            {
                                if (safe == nullptr)
                                    return;
                                if (result == 1 && safe->onLearn)
                                    safe->onLearn (*safe);
                                if (result == 2 && safe->onClear)
                                    safe->onClear (*safe);
                            });
    }
};

class MetalSlider : public LearnSlider
{
public:
    MetalSlider()
    {
        setSliderStyle (juce::Slider::LinearVertical);
        setOpaque (false);
    }

    void paint (juce::Graphics& g) override
    {
        const float proportion = (float) valueToProportionOfLength (getValue());
        const float height = (float) getHeight();
        const float width = (float) getWidth();
        const float thumbH = juce::jlimit (36.0f, 76.0f, height * 0.42f);
        const float thumbW = juce::jmax (16.0f, width - 6.0f);
        const float travel = juce::jmax (1.0f, height - thumbH);
        const float y = travel * (1.0f - proportion);
        const auto thumb = juce::Rectangle<float> ((width - thumbW) * 0.5f, y, thumbW, thumbH);

        if (learning)
        {
            g.setColour (juce::Colour (0xff3dc0ff));
            g.drawRoundedRectangle (thumb.expanded (3.0f), 4.0f, 2.0f);
        }

        paintThumb (g, thumb);

        if (mapped)
        {
            g.setColour (juce::Colour (0xff3dc0ff));
            g.fillEllipse ((float) getWidth() * 0.5f - 3.0f, 1.0f, 6.0f, 6.0f);
        }
    }

    static void paintThumb (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour (juce::Colour (0xff141210));
        g.fillRoundedRectangle (bounds.expanded (1.0f), 3.5f);

        juce::ColourGradient metal (juce::Colour (0xfff7f4ee), bounds.getX(), bounds.getY(),
                                    juce::Colour (0xff7d7972), bounds.getX(), bounds.getBottom(), false);
        metal.addColour (0.18, juce::Colour (0xffd5d0c6));
        metal.addColour (0.48, juce::Colour (0xffb7b1a6));
        metal.addColour (0.52, juce::Colour (0xff5e5a55));
        metal.addColour (0.78, juce::Colour (0xffc9c3b8));
        g.setGradientFill (metal);
        g.fillRoundedRectangle (bounds, 3.0f);

        auto slot = bounds.withSizeKeepingCentre (bounds.getWidth() * 0.62f, juce::jmax (3.0f, bounds.getHeight() * 0.075f));
        g.setColour (juce::Colour (0xff2c2926));
        g.fillRoundedRectangle (slot, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.drawLine (bounds.getX() + 3.0f, bounds.getY() + 1.5f, bounds.getRight() - 3.0f, bounds.getY() + 1.5f, 1.0f);
    }
};

class MetalSwitch : public juce::Button
{
public:
    std::function<void (MetalSwitch&)> onLearn;
    std::function<void (MetalSwitch&)> onClear;
    std::function<bool()> consumeClick;

    juce::String paramId;
    bool mapped = false;
    bool learning = false;

    MetalSwitch() : juce::Button ("switch")
    {
        setOpaque (false);
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        if (learning)
        {
            g.setColour (juce::Colour (0xff3dc0ff));
            g.drawEllipse (getLocalBounds().toFloat().reduced (3.0f), 2.5f);
        }
        else if (mapped)
        {
            g.setColour (juce::Colour (0xff3dc0ff));
            g.fillEllipse ((float) getWidth() * 0.5f - 3.5f, 4.0f, 7.0f, 7.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem (1, learning ? "Waiting for MIDI..." : "MIDI Learn");
            menu.addItem (2, "Clear MIDI Assignment", mapped);

            juce::Component::SafePointer<MetalSwitch> safe (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                [safe] (int result)
                                {
                                    if (safe == nullptr)
                                        return;
                                    if (result == 1 && safe->onLearn)
                                        safe->onLearn (*safe);
                                    if (result == 2 && safe->onClear)
                                        safe->onClear (*safe);
                                });
            return;
        }

        if (consumeClick && consumeClick())
            return;

        juce::Button::mouseDown (event);
    }
};

class GlowLed : public juce::Component
{
public:
    GlowLed() { setInterceptsMouseClicks (false, false); }

    void setOn (bool shouldBeOn)
    {
        if (on != shouldBeOn)
        {
            on = shouldBeOn;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (4.0f);

        if (on)
        {
            g.setColour (juce::Colour (0xff8fd8ff).withAlpha (0.35f));
            g.fillEllipse (bounds.expanded (7.0f));

            juce::ColourGradient glow (juce::Colour (0xfff4fbff), bounds.getCentre(),
                                       juce::Colour (0xff1494dc), bounds.getBottomLeft(), true);
            g.setGradientFill (glow);
            g.fillEllipse (bounds);

            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.fillEllipse (bounds.reduced (bounds.getWidth() * 0.30f));
        }
        else
        {
            g.setColour (juce::Colour (0xff07080b));
            g.fillEllipse (bounds.expanded (2.0f));
            g.setColour (juce::Colour (0xff16303c));
            g.drawEllipse (bounds.reduced (1.0f), 2.0f);
        }
    }

private:
    bool on = true;
};
