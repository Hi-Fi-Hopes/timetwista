#include "PluginEditor.h"

namespace
{
    using Style = PillButton::Style;
    const juce::Colour clear = juce::Colours::transparentBlack;

    Style headerStyle()  { return { theme::lime, theme::lime, theme::bg, theme::panel, theme::border, theme::text, 13.0f, false, false, true }; }
    Style freezeStyle()  { return { theme::cyan, theme::cyan, theme::bg, theme::inset, theme::cyan, theme::text, 13.0f, true, true, false }; }
    Style trailsStyle()  { return { theme::violet, theme::violet, theme::text, theme::inset, theme::border, theme::muted, 13.0f, false, false, true }; }
    Style captureStyle() { return { theme::lime, theme::cyan, theme::bg, theme::inset, theme::lime, theme::text, 13.0f, true, true, false }; }
    Style reverseStyle() { return { theme::violet, theme::pink, theme::bg, theme::inset, theme::violet, theme::text, 13.0f, true, false, false }; }
    Style stepsStyle()   { return { theme::lime, theme::lime, theme::text, theme::panel, theme::border, theme::text, 11.0f, false, false, true }; }
    Style routeStyle (juce::Colour c) { return { c, c, theme::bg, clear, theme::border, theme::muted, 12.0f, true, false, false }; }

    const juce::String arrow = juce::String::fromUTF8 (" \xe2\x86\x92 ");

    juce::String pct (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; }
    juce::String secs (float s)
    {
        return s < 1.0f ? juce::String (juce::roundToInt (s * 1000.0f)) + " ms" : juce::String (s, 2) + " s";
    }
    juce::String tilt (double v)
    {
        const float db = ((float) v - 0.5f) * 24.0f;
        if (std::abs (db) < 0.3f) return "flat";
        return (db > 0 ? "bright +" : "dark ") + juce::String (db, 1) + " dB";
    }
}

//==============================================================================
ClockView::ClockView (TimeTwistaProcessor& p)
    : steps ("Steps", stepsStyle()), proc (p),
      knobAttachment (p.apvts, ids::clock, knob),
      stepsAttachment (p.apvts, ids::clockSteps, steps)
{
    addAndMakeVisible (knob);
    addAndMakeVisible (steps);
    steps.setClickingTogglesState (true);
    knob.setDoubleClickReturnValue (true, 2.0 / 3.0);
    knob.setTitle ("Clock");
}

void ClockView::resized()
{
    knob.setBounds (juce::Rectangle<int> (78, 78).withCentre ({ getWidth() / 2, 90 }));
    steps.setBounds (170, 166, 62, 26);
}

void ClockView::tick()
{
    // Rings flow outward at a speed set by the clock: faster clock, faster flow.
    ringPhase = std::fmod (ringPhase + 0.9f * proc.displayRatio.load(), 32.0f);
    steps.setButtonText (steps.getToggleState() ? "Steps" : "Smooth");
    repaint();
}

