#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using Attr = juce::AudioParameterFloatAttributes;

    Attr percent()
    {
        return Attr().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; });
    }

    float knobForRatio (float r) { return (std::log2 (r) + 2.0f) / 3.0f; }

    struct PresetValue { const char* id; float value; };   // plain values: knobs 0..1, choices = index, bools 0/1
    struct Preset { const char* name; std::vector<PresetValue> values; };

    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> list = {
            { "Init", {} },
            { "Kaleidoscope", {
                { ids::wetMode, 0 }, { ids::wetTime, 0.7f }, { ids::wetModify, 0.75f }, { ids::wetMix, 0.45f },
                { ids::wetTone, 0.6f }, { ids::wetDrift, 0.55f }, { ids::wetWidth, 0.8f },
                { ids::loopMode, 2 }, { ids::loopLength, 0.55f }, { ids::loopModify, 0.3f }, { ids::loopLevel, 0.7f },
                { ids::loopTone, 0.55f }, { ids::wetToLoop, 1 } } },
            { "Half-Speed Haze", {
                { ids::clock, knobForRatio (0.5f) }, { ids::wetMode, 0 }, { ids::wetModify, 0.85f }, { ids::wetMix, 0.5f },
                { ids::wetTone, 0.35f }, { ids::wetDrift, 0.4f }, { ids::loopMode, 1 }, { ids::loopModify, 0.5f } } },
            { "Glitch Garden", {
                { ids::clock, knobForRatio (1.5f) }, { ids::wetMode, 1 }, { ids::wetTime, 0.35f }, { ids::wetModify, 0.55f },
                { ids::wetMix, 0.4f }, { ids::wetDrift, 0.2f }, { ids::loopMode, 2 }, { ids::loopLength, 0.25f },
                { ids::loopModify, 0.8f }, { ids::loopDirt, 0.45f }, { ids::reverse, 1 } } },
            { "Tape Ghost", {
                { ids::wetMode, 1 }, { ids::wetTime, 0.6f }, { ids::wetModify, 0.45f }, { ids::wetMix, 0.35f },
                { ids::wetTone, 0.3f }, { ids::wetDrift, 0.8f }, { ids::loopMode, 1 }, { ids::loopModify, 0.35f },
                { ids::loopTone, 0.3f }, { ids::loopDirt, 0.2f }, { ids::loopToWet, 1 } } },
            { "Twista Spiral", {
                { ids::wetMode, 2 }, { ids::wetTime, 0.55f }, { ids::wetModify, 0.93f }, { ids::wetMix, 0.45f },
                { ids::wetDrift, 0.5f }, { ids::wetWidth, 0.9f }, { ids::loopMode, 0 }, { ids::loopModify, 0.85f } } },
        };
        return list;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TimeTwistaProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto knob = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def, percent()));
    };
    auto toggle = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::clock, 1 }, "Clock", NormalisableRange<float> (0.0f, 1.0f),
        2.0f / 3.0f, Attr().withStringFromValueFunction ([] (float v, int)
        {
            return juce::String (tt::clockSteps[tt::clockStepIndex (v)].name);
        })));
    toggle (ids::clockSteps, "Clock Steps", true);

    toggle (ids::wetOn, "Wet On", true);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::wetMode, 1 }, "Wet Mode", wetModeNames(), 0));
    knob (ids::wetTime,   "Wet Time",   0.5f);
    knob (ids::wetModify, "Wet Modify", 0.6f);
    knob (ids::wetMix,    "Wet Mix",    0.35f);
    knob (ids::wetTone,   "Wet Tone",   0.5f);
    knob (ids::wetDrift,  "Wet Drift",  0.3f);
    knob (ids::wetWidth,  "Wet Width",  0.5f);
    toggle (ids::freeze, "Freeze", false);
    toggle (ids::trails, "Trails", true);

    toggle (ids::loopOn, "Looper On", false);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::loopMode, 1 }, "Looper Mode", loopModeNames(), 1));
    knob (ids::loopLength, "Loop Length", 0.5f);
    knob (ids::loopModify, "Loop Modify", 0.5f);
    knob (ids::loopLevel,  "Loop Level",  0.8f);
    knob (ids::loopFade,   "Loop Fade",   1.0f);
    knob (ids::loopTone,   "Loop Tone",   0.5f);
    knob (ids::loopDirt,   "Loop Dirt",   0.0f);
    toggle (ids::reverse, "Reverse", false);

    toggle (ids::inToWet,   "In to Wet",      true);
    toggle (ids::inToLoop,  "In to Looper",   true);
    toggle (ids::loopToWet, "Looper to Wet",  false);
    toggle (ids::wetToLoop, "Wet to Looper",  false);

    toggle (ids::sync, "Tempo Sync", false);
    return layout;
}

