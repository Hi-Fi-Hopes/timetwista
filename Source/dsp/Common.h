#pragma once

// Small, dependency-free DSP building blocks used by every part of TimeTwista.
// No JUCE in here, so the whole engine can be tested with plain g++.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tt
{
    constexpr float pi    = 3.14159265358979f;
    constexpr float twoPi = 6.28318530717959f;

    inline float clamp01 (float x) noexcept                  { return std::min (1.0f, std::max (0.0f, x)); }
    inline float clampf (float x, float lo, float hi)         { return std::min (hi, std::max (lo, x)); }
    inline float lerp (float a, float b, float t) noexcept    { return a + (b - a) * t; }
    inline float dbToGain (float db) noexcept                 { return std::pow (10.0f, db * 0.05f); }

    // Exponential map of a 0..1 control onto [lo, hi].
    inline float expMap (float t, float lo, float hi)         { return lo * std::pow (hi / lo, clamp01 (t)); }

    // 0..1 control -> speed, with the centre of the knob at exactly 1x.
    // 0 -> lo, 0.5 -> 1, 1 -> hi. A small dead zone around the centre snaps to 1x.
    inline float centredSpeed (float t, float lo, float hi)
    {
        t = clamp01 (t);
        if (std::abs (t - 0.5f) < 0.02f) return 1.0f;
        if (t < 0.5f) return std::pow (lo, (0.5f - t) * 2.0f);
        return std::pow (hi, (t - 0.5f) * 2.0f);
    }

    // Soft saturation that is exactly linear for small signals.
    inline float softClip (float x) noexcept
    {
        const float a = std::abs (x);
        if (a < 0.8f) return x;
        const float over = a - 0.8f;
        const float y = 0.8f + over / (1.0f + over * 2.5f);   // approaches ~1.2
        return x < 0.0f ? -y : y;
    }

    //==========================================================================
    // One-pole smoother toward a target, with a time constant in samples.
    struct Smoother
    {
        void setTime (float samples) noexcept { coeff = samples <= 1.0f ? 1.0f : 1.0f - std::exp (-1.0f / samples); }
        void snap (float v) noexcept          { value = target = v; }
        float next() noexcept                 { value += (target - value) * coeff; return value; }
        float value = 0.0f, target = 0.0f, coeff = 1.0f;
    };

    //==========================================================================
    // Circular buffer read k samples back (k = 1 is the most recent write).
    struct DelayBuffer
    {
        void allocate (int maxDelaySamples)
        {
            int size = 1;
            while (size < maxDelaySamples + 8) size <<= 1;
            data.assign ((size_t) size, 0.0f);
            mask = size - 1;
            w = 0;
        }
        void clear()                    { std::fill (data.begin(), data.end(), 0.0f); }
        void push (float x) noexcept    { data[(size_t) w] = x; w = (w + 1) & mask; }
        float at (int k) const noexcept { return data[(size_t) ((w - k) & mask)]; }
        int maxDelay() const noexcept   { return mask - 4; }

        float readHermite (float d) const noexcept
        {
            d = std::min ((float) maxDelay(), std::max (2.0f, d));
            const int i = (int) d;
            const float f = d - (float) i;
            const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }

        std::vector<float> data;
        int mask = 0, w = 0;
    };

    //==========================================================================
    // Schroeder allpass with a fixed integer length (diffusion).
    struct Allpass
    {
        void allocate (int length)  { buf.assign ((size_t) std::max (1, length), 0.0f); idx = 0; }
        void clear()                { std::fill (buf.begin(), buf.end(), 0.0f); }
        float process (float x, float g) noexcept
        {
            const float d = buf[(size_t) idx];
            const float v = x + g * d;
            buf[(size_t) idx] = v;
            if (++idx == (int) buf.size()) idx = 0;
            return d - g * v;
        }
        std::vector<float> buf;
        int idx = 0;
    };

    //==========================================================================
    // One-pole TPT filter: lowpass output, highpass = input - lowpass.
    struct OnePole
    {
        void setCutoff (float hz, float sampleRate) noexcept
        {
            hz = std::min (hz, sampleRate * 0.45f);
            const float g = std::tan (pi * hz / sampleRate);
            G = g / (1.0f + g);
        }
        float lowpass (float x) noexcept
        {
            const float v = (x - s) * G;
            const float y = v + s;
            s = y + v;
            return y;
        }
        float highpass (float x) noexcept { return x - lowpass (x); }
        void clear() noexcept { s = 0.0f; }
        float G = 1.0f, s = 0.0f;
    };

    //==========================================================================
    // 12 dB/oct state-variable lowpass (TPT form).
    struct SVFLowpass
    {
        void setCutoff (float hz, float sampleRate, float q = 0.7071f) noexcept
        {
            hz = std::min (hz, sampleRate * 0.49f);
            const float g = std::tan (pi * hz / sampleRate);
            k  = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        float process (float x) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return v2;
        }
        void clear() noexcept { ic1 = ic2 = 0.0f; }
        float k = 1.414f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    };

    //==========================================================================
    // Tilt EQ: dark <-> bright around ~800 Hz. tone 0.5 is flat, 0/1 = -/+12 dB tilt.
    struct Tilt
    {
        void prepare (float sampleRate) { split.setCutoff (800.0f, sampleRate); split.clear(); }
        void setTone (float tone) noexcept
        {
            const float db = (clamp01 (tone) - 0.5f) * 24.0f;
            gHigh = dbToGain (db * 0.5f);
            gLow  = dbToGain (-db * 0.5f);
        }
        float process (float x) noexcept
        {
            const float lo = split.lowpass (x);
            return lo * gLow + (x - lo) * gHigh;
        }
        void clear() noexcept { split.clear(); }
        OnePole split;
        float gLow = 1.0f, gHigh = 1.0f;
    };

    //==========================================================================
    // Cheap, deterministic noise for modulation (no <random> needed in the audio thread).
    struct Rng
    {
        uint32_t s = 0x12345678u;
        float next() noexcept   // -1..1
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (float) (s & 0xFFFFFF) / 8388607.5f - 1.0f;
        }
    };

    // Slowly wandering random value in -1..1 (smoothed noise), for organic drift.
    struct Wander
    {
        void prepare (float sampleRate, float hz, uint32_t seed)
        {
            rng.s = seed | 1u;
            period = std::max (1, (int) (sampleRate / std::max (0.01f, hz)));
            counter = 0; from = 0.0f; to = rng.next();
        }
        float next() noexcept
        {
            if (++counter >= period) { counter = 0; from = to; to = rng.next(); }
            const float t = (float) counter / (float) period;
            const float s = t * t * (3.0f - 2.0f * t);
            return lerp (from, to, s);
        }
        Rng rng;
        int period = 1, counter = 0;
        float from = 0.0f, to = 0.0f;
    };
}