void ClockView::paint (juce::Graphics& g)
{
    const float cx = (float) getWidth() * 0.5f, cy = 90.0f;

    g.setFont (theme::font (theme::FontKind::bold, 12.0f, 0.26f));
    g.setColour (theme::muted);
    g.drawText ("CLOCK", 0, 0, getWidth(), 14, juce::Justification::centred, false);

    // Op-art rings: 4 px stripes, repeating every 32 px, clipped to the dial.
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path dial; dial.addEllipse (cx - 66.0f, cy - 66.0f, 132.0f, 132.0f);
        g.reduceClipRegion (dial);
        g.setColour (theme::bg);
        g.fillPath (dial);
        const juce::Colour cols[4] = { theme::pink, theme::orange, theme::lime, theme::cyan };
        for (int k = -1; k < 3; ++k)
            for (int i = 0; i < 4; ++i)
            {
                const float r0 = ringPhase + (float) (k * 32 + i * 8);
                if (r0 + 4.0f < 36.0f || r0 > 70.0f) continue;
                juce::Path ring;
                ring.addCentredArc (cx, cy, r0 + 2.0f, r0 + 2.0f, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
                g.setColour (cols[i]);
                g.strokePath (ring, juce::PathStrokeType (4.0f));
            }
    }

    // Ratio readout
    const float ratio = proc.displayRatio.load();
    const bool stepsOn = proc.apvts.getRawParameterValue (ids::clockSteps)->load() > 0.5f;
    const float knobVal = proc.apvts.getRawParameterValue (ids::clock)->load();
    const juce::String ratioText = stepsOn ? juce::String (tt::clockSteps[tt::clockStepIndex (knobVal)].name)
                                           : juce::String::fromUTF8 ("\xc3\x97") + juce::String (ratio, 2);
    g.setFont (theme::font (theme::FontKind::mono, 20.0f));
    g.setColour (theme::text);
    g.drawText (ratioText, 0, 166, 96, 26, juce::Justification::centredRight, false);
    g.setFont (theme::font (theme::FontKind::mono, 11.0f));
    g.setColour (theme::muted);
    const double khz = proc.hostRate.load() * ratio / 1000.0;
    g.drawText (juce::String::fromUTF8 ("\xe2\x89\x88") + juce::String (juce::roundToInt (khz)) + " kHz", 104, 166, 62, 26,
                juce::Justification::centredLeft, false);

    // Step row, current one lit
    const int idx = tt::clockStepIndex (knobVal);
    const char* names[] = { "1/4", "1/3", "1/2", "2/3", "3/4", "1", "4/3", "3/2", "2" };
    g.setFont (theme::font (theme::FontKind::mono, 10.0f));
    const float slot = (float) getWidth() / 9.0f;
    for (int i = 0; i < 9; ++i)
    {
        g.setColour (i == idx ? theme::lime : theme::muted.withAlpha (stepsOn ? 1.0f : 0.5f));
        g.drawText (names[i], juce::Rectangle<float> ((float) i * slot, 200.0f, slot, 14.0f), juce::Justification::centred, false);
    }
}