//==============================================================================
TimeTwistaProcessor::TimeTwistaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TimeTwistaState", createLayout())
{
    pClock = p (ids::clock); pClockSteps = p (ids::clockSteps);
    pWetOn = p (ids::wetOn); pWetMode = p (ids::wetMode); pWetTime = p (ids::wetTime); pWetModify = p (ids::wetModify);
    pWetMix = p (ids::wetMix); pWetTone = p (ids::wetTone); pWetDrift = p (ids::wetDrift); pWetWidth = p (ids::wetWidth);
    pFreeze = p (ids::freeze); pTrails = p (ids::trails);
    pLoopOn = p (ids::loopOn); pLoopMode = p (ids::loopMode); pLoopLength = p (ids::loopLength); pLoopModify = p (ids::loopModify);
    pLoopLevel = p (ids::loopLevel); pLoopFade = p (ids::loopFade); pLoopTone = p (ids::loopTone); pLoopDirt = p (ids::loopDirt);
    pReverse = p (ids::reverse);
    pInToWet = p (ids::inToWet); pInToLoop = p (ids::inToLoop); pLoopToWet = p (ids::loopToWet); pWetToLoop = p (ids::wetToLoop);
    pSync = p (ids::sync);

    for (auto& b : displayBars) b.store (0.0f);
}

std::atomic<float>* TimeTwistaProcessor::p (const char* id) const
{
    auto* v = apvts.getRawParameterValue (id);
    jassert (v != nullptr);
    return v;
}

bool TimeTwistaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void TimeTwistaProcessor::prepareToPlay (double sampleRate, int)
{
    hostRate.store (sampleRate);
    engine.prepare (sampleRate);
    scratchL.assign (512, 0.0f);
    scratchR.assign (512, 0.0f);
}

tt::Settings TimeTwistaProcessor::readSettings() const
{
    auto b = [] (const std::atomic<float>* v) { return v->load() > 0.5f; };
    tt::Settings s;
    s.clock = pClock->load();            s.clockSteps = b (pClockSteps);
    s.wetOn = b (pWetOn);                s.wetMode = (tt::WetMode) juce::jlimit (0, 2, (int) pWetMode->load());
    s.wetTime = pWetTime->load();        s.wetModify = pWetModify->load();
    s.wetMix = pWetMix->load();          s.wetTone = pWetTone->load();
    s.wetDrift = pWetDrift->load();      s.wetWidth = pWetWidth->load();
    s.freeze = b (pFreeze);              s.trails = b (pTrails);
    s.loopOn = b (pLoopOn);              s.loopMode = (tt::LoopMode) juce::jlimit (0, 2, (int) pLoopMode->load());
    s.loopLength = pLoopLength->load();  s.loopModify = pLoopModify->load();
    s.loopLevel = pLoopLevel->load();    s.loopFade = pLoopFade->load();
    s.loopTone = pLoopTone->load();      s.loopDirt = pLoopDirt->load();
    s.reverse = b (pReverse);
    s.inToWet = b (pInToWet);            s.inToLoop = b (pInToLoop);
    s.loopToWet = b (pLoopToWet);        s.wetToLoop = b (pWetToLoop);
    s.sync = b (pSync);                  s.bpm = hostBpm.load();
    return s;
}

void TimeTwistaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm.store ((float) *bpm);

    const auto settings = readSettings();
    const int numIn = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);
    int captures = captureRequests.exchange (0);

    for (int start = 0; start < numSamples; start += 512)
    {
        const int n = std::min (512, numSamples - start);
        // Copy the input first: the engine writes the output over the same buffer.
        std::copy (outL + start, outL + start + n, scratchL.begin());
        if (numIn >= 2) std::copy (outR + start, outR + start + n, scratchR.begin());
        else            std::copy (outL + start, outL + start + n, scratchR.begin());

        engine.process (settings, scratchL.data(), scratchR.data(), outL + start, outR + start, n, captures);
        captures = 0;
    }

    float bars[tt::Looper::numBars], playhead;
    engine.looper.getDisplay (bars, playhead);
    for (int i = 0; i < tt::Looper::numBars; ++i) displayBars[(size_t) i].store (bars[i]);
    displayPlayhead.store (playhead);
    displayRatio.store (engine.currentRatio());
}

void TimeTwistaProcessor::requestCapture()
{
    if (auto* on = apvts.getParameter (ids::loopOn); on != nullptr && on->getValue() < 0.5f)
    {
        on->beginChangeGesture();
        on->setValueNotifyingHost (1.0f);   // switching on captures
        on->endChangeGesture();
    }
    else
    {
        captureRequests.fetch_add (1);
    }
}

//==============================================================================
juce::StringArray TimeTwistaProcessor::presetNames()
{
    juce::StringArray names;
    for (auto& pr : presets()) names.add (pr.name);
    return names;
}

int TimeTwistaProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String TimeTwistaProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets().size()) ? presets()[(size_t) index].name : "";
}

void TimeTwistaProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size())) return;
    currentPreset = index;

    // Start from defaults (except Looper On, routing to the outside world stays sane), then apply the preset.
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (rp->getParameterID() == ids::loopOn || rp->getParameterID() == ids::sync) continue;
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }

    for (auto& v : presets()[(size_t) index].values)
        if (auto* rp = apvts.getParameter (v.id))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 (v.value));
            rp->endChangeGesture();
        }
}

//==============================================================================
void TimeTwistaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TimeTwistaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset = tree.getProperty ("preset", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* TimeTwistaProcessor::createEditor() { return new TimeTwistaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TimeTwistaProcessor(); }
