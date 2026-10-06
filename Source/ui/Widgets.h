#pragma once

// The custom controls: rainbow knobs, pill buttons, switches, the mode selector,
// the looper waveform, the Clock and the X/Y pad.

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

//==============================================================================
// A knob drawn as a dark disc inside a rotating rainbow ring, with a pointer.
class RainbowSlider : public juce::Slider
{
public:
    RainbowSlider (const std::vector<juce::Colour>& ringStops) : stops (ringStops)
    {
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity (220);
        setScrollWheelEnabled (true);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float size = juce::jmin (r.getWidth(), r.getHeight());
        const auto c = r.getCentre();
        const float radius = size * 0.5f;
        const float ringT = juce::jmax (3.0f, size * 0.055f);
        const float t = (float) valueToProportionOfLength (getValue());
        const float angle = juce::MathConstants<float>::pi * (-0.75f + 1.5f * t);   // 0 = straight up

        if (isMouseOverOrDragging())
        {
            g.setColour (stops.front().withAlpha (0.18f));
            g.fillEllipse (r.withSizeKeepingCentre (size, size));
        }

        theme::drawRainbowRing (g, c, radius - ringT * 0.5f, ringT, angle, stops);
        g.setColour (theme::bg);
        g.fillEllipse (juce::Rectangle<float> (size - ringT * 2.0f, size - ringT * 2.0f).withCentre (c));

        // Pointer
        const float len = size * (size > 70 ? 0.26f : 0.25f), w = juce::jmax (3.0f, size * 0.065f);
        juce::Path p;
        p.addRoundedRectangle (-w * 0.5f, -(radius - ringT - size * 0.05f), w, len, w * 0.5f);
        g.setColour (theme::text);
        g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
    }

    void mouseEnter (const juce::MouseEvent& e) override { juce::Slider::mouseEnter (e); if (onHover) onHover(); }
    void mouseExit  (const juce::MouseEvent& e) override { juce::Slider::mouseExit (e);  if (onHover) onHover(); }

    std::function<void()> onHover;

private:
    std::vector<juce::Colour> stops;
};

//==============================================================================
// Knob + caption. The caption shows the live value while you hover or drag.
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const char* paramID, juce::String caption,
          const std::vector<juce::Colour>& stops, bool small)
        : slider (stops), name (std::move (caption)), isSmall (small),
          attachment (state, paramID, slider)
    {
        addAndMakeVisible (slider);
        if (auto* p = state.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.onValueChange = [this] { repaint(); };
        slider.onDragStart = [this] { repaint(); };
        slider.onDragEnd = [this] { repaint(); };
        slider.onHover = [this] { repaint(); };
        slider.setTitle (name);
    }

    std::function<juce::String (double)> valueText;   // optional pretty readout

    void resized() override
    {
        const int s = isSmall ? 48 : 60;
        slider.setBounds ((getWidth() - s) / 2, 0, s, s);
    }

    void paint (juce::Graphics& g) override
    {
        const int s = isSmall ? 48 : 60;
        auto area = getLocalBounds().withTrimmedTop (s + (isSmall ? 6 : 8));
        const bool live = slider.isMouseOverOrDragging();
        juce::String label = name;
        if (live)
            label = valueText ? valueText (slider.getValue()) : juce::String (juce::roundToInt (slider.getValue() * 100.0)) + "%";

        g.setFont (live ? theme::font (theme::FontKind::mono, isSmall ? 10.0f : 11.0f)
                        : theme::font (theme::FontKind::bold, isSmall ? 10.0f : 11.0f, 0.12f));
        g.setColour (live ? theme::text : theme::muted);
        g.drawText (label.toUpperCase(), area.removeFromTop (16), juce::Justification::centredTop, false);
    }

    RainbowSlider slider;

private:
    juce::String name;
    bool isSmall;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==============================================================================
// Rounded pill button with separate on/off looks.
class PillButton : public juce::Button
{
public:
    struct Style
    {
        juce::Colour onA, onB, onText;            // on: gradient fill A->B
        juce::Colour offFill, offBorder, offText; // off: flat fill + border
        float fontSize = 13.0f;
        bool boldWhenOn = true, boldAlways = false, outlineOn = false;
    };

    PillButton (juce::String label, Style s) : juce::Button (label), style (s)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const float rad = r.getHeight() * 0.5f;
        const bool on = getToggleState();

        if (on && ! style.outlineOn)
        {
            g.setGradientFill (theme::horizontal (style.onA, style.onB, r));
            g.fillRoundedRectangle (r, rad);
        }
        else
        {
            g.setColour (style.offFill);
            g.fillRoundedRectangle (r, rad);
            g.setColour (on ? style.onA : style.offBorder);
            g.drawRoundedRectangle (r, rad, on ? 1.5f : 1.0f);
        }

        if (over || down)
        {
            g.setColour (juce::Colours::white.withAlpha (down ? 0.14f : 0.07f));
            g.fillRoundedRectangle (r, rad);
        }

        const bool bold = style.boldAlways || (on && style.boldWhenOn);
        g.setFont (theme::font (bold ? theme::FontKind::bold : theme::FontKind::medium, style.fontSize));
        g.setColour (on && ! style.outlineOn ? style.onText : (on ? theme::text : style.offText));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, false);
    }

    Style style;
};