//==============================================================================
Content::Content (TimeTwistaProcessor& p)
    : proc (p),
      presetBtn ("Preset", headerStyle()), syncBtn ("Sync", headerStyle()),
      wetSwitch (theme::pink, theme::orange), loopSwitch (theme::lime, theme::cyan),
      wetMode (p.apvts, ids::wetMode, TimeTwistaProcessor::wetModeNames(), theme::pink, theme::orange),
      loopMode (p.apvts, ids::loopMode, TimeTwistaProcessor::loopModeNames(), theme::lime, theme::cyan),
      wetTime    (p.apvts, ids::wetTime,    "Time",   theme::wetStops(), false),
      wetModify  (p.apvts, ids::wetModify,  "Modify", theme::wetStops(), false),
      wetMix     (p.apvts, ids::wetMix,     "Mix",    theme::wetStops(), false),
      wetTone    (p.apvts, ids::wetTone,    "Tone",   theme::wetStops(), true),
      wetDrift   (p.apvts, ids::wetDrift,   "Drift",  theme::wetStops(), true),
      wetWidth   (p.apvts, ids::wetWidth,   "Width",  theme::wetStops(), true),
      loopLength (p.apvts, ids::loopLength, "Length", theme::loopStops(), false),
      loopModify (p.apvts, ids::loopModify, "Modify", theme::loopStops(), false),
      loopLevel  (p.apvts, ids::loopLevel,  "Level",  theme::loopStops(), false),
      loopFade   (p.apvts, ids::loopFade,   "Fade",   theme::loopStops(), true),
      loopTone   (p.apvts, ids::loopTone,   "Tone",   theme::loopStops(), true),
      loopDirt   (p.apvts, ids::loopDirt,   "Dirt",   theme::loopStops(), true),
      freezeBtn ("Freeze", freezeStyle()), trailsBtn ("Trails", trailsStyle()),
      captureBtn ("Capture", captureStyle()), reverseBtn ("Reverse", reverseStyle()),
      clock (p),
      xy (*p.apvts.getParameter (ids::wetModify), *p.apvts.getParameter (ids::loopModify)),
      rInToWet   ("In" + arrow + "Wet",       routeStyle (theme::pink)),
      rInToLoop  ("In" + arrow + "Looper",    routeStyle (theme::lime)),
      rLoopToWet ("Looper" + arrow + "Wet",   routeStyle (theme::cyan)),
      rWetToLoop ("Wet" + arrow + "Looper",   routeStyle (theme::orange))
{
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &presetBtn, &syncBtn, &wetSwitch, &loopSwitch, &wetMode, &loopMode,
             &wetTime, &wetModify, &wetMix, &wetTone, &wetDrift, &wetWidth,
             &loopLength, &loopModify, &loopLevel, &loopFade, &loopTone, &loopDirt,
             &freezeBtn, &trailsBtn, &captureBtn, &reverseBtn, &waveform, &clock, &xy,
             &rInToWet, &rInToLoop, &rLoopToWet, &rWetToLoop })
        addAndMakeVisible (c);

    wetSwitch.setTitle ("Wet channel on");
    loopSwitch.setTitle ("Looper on");

    auto attach = [this] (const char* id, juce::Button& b)
    {
        b.setClickingTogglesState (true);
        buttonAttachments.push_back (std::make_unique<ButtonAttach> (proc.apvts, id, b));
    };
    attach (ids::sync, syncBtn);
    attach (ids::wetOn, wetSwitch);
    attach (ids::loopOn, loopSwitch);
    attach (ids::freeze, freezeBtn);
    attach (ids::trails, trailsBtn);
    attach (ids::reverse, reverseBtn);
    attach (ids::inToWet, rInToWet);
    attach (ids::inToLoop, rInToLoop);
    attach (ids::loopToWet, rLoopToWet);
    attach (ids::wetToLoop, rWetToLoop);

    captureBtn.setToggleState (true, juce::dontSendNotification);
    captureBtn.onClick = [this] { proc.requestCapture(); };
    presetBtn.onClick = [this] { showPresetMenu(); };

    waveform.source = [this] (float* bars, float& playhead)
    {
        for (int i = 0; i < 48; ++i) bars[i] = proc.displayBars[(size_t) i].load();
        playhead = proc.displayPlayhead.load();
    };

    // Readouts that depend on mode, clock and tempo
    wetTime.valueText = [this] (double v)
    {
        const int mode = (int) param (ids::wetMode);
        if (mode == 0) return "size " + juce::String (tt::maps::reverbSize ((float) v), 2) + "x";
        const float base = mode == 1 ? tt::maps::delaySeconds ((float) v) : tt::maps::slipSeconds ((float) v);
        const float bpm = proc.hostBpm.load();
        if (param (ids::sync) > 0.5f && bpm > 0.0f)
            return juce::String (tt::maps::delayBeatNames[tt::maps::nearestBeatIndex (base, bpm, tt::maps::delayBeats)]);
        return secs (base / clockRatio());
    };
    wetModify.valueText = [this] (double v)
    {
        const int mode = (int) param (ids::wetMode);
        if (mode == 0) return "decay " + secs (tt::maps::reverbDecaySeconds ((float) v) / clockRatio());
        if (mode == 1) return "fdbk " + juce::String (juce::roundToInt (tt::maps::delayFeedback ((float) v) * 100.0f)) + "%";
        return juce::String (tt::maps::slipNames[tt::maps::slipIndex ((float) v)]);
    };
    wetTone.valueText = tilt;
    loopTone.valueText = tilt;
    wetMix.valueText = pct;
    wetDrift.valueText = pct;
    wetWidth.valueText = [] (double v) { return v < 0.02 ? juce::String ("mono") : pct (v * 2.0); };
    loopLength.valueText = [this] (double v)
    {
        const float s = tt::maps::loopSeconds ((float) v);
        const float bpm = proc.hostBpm.load();
        if (param (ids::sync) > 0.5f && bpm > 0.0f)
            return juce::String (tt::maps::loopBeatNames[tt::maps::nearestBeatIndex (s, bpm, tt::maps::loopBeats)]);
        return secs (s / clockRatio());
    };
    loopModify.valueText = [this] (double v)
    {
        const int mode = (int) param (ids::loopMode);
        if (mode == 0)
        {
            const float a = tt::maps::envAmount ((float) v);
            if (std::abs (a) < 0.03f) return juce::String ("steady");
            return (a < 0 ? "duck " : "swell ") + juce::String (juce::roundToInt (std::abs (a) * 100.0f)) + "%";
        }
        const float sp = mode == 1 ? tt::maps::tapeSpeed ((float) v) : tt::maps::stretchRate ((float) v);
        return juce::String (sp, 2) + "x";
    };
    loopLevel.valueText = pct;
    loopFade.valueText = [] (double v) { return v > 0.995 ? juce::String ("hold") : pct (tt::maps::fadePerPass ((float) v)) + "/pass"; };
    loopDirt.valueText = pct;

    refreshLabels();
    startTimerHz (30);
}

Content::~Content() { stopTimer(); }

float Content::clockRatio() const
{
    return tt::clockRatio (param (ids::clock), param (ids::clockSteps) > 0.5f);
}

