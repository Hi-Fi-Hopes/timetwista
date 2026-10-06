#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Engine.h"
#include <array>
#include <atomic>

namespace ids
{
    inline constexpr const char* clock      = "clock";
    inline constexpr const char* clockSteps = "clockSteps";

    inline constexpr const char* wetOn      = "wetOn";
    inline constexpr const char* wetMode    = "wetMode";
    inline constexpr const char* wetTime    = "wetTime";
    inline constexpr const char* wetModify  = "wetModify";
    inline constexpr const char* wetMix     = "wetMix";
    inline constexpr const char* wetTone    = "wetTone";
    inline constexpr const char* wetDrift   = "wetDrift";
    inline constexpr const char* wetWidth   = "wetWidth";
    inline constexpr const char* freeze     = "freeze";
    inline constexpr const char* trails     = "trails";

    inline constexpr const char* loopOn     = "loopOn";
    inline constexpr const char* loopMode   = "loopMode";
    inline constexpr const char* loopLength = "loopLength";
    inline constexpr const char* loopModify = "loopModify";
    inline constexpr const char* loopLevel  = "loopLevel";
    inline constexpr const char* loopFade   = "loopFade";
    inline constexpr const char* loopTone   = "loopTone";
    inline constexpr const char* loopDirt   = "loopDirt";
    inline constexpr const char* reverse    = "reverse";

    inline constexpr const char* inToWet    = "inToWet";
    inline constexpr const char* inToLoop   = "inToLoop";
    inline constexpr const char* loopToWet  = "loopToWet";
    inline constexpr const char* wetToLoop  = "wetToLoop";

    inline constexpr const char* sync       = "sync";
}

class TimeTwistaProcessor : public juce::AudioProcessor
{
public:
    TimeTwistaProcessor();
    ~TimeTwistaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TimeTwista mk1"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::StringArray wetModeNames()  { return { "Reverb", "Delay", "Slip" }; }
    static juce::StringArray loopModeNames() { return { "Env", "Tape", "Stretch" }; }
    static juce::StringArray presetNames();

    // Capture button: grabs a new loop (and switches the looper on if it's off).
    void requestCapture();

    // For the UI (written by the audio thread, read by the editor's timer).
    std::array<std::atomic<float>, tt::Looper::numBars> displayBars {};
    std::atomic<float> displayPlayhead { -1.0f };
    std::atomic<float> displayRatio { 1.0f };
    std::atomic<float> hostBpm { 0.0f };
    std::atomic<double> hostRate { 48000.0 };

    juce::AudioProcessorValueTreeState apvts;

private:
    tt::Settings readSettings() const;

    tt::Engine engine;
    std::vector<float> scratchL, scratchR;
    std::atomic<int> captureRequests { 0 };
    int currentPreset = 0;

    std::atomic<float>* p (const char* id) const;
    std::atomic<float> *pClock, *pClockSteps, *pWetOn, *pWetMode, *pWetTime, *pWetModify, *pWetMix, *pWetTone,
                       *pWetDrift, *pWetWidth, *pFreeze, *pTrails, *pLoopOn, *pLoopMode, *pLoopLength, *pLoopModify,
                       *pLoopLevel, *pLoopFade, *pLoopTone, *pLoopDirt, *pReverse, *pInToWet, *pInToLoop,
                       *pLoopToWet, *pWetToLoop, *pSync;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeTwistaProcessor)
};