//==============================================================================
// Small on/off switch (channel power).
class Switch : public juce::Button
{
public:
    Switch (juce::Colour a, juce::Colour b) : juce::Button ("switch"), onA (a), onB (b)
    {
        setClickingTogglesState (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const float rad = r.getHeight() * 0.5f;
        if (getToggleState())
        {
            g.setGradientFill (theme::horizontal (onA, onB, r));
            g.fillRoundedRectangle (r, rad);
            g.setColour (theme::bg);
            g.fillEllipse (juce::Rectangle<float> (18, 18).withCentre ({ r.getRight() - 12.0f, r.getCentreY() }));
        }
        else
        {
            g.setColour (theme::inset);
            g.fillRoundedRectangle (r, rad);
            g.setColour (theme::border);
            g.drawRoundedRectangle (r, rad, 1.0f);
            g.setColour (theme::muted);
            g.fillEllipse (juce::Rectangle<float> (18, 18).withCentre ({ r.getX() + 12.0f, r.getCentreY() }));
        }
        if (over) { g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (r, rad); }
    }

private:
    juce::Colour onA, onB;
};

//==============================================================================
// Three-way mode selector bound to a choice parameter.
class Segmented : public juce::Component
{
public:
    Segmented (juce::AudioProcessorValueTreeState& state, const char* paramID, const juce::StringArray& names,
               juce::Colour a, juce::Colour b)
    {
        PillButton::Style st { a, b, theme::bg, juce::Colours::transparentBlack, juce::Colours::transparentBlack, theme::muted, 13.0f };
        for (int i = 0; i < names.size(); ++i)
        {
            auto* btn = buttons.add (new PillButton (names[i], st));
            btn->setRadioGroupId (1);
            btn->onClick = [this, i] { if (attachment) attachment->setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (btn);
        }
        auto* param = state.getParameter (paramID);
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { select ((int) v); });
        attachment->sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::inset);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), (float) getHeight() * 0.5f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (4);
        const int w = (r.getWidth() - 4 * (buttons.size() - 1)) / buttons.size();
        for (auto* b : buttons) { b->setBounds (r.removeFromLeft (w)); r.removeFromLeft (4); }
    }

private:
    void select (int index)
    {
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == index, juce::dontSendNotification);
    }

    juce::OwnedArray<PillButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

//==============================================================================
// Looper buffer: rolling peaks while listening, the captured loop + playhead while playing.
class Waveform : public juce::Component
{
public:
    std::function<void (float* bars, float& playhead)> source;

    void update() { repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::deep);
        g.fillRoundedRectangle (r, 12.0f);

        float bars[48] {}; float playhead = -1.0f;
        if (source) source (bars, playhead);

        float peak = 0.05f;
        for (float b : bars) peak = juce::jmax (peak, b);

        auto inner = r.reduced (10.0f, 8.0f);
        const float slot = inner.getWidth() / 48.0f;
        for (int i = 0; i < 48; ++i)
        {
            const float h = juce::jmax (2.0f, juce::jmin (1.0f, bars[i] / peak) * (inner.getHeight() - 6.0f));
            const float hue = std::fmod ((300.0f + (float) i * 7.0f) / 360.0f, 1.0f);
            g.setColour (juce::Colour::fromHSL (hue, 0.9f, 0.64f, 1.0f));
            g.fillRoundedRectangle (inner.getX() + (float) i * slot + 1.0f, inner.getCentreY() - h * 0.5f, slot - 2.0f, h, 1.5f);
        }

        if (playhead >= 0.0f)
        {
            const float x = inner.getX() + playhead * inner.getWidth();
            g.setColour (theme::text);
            g.fillRect (x - 1.0f, r.getY() + 4.0f, 2.0f, r.getHeight() - 8.0f);
        }

