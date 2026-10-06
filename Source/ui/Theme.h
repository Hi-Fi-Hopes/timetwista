#pragma once

// Colours, fonts and drawing helpers for the psychedelic "A2" look.

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace theme
{
    inline const juce::Colour bg      { 0xff1A0F2E };
    inline const juce::Colour panel   { 0xe024163F };
    inline const juce::Colour border  { 0xff4A3380 };
    inline const juce::Colour inset   { 0xff160B28 };
    inline const juce::Colour deep    { 0xff12091F };
    inline const juce::Colour text    { 0xffFFF4E6 };
    inline const juce::Colour muted   { 0xffC9B8E8 };
    inline const juce::Colour pink    { 0xffFF4FA3 };
    inline const juce::Colour orange  { 0xffFF9F1C };
    inline const juce::Colour lime    { 0xffC6F432 };
    inline const juce::Colour cyan    { 0xff3DDCFF };
    inline const juce::Colour violet  { 0xff9B6BFF };

    // Rainbow stops for knob rings; the looper side starts on lime.
    inline const std::vector<juce::Colour>& wetStops()  { static std::vector<juce::Colour> s { pink, orange, lime, cyan, violet, pink }; return s; }
    inline const std::vector<juce::Colour>& loopStops() { static std::vector<juce::Colour> s { lime, cyan, violet, pink, orange, lime }; return s; }

    inline juce::Colour sampleStops (const std::vector<juce::Colour>& stops, float t)
    {
        t = juce::jlimit (0.0f, 1.0f, t) * (float) (stops.size() - 1);
        const int i = juce::jmin ((int) t, (int) stops.size() - 2);
        return stops[(size_t) i].interpolatedWith (stops[(size_t) i + 1], t - (float) i);
    }

    //==========================================================================
    enum class FontKind { display, medium, bold, mono };

    struct Fonts
    {
        static Fonts& get() { static Fonts f; return f; }
        juce::Typeface::Ptr display = make (BinaryData::ShrikhandRegular_ttf,     BinaryData::ShrikhandRegular_ttfSize);
        juce::Typeface::Ptr medium  = make (BinaryData::SpaceGroteskMedium_ttf,   BinaryData::SpaceGroteskMedium_ttfSize);
        juce::Typeface::Ptr bold    = make (BinaryData::SpaceGroteskBold_ttf,     BinaryData::SpaceGroteskBold_ttfSize);
        juce::Typeface::Ptr mono    = make (BinaryData::JetBrainsMonoSemiBold_ttf, BinaryData::JetBrainsMonoSemiBold_ttfSize);
    private:
        static juce::Typeface::Ptr make (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); }
    };

    inline juce::Font font (FontKind k, float height, float kerning = 0.0f)
    {
        auto& f = Fonts::get();
        juce::Typeface::Ptr tf = k == FontKind::display ? f.display : k == FontKind::medium ? f.medium : k == FontKind::bold ? f.bold : f.mono;
        return juce::Font (juce::FontOptions (tf).withPointHeight (height).withKerningFactor (kerning));
    }

    //==========================================================================
    // Ring of colour around a centre, rotated by 'rotation' radians.
    inline void drawRainbowRing (juce::Graphics& g, juce::Point<float> c, float radius, float thickness,
                                 float rotation, const std::vector<juce::Colour>& stops)
    {
        constexpr int segments = 72;
        const float step = juce::MathConstants<float>::twoPi / (float) segments;
        for (int i = 0; i < segments; ++i)
        {
            juce::Path p;
            const float a0 = rotation + (float) i * step;
            p.addCentredArc (c.x, c.y, radius, radius, 0.0f, a0, a0 + step * 1.08f, true);
            g.setColour (sampleStops (stops, (float) i / (float) segments));
            g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }
    }

    inline juce::ColourGradient horizontal (juce::Colour a, juce::Colour b, juce::Rectangle<float> r)
    {
        return juce::ColourGradient (a, r.getX(), r.getCentreY(), b, r.getRight(), r.getCentreY(), false);
    }
}
