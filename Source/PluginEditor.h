#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/Widgets.h"

//==============================================================================
// The Clock: op-art rings that flow at the clock's speed, a big rainbow knob,
// the ratio readout and the Steps/Smooth switch.
class ClockView : public juce::Component
{
public:
    ClockView (TimeTwistaProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();   // called from the editor's timer

    RainbowSlider knob { theme::wetStops() };
    PillButton steps;

private:
    TimeTwistaProcessor& proc;
    juce::AudioProcessorValueTreeState::SliderAttachment knobAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment stepsAttachment;
    float ringPhase = 0.0f;
};

//==============================================================================
// Everything, laid out at the design size (960 x 600). The editor scales it.
class Content : public juce::Component, private juce::Timer
{
public:
    explicit Content (TimeTwistaProcessor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPresetMenu();
    void refreshLabels();
    float clockRatio() const;
    float param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    TimeTwistaProcessor& proc;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    PillButton presetBtn, syncBtn;

    Switch wetSwitch, loopSwitch;
    Segmented wetMode, loopMode;
    Knob wetTime, wetModify, wetMix, wetTone, wetDrift, wetWidth;
    Knob loopLength, loopModify, loopLevel, loopFade, loopTone, loopDirt;
    PillButton freezeBtn, trailsBtn, captureBtn, reverseBtn;
    Waveform waveform;
    ClockView clock;
    XYPad xy;
    PillButton rInToWet, rInToLoop, rLoopToWet, rWetToLoop;

    std::vector<std::unique_ptr<ButtonAttach>> buttonAttachments;
    juce::String lastXYText;
    float lastBpm = -1.0f;
};

//==============================================================================
class TimeTwistaEditor : public juce::AudioProcessorEditor
{
public:
    explicit TimeTwistaEditor (TimeTwistaProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (theme::bg); }

    static constexpr int designW = 960, designH = 600;

private:
    Content content;
};