        const juce::String tag = playhead >= 0.0f ? "looping" : "listening";
        g.setFont (theme::font (theme::FontKind::mono, 10.0f));
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), tag) + 10.0f;
        auto tagR = juce::Rectangle<float> (r.getRight() - tw - 6.0f, r.getY() + 4.0f, tw, 15.0f);
        g.setColour (theme::deep);
        g.fillRoundedRectangle (tagR, 6.0f);
        g.setColour (theme::muted);
        g.drawText (tag, tagR, juce::Justification::centred, false);
    }
};

//==============================================================================
// X/Y pad: X drives Wet Modify, Y drives Loop Modify. Drag, or use the arrow keys.
class XYPad : public juce::Component
{
public:
    XYPad (juce::RangedAudioParameter& xParam, juce::RangedAudioParameter& yParam) : px (xParam), py (yParam)
    {
        setWantsKeyboardFocus (true);
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        setTitle ("X/Y pad: X sets Wet Modify, Y sets Looper Modify");
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (theme::deep);
        g.fillRoundedRectangle (r, 14.0f);

        // grid
        g.setColour (theme::muted.withAlpha (0.10f));
        for (float x = r.getX() + 25.0f; x < r.getRight(); x += 25.0f) g.fillRect (x, r.getY(), 1.0f, r.getHeight());
        for (float y = r.getY() + 25.0f; y < r.getBottom(); y += 25.0f) g.fillRect (r.getX(), y, r.getWidth(), 1.0f);

        const float x = r.getX() + px.getValue() * r.getWidth();
        const float y = r.getY() + (1.0f - py.getValue()) * r.getHeight();

        g.saveState();
        juce::Path clip; clip.addRoundedRectangle (r, 14.0f);
        g.reduceClipRegion (clip);
        g.setColour (theme::cyan.withAlpha (0.5f));  g.fillRect (x - 0.5f, r.getY(), 1.0f, r.getHeight());
        g.setColour (theme::pink.withAlpha (0.5f));  g.fillRect (r.getX(), y - 0.5f, r.getWidth(), 1.0f);
        juce::ColourGradient glow (theme::lime.withAlpha (0.35f), x, y, theme::lime.withAlpha (0.0f), x + 32.0f, y, true);
        g.setGradientFill (glow);
        g.fillEllipse (x - 32.0f, y - 32.0f, 64.0f, 64.0f);
        g.restoreState();

        // puck
        juce::ColourGradient puck (theme::text, x, y, theme::violet, x + 10.0f, y, true);
        puck.addColour (0.35, theme::pink);
        puck.addColour (0.62, theme::pink);
        g.setGradientFill (puck);
        g.fillEllipse (x - 10.0f, y - 10.0f, 20.0f, 20.0f);

        g.setColour (hasKeyboardFocus (false) ? theme::cyan : theme::border);
        g.drawRoundedRectangle (r, 14.0f, 1.0f);

        g.setFont (theme::font (theme::FontKind::medium, 10.0f));
        g.setColour (theme::muted);
        g.drawText (juce::String::fromUTF8 ("Y \xc2\xb7 Loop Modify"), r.reduced (8.0f, 6.0f), juce::Justification::topLeft, false);
        g.drawText (juce::String::fromUTF8 ("X \xc2\xb7 Wet Modify"), r.reduced (8.0f, 6.0f), juce::Justification::bottomRight, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        px.beginChangeGesture(); py.beginChangeGesture();
        dragging = true;
        setFrom (e.position);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragging) setFrom (e.position); }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging) return;
        dragging = false;
        px.endChangeGesture(); py.endChangeGesture();
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        float dx = 0, dy = 0;
        if (k == juce::KeyPress::leftKey) dx = -0.05f; else if (k == juce::KeyPress::rightKey) dx = 0.05f;
        else if (k == juce::KeyPress::upKey) dy = 0.05f; else if (k == juce::KeyPress::downKey) dy = -0.05f;
        else return false;
        px.beginChangeGesture(); py.beginChangeGesture();
        px.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, px.getValue() + dx));
        py.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, py.getValue() + dy));
        px.endChangeGesture(); py.endChangeGesture();
        repaint();
        return true;
    }

    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override   { repaint(); }

private:
    void setFrom (juce::Point<float> p)
    {
        auto r = getLocalBounds().toFloat();
        px.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth()));
        py.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, 1.0f - (p.y - r.getY()) / r.getHeight()));
        repaint();
    }

    juce::RangedAudioParameter& px;
    juce::RangedAudioParameter& py;
    bool dragging = false;
};