void Content::refreshLabels()
{
    presetBtn.setButtonText ("Preset: " + proc.getProgramName (proc.getCurrentProgram()));
    syncBtn.setButtonText (syncBtn.getToggleState() ? "Sync: Host" : "Sync: Free");
}

void Content::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto names = TimeTwistaProcessor::presetNames();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i], true, i == proc.getCurrentProgram());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn),
                        [safe = juce::Component::SafePointer<Content> (this)] (int result)
                        {
                            if (safe == nullptr || result <= 0) return;
                            safe->proc.setCurrentProgram (result - 1);
                            safe->refreshLabels();
                        });
}

void Content::timerCallback()
{
    refreshLabels();
    waveform.update();
    clock.tick();
    xy.repaint();

    const juce::String xyText = "x " + juce::String (param (ids::wetModify), 2) + juce::String::fromUTF8 (" \xc2\xb7 y ")
                              + juce::String (param (ids::loopModify), 2);
    if (xyText != lastXYText) { lastXYText = xyText; repaint (355, 296, 250, 18); }

    if (proc.hostBpm.load() != lastBpm) { lastBpm = proc.hostBpm.load(); repaint (600, 548, 360, 52); }
}

//==============================================================================
// Layout (all in design pixels)
namespace layout
{
    constexpr int wetX = 24, loopX = 621, panelY = 72, panelW = 315, panelH = 460, pad = 18;
    constexpr int centreX = 355, centreW = 250;
    constexpr int footerY = 548;
}

void Content::resized()
{
    using namespace layout;
    syncBtn.setBounds (852, 10, 84, 36);
    presetBtn.setBounds (852 - 8 - 210, 10, 210, 36);

    auto knobRow = [] (int x, int y, int h, std::initializer_list<Knob*> knobs)
    {
        const int w = (panelW - 2 * pad) / 3;
        int i = 0;
        for (auto* k : knobs) k->setBounds (x + pad + w * i++, y, w, h);
    };

    // Wet
    wetSwitch.setBounds (wetX + panelW - pad - 44, panelY + pad, 44, 24);
    wetMode.setBounds (wetX + pad, panelY + 56, panelW - 2 * pad, 38);
    knobRow (wetX, panelY + 108, 82, { &wetTime, &wetModify, &wetMix });
    knobRow (wetX, panelY + 214, 72, { &wetTone, &wetDrift, &wetWidth });
    const int bw = (panelW - 2 * pad - 8) / 2;
    freezeBtn.setBounds (wetX + pad, panelY + panelH - pad - 44, bw, 44);
    trailsBtn.setBounds (wetX + pad + bw + 8, panelY + panelH - pad - 44, bw, 44);

    // Looper
    loopSwitch.setBounds (loopX + panelW - pad - 44, panelY + pad, 44, 24);
    loopMode.setBounds (loopX + pad, panelY + 56, panelW - 2 * pad, 38);
    waveform.setBounds (loopX + pad, panelY + 108, panelW - 2 * pad, 60);
    knobRow (loopX, panelY + 182, 82, { &loopLength, &loopModify, &loopLevel });
    knobRow (loopX, panelY + 288, 72, { &loopFade, &loopTone, &loopDirt });
    captureBtn.setBounds (loopX + pad, panelY + panelH - pad - 44, bw, 44);
    reverseBtn.setBounds (loopX + pad + bw + 8, panelY + panelH - pad - 44, bw, 44);

    // Centre
    clock.setBounds (centreX, panelY, centreW, 214);
    xy.setBounds (centreX, 320, centreW, 150);

    // Routing chips, sized to their text
    int x = 134;
    for (auto* b : { &rInToWet, &rInToLoop, &rLoopToWet, &rWetToLoop })
    {
        const auto f = theme::font (theme::FontKind::bold, 12.0f);
        const int w = (int) juce::GlyphArrangement::getStringWidth (f, b->getButtonText()) + 30;
        b->setBounds (x, footerY, w, 36);
        x += w + 10;
    }
}

