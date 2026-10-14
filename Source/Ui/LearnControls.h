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
        setNumDecimalPlacesToDisplay (2);
        setPopupDisplayEnabled (true, false, nullptr);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        suppressEntry = false;

        if (event.mods.isPopupMenu())
        {
            suppressEntry = true;
            showAssignMenu();
            return;
        }

        if (consumeClick && consumeClick())
        {
            suppressEntry = true;
            return;
        }

        valueBeforeClick = getValue();
        juce::Slider::mouseDown (event);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        const bool typedEntry = ! suppressEntry
                                && ! event.mouseWasDraggedSinceMouseDown()
                                && getTextBoxPosition() == juce::Slider::NoTextBox;

        if (typedEntry)
            setValue (valueBeforeClick, juce::sendNotificationSync);

        juce::Slider::mouseUp (event);

        if (typedEntry)
            beginEntry();
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

    void beginEntry()
    {
        if (entry != nullptr)
            return;

        auto* parent = getParentComponent();
        if (parent == nullptr)
            return;

        entry = std::make_unique<juce::TextEditor>();
        entry->setFont (juce::Font (juce::FontOptions (14.0f)));
        const auto shown = valueFromTextFunction != nullptr && textFromValueFunction != nullptr
                               ? textFromValueFunction (getValue())
                               : juce::String (getValue(), 2);
        entry->setInputRestrictions (16, entryCharacters);
        entry->setText (shown, false);
        entry->setSelectAllWhenFocused (true);
        entry->setJustification (juce::Justification::centred);
        entry->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff12161b));
        entry->setColour (juce::TextEditor::textColourId, juce::Colour (0xffe6d7b8));
        entry->setColour (juce::TextEditor::highlightColourId, juce::Colour (0xff1c6eae));
        entry->setColour (juce::TextEditor::outlineColourId, juce::Colour (0xffe6d7b8));
        entry->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xffe6d7b8));

        auto area = parent->getLocalArea (this, getLocalBounds()).withSizeKeepingCentre (92, 22);
        area.setX (juce::jlimit (4, juce::jmax (4, parent->getWidth() - area.getWidth() - 4), area.getX()));
        area.setY (juce::jlimit (4, juce::jmax (4, parent->getHeight() - area.getHeight() - 4), area.getY()));

        parent->addAndMakeVisible (*entry);
        entry->setBounds (area);
        entry->toFront (true);

        juce::Component::SafePointer<LearnSlider> safe (this);
        entry->onReturnKey = [safe] { if (safe != nullptr) safe->commitEntry (true); };
        entry->onEscapeKey = [safe] { if (safe != nullptr) safe->commitEntry (false); };
        entry->onFocusLost = [safe] { if (safe != nullptr) safe->commitEntry (true); };
    }

    void commitEntry (bool apply)
    {
        if (entry == nullptr || committing)
            return;

        committing = true;
        const auto text = entry->getText();

        if (auto* parent = entry->getParentComponent())
            parent->removeChildComponent (entry.get());

        entry.reset();

        if (apply)
        {
            const double next = valueFromTextFunction != nullptr
                                    ? valueFromTextFunction (text)
                                    : text.getDoubleValue();
            setValue (next, juce::sendNotificationSync);
        }

        committing = false;
    }

    std::unique_ptr<juce::TextEditor> entry;
    double valueBeforeClick = 0.0;
    bool suppressEntry = false;
    bool committing = false;
    juce::String entryCharacters { "0123456789.-" };
};

class LedReadout : public juce::Component
{
public:
    LedReadout() { setInterceptsMouseClicks (false, false); }

    void setText (const juce::String& next)
    {
        if (text == next)
            return;

        text = next;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff041208));
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (juce::Colour (0xff16361f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);

        const bool dim = text == "OFF";
        g.setColour (dim ? juce::Colour (0xff2d6a3e) : juce::Colour (0xff8dff9a));
        g.setFont (juce::Font (juce::FontOptions (11.0f).withName (juce::Font::getDefaultMonospacedFontName())));
        g.drawFittedText (text, getLocalBounds().reduced (2, 0), juce::Justification::centred, 1);
    }

