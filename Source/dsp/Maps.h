#pragma once

// How each 0..1 knob maps onto a real value. Shared by the DSP and the UI readouts,
// so what the screen says is what the engine does.
// Times are "reference" times: at Clock 1:1. Other clock ratios divide them by the ratio.

#include "Common.h"

namespace tt::maps
{
    inline float delaySeconds (float t)       { return expMap (t, 0.02f, 1.5f); }
    inline float slipSeconds (float t)        { return expMap (t, 0.04f, 1.5f); }
    inline float reverbSize (float t)         { return expMap (t, 0.35f, 1.6f); }
    inline float reverbDecaySeconds (float m) { return expMap (m, 0.4f, 25.0f); }
    inline float delayFeedback (float m)      { return clamp01 (m) * 1.02f; }   // a touch past 100% = self-oscillation

    inline constexpr float slipSemis[] = { -12.0f, -7.0f, -5.0f, 0.1f, 5.0f, 7.0f, 12.0f };
    inline constexpr const char* slipNames[] = { "-oct", "-5th", "-4th", "shimmer", "+4th", "+5th", "+oct" };
    inline int slipIndex (float m)            { return std::min (6, std::max (0, (int) (clamp01 (m) * 7.0f))); }

    inline float loopSeconds (float t)        { return expMap (t, 0.08f, 8.0f); }
    inline float tapeSpeed (float m)          { return centredSpeed (m, 0.25f, 2.0f); }
    inline float stretchRate (float m)        { return centredSpeed (m, 0.125f, 2.0f); }
    inline float envAmount (float m)          { return (clamp01 (m) - 0.5f) * 2.0f; }   // -1 duck .. +1 swell
    inline float fadePerPass (float f)        { return 0.5f + 0.5f * clamp01 (f); }

    // Tempo sync: snap a time to the nearest musical length.
    inline constexpr float delayBeats[] = { 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 4.0f };
    inline constexpr const char* delayBeatNames[] = { "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1" };
    inline constexpr float loopBeats[] = { 0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f };
    inline constexpr const char* loopBeatNames[] = { "1/16", "1/8", "1 beat", "2 beats", "3 beats", "1 bar", "6 beats", "2 bars", "3 bars", "4 bars" };

    template <size_t N>
    inline int nearestBeatIndex (float seconds, float bpm, const float (&beats)[N])
    {
        const float b = seconds * bpm / 60.0f;
        int best = 0; float bestD = 1e9f;
        for (int i = 0; i < (int) N; ++i)
        {
            const float d = std::abs (std::log2 (beats[i] / std::max (1e-6f, b)));
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    // Final delay/slip time in seconds at 1:1, after optional tempo snap.
    inline float syncedSeconds (float seconds, bool sync, float bpm, float maxSeconds)
    {
        if (! sync || bpm <= 0.0f) return seconds;
        const int i = nearestBeatIndex (seconds, bpm, delayBeats);
        return std::min (maxSeconds, delayBeats[i] * 60.0f / bpm);
    }

    inline float syncedLoopSeconds (float seconds, bool sync, float bpm, float maxSeconds)
    {
        if (! sync || bpm <= 0.0f) return seconds;
        const int i = nearestBeatIndex (seconds, bpm, loopBeats);
        return std::min (maxSeconds, loopBeats[i] * 60.0f / bpm);
    }
}
