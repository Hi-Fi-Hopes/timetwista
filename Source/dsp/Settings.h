#pragma once

// Everything the engine needs from the plugin's parameters, read once per block.
// All knob values are 0..1 unless noted.

namespace tt
{
    enum class WetMode   { reverb = 0, delay = 1, slip = 2 };
    enum class LoopMode  { env = 0, tape = 1, stretch = 2 };

    struct Settings
    {
        // Clock
        float clock      = 2.0f / 3.0f;   // 0..1 -> ratio 1/4..2 (2/3 of the way = 1:1)
        bool  clockSteps = true;          // snap to musical ratios, else smooth sweep

        // Wet channel
        bool    wetOn     = true;
        WetMode wetMode   = WetMode::reverb;
        float   wetTime   = 0.5f;
        float   wetModify = 0.6f;
        float   wetMix    = 0.35f;
        float   wetTone   = 0.5f;
        float   wetDrift  = 0.3f;
        float   wetWidth  = 0.5f;
        bool    freeze    = false;
        bool    trails    = true;

        // Micro-looper
        bool     loopOn     = false;
        LoopMode loopMode   = LoopMode::tape;
        float    loopLength = 0.5f;
        float    loopModify = 0.5f;
        float    loopLevel  = 0.8f;
        float    loopFade   = 1.0f;
        float    loopTone   = 0.5f;
        float    loopDirt   = 0.0f;
        bool     reverse    = false;

        // Routing
        bool inToWet   = true;
        bool inToLoop  = true;
        bool loopToWet = false;
        bool wetToLoop = false;

        // Host tempo (0 = unknown) and whether to snap times to it
        bool  sync = false;
        float bpm  = 0.0f;
    };
}
