#pragma once

// The Clock: both channels run at an internal sample rate = host rate x ratio.
// Lower ratio = slower, lower-pitched, grittier (aliasing is part of the sound).
// Higher ratio = faster, higher-pitched, cleaner.

#include "Common.h"

namespace tt
{
    struct ClockStep { float ratio; const char* name; };

    // Musical steps, low to high. Names read "internal : host".
    inline constexpr ClockStep clockSteps[] = {
        { 1.0f / 4.0f, "1 : 4" }, { 1.0f / 3.0f, "1 : 3" }, { 1.0f / 2.0f, "1 : 2" },
        { 2.0f / 3.0f, "2 : 3" }, { 3.0f / 4.0f, "3 : 4" }, { 1.0f,        "1 : 1" },
        { 4.0f / 3.0f, "4 : 3" }, { 3.0f / 2.0f, "3 : 2" }, { 2.0f,        "2 : 1" },
    };
    inline constexpr int numClockSteps = (int) (sizeof (clockSteps) / sizeof (clockSteps[0]));

    // Knob 0..1 -> continuous ratio 1/4..2 (log scale; 2/3 of the way = 1:1).
    inline float clockRatioSmooth (float knob) { return std::pow (2.0f, -2.0f + 3.0f * clamp01 (knob)); }

    inline int clockStepIndex (float knob)
    {
        const float lr = std::log2 (clockRatioSmooth (knob));
        int best = 0; float bestD = 1e9f;
        for (int i = 0; i < numClockSteps; ++i)
        {
            const float d = std::abs (std::log2 (clockSteps[i].ratio) - lr);
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    inline float clockRatio (float knob, bool steps)
    {
        return steps ? clockSteps[clockStepIndex (knob)].ratio : clockRatioSmooth (knob);
    }
}