private:
    juce::String text { "--" };
};

class MetalSlider : public LearnSlider
{
public:
    static constexpr int ledBand = 16;

    MetalSlider()
    {
        setSliderStyle (juce::Slider::LinearVertical);
        setOpaque (false);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        ledClick = event.y >= getHeight() - ledBand && ! event.mods.isPopupMenu();

        if (ledClick)
        {
            suppressEntry = true;
            beginEntry();
            return;
        }

        LearnSlider::mouseDown (event);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (ledClick)
        {
            ledClick = false;
            return;
        }

        LearnSlider::mouseUp (event);
    }

    void paint (juce::Graphics& g) override
    {
        const float proportion = (float) valueToProportionOfLength (getValue());
        const float width = (float) getWidth();
        const float trackH = (float) juce::jmax (8, getHeight() - ledBand - 2);
        const float thumbH = juce::jlimit (16.0f, 40.0f, trackH * 0.34f);
        const float thumbW = juce::jmax (14.0f, width - 8.0f);
        const float travel = juce::jmax (1.0f, trackH - thumbH);
        const float y = travel * (1.0f - proportion);
        const auto thumb = juce::Rectangle<float> ((width - thumbW) * 0.5f, y, thumbW, thumbH);

        auto groove = juce::Rectangle<float> (width * 0.5f - 3.0f, 2.0f, 6.0f, juce::jmax (4.0f, trackH - 4.0f));
        g.setColour (juce::Colour (0xff070908));
        g.fillRoundedRectangle (groove, 3.0f);
        g.setColour (juce::Colour (0xffe6d7b8).withAlpha (0.28f));
        g.drawRoundedRectangle (groove, 3.0f, 1.0f);

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

        auto led = juce::Rectangle<float> (1.0f, (float) getHeight() - (float) ledBand,
                                           width - 2.0f, (float) ledBand - 1.0f);
        g.setColour (juce::Colour (0xff041208));
        g.fillRoundedRectangle (led, 2.0f);
        g.setColour (juce::Colour (0xff16361f));
        g.drawRoundedRectangle (led.reduced (0.5f), 2.0f, 1.0f);
        g.setColour (juce::Colour (0xff8dff9a));
        g.setFont (juce::Font (juce::FontOptions (10.0f).withName (juce::Font::getDefaultMonospacedFontName())));
        g.drawFittedText (juce::String (getValue(), 2), led.toNearestInt(), juce::Justification::centred, 1);
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

private:
    bool ledClick = false;
};

class MetalKnob : public LearnSlider
{
public:
    MetalKnob()
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                             juce::MathConstants<float>::pi * 2.75f,
                             true);
        setOpaque (false);
        entryCharacters = "0123456789+-oOfF ";
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (3.0f);
        const auto centre = bounds.getCentre();
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float start = juce::MathConstants<float>::pi * 1.25f;
        const float end = juce::MathConstants<float>::pi * 2.75f;
        const float proportion = (float) valueToProportionOfLength (getValue());
        const float angle = start + (end - start) * proportion;

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius - 1.0f, radius - 1.0f, 0.0f, start, end, true);
        g.setColour (juce::Colour (0xff141816));
        g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (proportion > 0.001f)
        {
            juce::Path filled;
            filled.addCentredArc (centre.x, centre.y, radius - 1.0f, radius - 1.0f, 0.0f, start, angle, true);
            g.setColour (juce::Colour (0xff8dff9a));
            g.strokePath (filled, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float capR = radius * 0.62f;
        auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (cap.translated (0.0f, 1.5f));

        juce::ColourGradient metal (juce::Colour (0xfff4f1ea), cap.getX(), cap.getY(),
                                    juce::Colour (0xff6e6a64), cap.getX(), cap.getBottom(), false);
        metal.addColour (0.45, juce::Colour (0xffb7b1a6));
        metal.addColour (0.55, juce::Colour (0xff4e4b47));
        g.setGradientFill (metal);
        g.fillEllipse (cap);

        g.setColour (juce::Colour (0xff1a1816));
        juce::Path pointer;
        pointer.addRoundedRectangle (-1.6f, -capR * 0.92f, 3.2f, capR * 0.55f, 1.0f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
        g.fillPath (pointer);

        if (learning)
        {
            g.setColour (juce::Colour (0xff3dc0ff));
            g.drawEllipse (cap.expanded (3.0f), 2.0f);
        }
        else if (mapped)
        {
            g.setColour (juce::Colour (0xff3dc0ff));
            g.fillEllipse (centre.x - 3.0f, cap.getY() + 2.0f, 6.0f, 6.0f);
        }
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

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto disc = juce::Rectangle<float> (diameter, diameter).withCentre (bounds.getCentre());
        if (down)
            disc = disc.reduced (1.0f);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (disc.translated (0.0f, 2.0f));

        juce::ColourGradient metal (juce::Colour (0xfff7f4ee), disc.getX(), disc.getY(),
                                    juce::Colour (0xff5c5954), disc.getRight(), disc.getBottom(), false);
        metal.addColour (0.4, juce::Colour (0xffc9c3b8));
        metal.addColour (0.55, juce::Colour (0xff3a3835));
        g.setGradientFill (metal);
        g.fillEllipse (disc.reduced (1.0f));

        g.setColour (juce::Colour (0xfff3ecdf).withAlpha (highlighted ? 1.0f : 0.85f));
        g.drawEllipse (disc.reduced (1.2f), highlighted ? 2.2f : 1.6f);

        auto lamp = disc.reduced (diameter * 0.30f);
        if (getToggleState())
        {
            g.setColour (juce::Colour (0xff8dff9a).withAlpha (0.45f));
            g.fillEllipse (lamp.expanded (4.0f));
            g.setColour (juce::Colour (0xffc8ffd0));
            g.fillEllipse (lamp);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.fillEllipse (lamp.reduced (lamp.getWidth() * 0.34f));
        }
        else
        {
            g.setColour (juce::Colour (0xff2a1414));
            g.fillEllipse (lamp);
            g.setColour (juce::Colour (0xff8a4038));
            g.drawEllipse (lamp.reduced (0.5f), 1.6f);
            g.setColour (juce::Colour (0xff5a2824));
            g.fillEllipse (lamp.reduced (lamp.getWidth() * 0.28f));
        }

        if (learning)
        {
            g.setColour (juce::Colour (0xff3dc0ff));
            g.drawEllipse (disc.expanded (2.0f), 2.0f);
        }
        else if (mapped)
        {
            g.setColour (juce::Colour (0xff3dc0ff));
            g.fillEllipse (disc.getCentreX() - 3.0f, disc.getY() + 3.0f, 6.0f, 6.0f);
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

class LearnButton : public juce::TextButton
{
public:
    std::function<void (LearnButton&)> onLearn;
    std::function<void (LearnButton&)> onClear;
    std::function<bool()> consumeClick;

    juce::String paramId;
    int action = -1;
    bool mapped = false;
    bool learning = false;

    LearnButton() { setMouseClickGrabsKeyboardFocus (false); }

    void paint (juce::Graphics& g) override
    {
        juce::TextButton::paint (g);

        if (learning)
        {
            g.setColour (juce::Colour (0xff3dc0ff));
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 2.0f);
        }
        else if (mapped)
        {
            g.setColour (juce::Colour (0xff3dc0ff));
            g.fillEllipse ((float) getWidth() - 10.0f, 3.0f, 6.0f, 6.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem (1, learning ? "Waiting for MIDI..." : "MIDI Learn");
            menu.addItem (2, "Clear MIDI Assignment", mapped);

            juce::Component::SafePointer<LearnButton> safe (this);
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

        juce::TextButton::mouseDown (event);
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