void Content::paint (juce::Graphics& g)
{
    using namespace layout;
    const auto all = getLocalBounds().toFloat();

    // Background with soft colour glows
    g.fillAll (theme::bg);
    g.setGradientFill (juce::ColourGradient (theme::pink.withAlpha (0.28f), 77, 0, theme::pink.withAlpha (0.0f), 77 + 448, 0, true));
    g.fillRect (all);
    g.setGradientFill (juce::ColourGradient (theme::cyan.withAlpha (0.22f), 922, 600, theme::cyan.withAlpha (0.0f), 922 + 506, 600, true));
    g.fillRect (all);
    g.setGradientFill (juce::ColourGradient (theme::lime.withAlpha (0.08f), 480, 270, theme::lime.withAlpha (0.0f), 480 + 165, 270, true));
    g.fillRect (all);

    // Header
    g.setColour (theme::border);
    g.fillRect (0, 55, getWidth(), 1);
    {
        const auto titleFont = theme::font (theme::FontKind::display, 26.0f);
        const float tw = juce::GlyphArrangement::getStringWidth (titleFont, "TimeTwista");
        juce::ColourGradient grad (theme::pink, 24, 0, theme::cyan, 24 + tw, 0, false);
        grad.addColour (0.33, theme::orange);
        grad.addColour (0.66, theme::lime);
        g.setGradientFill (grad);
        g.setFont (titleFont);
        g.drawText ("TimeTwista", 24, 6, (int) tw + 8, 44, juce::Justification::centredLeft, false);

        auto badge = juce::Rectangle<float> (24.0f + tw + 14.0f, 17.0f, 46.0f, 22.0f);
        g.setColour (theme::lime);
        g.fillRoundedRectangle (badge, 11.0f);
        g.setColour (theme::bg);
        g.setFont (theme::font (theme::FontKind::mono, 12.0f, 0.2f));
        g.drawText ("MK1", badge, juce::Justification::centred, false);
    }

    // Channel panels
    auto panel = [&] (int x, const char* title)
    {
        auto r = juce::Rectangle<float> ((float) x, (float) panelY, (float) panelW, (float) panelH).reduced (0.5f);
        g.setColour (theme::panel);
        g.fillRoundedRectangle (r, 18.0f);
        g.setColour (theme::border);
        g.drawRoundedRectangle (r, 18.0f, 1.0f);
        g.setColour (theme::text);
        g.setFont (theme::font (theme::FontKind::bold, 14.0f, 0.18f));
        g.drawText (title, x + pad, panelY + pad, 200, 24, juce::Justification::centredLeft, false);
    };
    panel (wetX, "WET");
    panel (loopX, "LOOPER");

    auto insetBox = [&] (int x, int y)
    {
        g.setColour (theme::inset);
        g.fillRoundedRectangle ((float) (x + pad), (float) y, (float) (panelW - 2 * pad), 90.0f, 14.0f);
    };
    insetBox (wetX, panelY + 204);
    insetBox (loopX, panelY + 278);

    // X/Y caption and readout
    g.setColour (theme::muted);
    g.setFont (theme::font (theme::FontKind::bold, 12.0f, 0.26f));
    g.drawText ("X / Y", centreX, 298, 80, 16, juce::Justification::centredLeft, false);
    g.setColour (theme::text);
    g.setFont (theme::font (theme::FontKind::mono, 11.0f));
    g.drawText ("x " + juce::String (param (ids::wetModify), 2) + juce::String::fromUTF8 (" \xc2\xb7 y ") + juce::String (param (ids::loopModify), 2),
                centreX + 80, 298, centreW - 80, 16, juce::Justification::centredRight, false);

    // Footer
    g.setColour (theme::muted);
    g.setFont (theme::font (theme::FontKind::bold, 11.0f, 0.2f));
    g.drawText ("ROUTING", 24, footerY, 110, 36, juce::Justification::centredLeft, false);

    g.setFont (theme::font (theme::FontKind::mono, 11.0f));
    const float bpm = proc.hostBpm.load();
    const juce::String right = (param (ids::sync) > 0.5f && bpm > 0.0f)
        ? "Synced to " + juce::String (bpm, 1) + " BPM"
        : juce::String::fromUTF8 ("TimeTwista mk1 \xc2\xb7 Hi-Fi Hopes");
    g.drawText (right, 600, footerY, 336, 36, juce::Justification::centredRight, false);
}

//==============================================================================
TimeTwistaEditor::TimeTwistaEditor (TimeTwistaProcessor& p)
    : juce::AudioProcessorEditor (p), content (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 2, designH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) designW / (double) designH);
    setSize (designW, designH);
}

void TimeTwistaEditor::resized()
{
    const float scale = (float) getWidth() / (float) designW;
    content.setBounds (0, 0, designW, designH);
    content.setTransform (juce::AffineTransform::scale (scale));
}
