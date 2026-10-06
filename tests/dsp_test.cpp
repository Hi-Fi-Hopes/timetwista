// Standalone DSP test for TimeTwista (no JUCE).
//   g++ -O2 -std=c++17 -I Source tests/dsp_test.cpp -o /tmp/dsp_test && /tmp/dsp_test

#include "dsp/Engine.h"
#include <chrono>
#include <cstdio>
#include <random>

using namespace tt;

static constexpr double SR = 48000.0;
static int failures = 0;

#define CHECK(cond, ...) do { if (cond) { std::printf ("  ok    "); } else { std::printf ("  FAIL  "); ++failures; } std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)

struct Run
{
    Engine e;
    std::vector<float> outL, outR;
    Run() { e.prepare (SR); }

    // Process a whole signal in 256-sample blocks with fixed settings.
    void go (const Settings& s, const std::vector<float>& in, int captureAt = -1)
    {
        outL.assign (in.size(), 0.0f); outR.assign (in.size(), 0.0f);
        const int B = 256;
        for (size_t i = 0; i < in.size(); i += B)
        {
            const int n = (int) std::min ((size_t) B, in.size() - i);
            Settings t = s;
            if (captureAt >= 0) t.loopOn = (int) i >= captureAt;
            e.process (t, in.data() + i, in.data() + i, outL.data() + i, outR.data() + i, n);
        }
    }
};

static std::vector<float> silence (double sec) { return std::vector<float> ((size_t) (sec * SR), 0.0f); }
static void addSine (std::vector<float>& v, double f, double from, double to, float amp)
{
    for (size_t i = (size_t) (from * SR); i < (size_t) (to * SR) && i < v.size(); ++i)
        v[i] += amp * (float) std::sin (2.0 * M_PI * f * (double) i / SR);
}
static float rms (const std::vector<float>& v, double from, double to)
{
    double s = 0; size_t n = 0;
    for (size_t i = (size_t) (from * SR); i < (size_t) (to * SR) && i < v.size(); ++i) { s += (double) v[i] * v[i]; ++n; }
    return n ? (float) std::sqrt (s / (double) n) : 0.0f;
}
// Frequency from zero crossings.
static double freq (const std::vector<float>& v, double from, double to)
{
    int crossings = 0; size_t a = (size_t) (from * SR), b = (size_t) (to * SR);
    for (size_t i = a + 1; i < b; ++i) if (v[i - 1] < 0.0f && v[i] >= 0.0f) ++crossings;
    return crossings / (to - from);
}
// First time the envelope (20 ms RMS) passes a threshold after 'from'.
static double onset (const std::vector<float>& v, double from, float thresh)
{
    const size_t w = (size_t) (0.005 * SR);
    for (size_t i = (size_t) (from * SR); i + w < v.size(); i += 16)
    {
        double s = 0; for (size_t k = 0; k < w; ++k) s += (double) v[i + k] * v[i + k];
        if (std::sqrt (s / (double) w) > thresh) return (double) i / SR;
    }
    return -1;
}

static Settings delayOnly (float seconds)
{
    Settings s;
    s.wetMode = WetMode::delay;
    s.wetTime = std::log (seconds / 0.02f) / std::log (1.5f / 0.02f);
    s.wetModify = 0.0f; s.wetMix = 1.0f; s.wetDrift = 0.0f; s.wetTone = 0.5f;
    s.inToLoop = false;
    return s;
}

int main()
{
    std::printf ("Dry path\n");
    {
        Run r; Settings s; s.wetOn = false; s.trails = false;
        auto in = silence (1.0); addSine (in, 440, 0, 1, 0.5f);
        r.go (s, in);
        double err = 0; for (size_t i = 4800; i < in.size(); ++i) err = std::max (err, (double) std::abs (r.outL[i] - in[i]));
        CHECK (err < 1e-4, "wet off: output equals input (max err %.2g)", err);
    }

    std::printf ("Delay timing and Clock scaling\n");
    {
        for (auto [ratioKnob, steps, expect] : { std::tuple { 2.0f / 3.0f, true, 0.25 },     // 1:1
                                                 std::tuple { 0.0f + (std::log2 (0.5f) + 2.0f) / 3.0f, true, 0.5 },   // 1:2
                                                 std::tuple { 1.0f, true, 0.125 } })          // 2:1
        {
            Run r; Settings s = delayOnly (0.25f); s.clock = ratioKnob; s.clockSteps = steps;
            r.e.reset();
            auto in = silence (1.5); addSine (in, 300, 0.2, 0.25, 0.5f);
            r.go (s, in);
            const double t = onset (r.outL, 0.26, 0.05f) - 0.2;
            CHECK (std::abs (t - expect) < 0.01, "clock ratio %.3f: echo after %.1f ms (want %.0f)", clockRatio (ratioKnob, true), t * 1000, expect * 1000);
        }
    }

    std::printf ("Delay level\n");
    {
        Run r; Settings s = delayOnly (0.2f);
        auto in = silence (1.0); addSine (in, 300, 0.1, 0.25, 0.5f);
        r.go (s, in);
        const float a = rms (in, 0.12, 0.24), b = rms (r.outL, 0.32, 0.44);
        CHECK (std::abs (20 * std::log10 (b / a)) < 1.5f, "echo level %.2f dB vs input", 20 * std::log10 (b / a));
    }

    std::printf ("Looper\n");
    {
        Run r; Settings s; s.wetOn = false; s.trails = false; s.loopMode = LoopMode::tape; s.loopModify = 0.5f;
        s.loopLength = std::log (1.0f / 0.08f) / std::log (8.0f / 0.08f);  // 1 s
        s.loopLevel = 0.8f;
        auto in = silence (5.0); addSine (in, 440, 0, 2, 0.5f);
        r.go (s, in, (int) (2.0 * SR));
        CHECK (rms (r.outL, 2.5, 4.5) > 0.2f, "loop keeps playing after input stops (rms %.3f)", rms (r.outL, 2.5, 4.5));
        CHECK (std::abs (freq (r.outL, 2.5, 4.5) - 440) < 5, "tape 1x pitch %.1f Hz", freq (r.outL, 2.5, 4.5));

        Run r2; s.loopModify = 1.0f;  // 2x
        r2.go (s, in, (int) (2.0 * SR));
        CHECK (std::abs (freq (r2.outL, 2.8, 4.5) - 880) < 10, "tape 2x pitch %.1f Hz", freq (r2.outL, 2.8, 4.5));

        Run r3; s.loopModify = 0.5f; s.clock = (std::log2 (0.5f) + 2.0f) / 3.0f;  // clock 1:2 halves pitch
        r3.go (s, in, (int) (2.0 * SR));
        // input was recorded at 1:2 too, so the loop plays back at the input's pitch; check it is still clean
        CHECK (std::abs (freq (r3.outL, 2.8, 4.5) - 440) < 8, "clock 1:2, recorded and played at 1:2: %.1f Hz", freq (r3.outL, 2.8, 4.5));

        // Capture at 1:1, then drop the clock to 1:2: loop should halve in pitch.
        Run r4; Settings t = s; t.clock = 2.0f / 3.0f;
        std::vector<float> outL (in.size()), outR (in.size());
        for (size_t i = 0; i < in.size(); i += 256)
        {
            Settings u = t; u.loopOn = i >= (size_t) (2.0 * SR);
            if (i >= (size_t) (2.5 * SR)) u.clock = (std::log2 (0.5f) + 2.0f) / 3.0f;
            r4.e.process (u, in.data() + i, in.data() + i, outL.data() + i, outR.data() + i, (int) std::min ((size_t) 256, in.size() - i));
        }
        CHECK (std::abs (freq (outL, 3.0, 4.5) - 220) < 6, "captured at 1:1, played at 1:2: %.1f Hz (want 220)", freq (outL, 3.0, 4.5));

        Run r5; s.clock = 2.0f / 3.0f; s.loopMode = LoopMode::stretch; s.loopModify = 0.0f;  // slowest stretch, pitch kept
        r5.go (s, in, (int) (2.0 * SR));
        CHECK (std::abs (freq (r5.outL, 2.5, 4.5) - 440) < 12, "stretch keeps pitch: %.1f Hz", freq (r5.outL, 2.5, 4.5));

        Run r6; s.loopMode = LoopMode::tape; s.loopModify = 0.5f; s.loopFade = 0.0f;  // halves each pass
        r6.go (s, in, (int) (2.0 * SR));
        const float p1 = rms (r6.outL, 2.1, 2.9), p3 = rms (r6.outL, 4.1, 4.9);
        CHECK (p3 < p1 * 0.3f, "fade 0%%: loop dies away (%.3f -> %.3f)", p1, p3);
    }

    std::printf ("Reverb\n");
    {
        Run r; Settings s; s.wetMode = WetMode::reverb; s.wetMix = 1.0f; s.inToLoop = false; s.wetModify = 0.5f;
        auto in = silence (6.0); addSine (in, 500, 0.0, 0.3, 0.5f);
        r.go (s, in);
        const float early = rms (r.outL, 0.4, 0.6), late = rms (r.outL, 5.0, 5.5);
        CHECK (early > 0.02f && late < early * 0.5f, "tail present and decaying (%.3f -> %.4f)", early, late);
        Run r2;
        r2.outL.assign (in.size(), 0.0f); r2.outR.assign (in.size(), 0.0f);
        for (size_t i = 0; i < in.size(); i += 256)
        {
            Settings t = s; t.freeze = i >= (size_t) (0.35 * SR);
            r2.e.process (t, in.data() + i, in.data() + i, r2.outL.data() + i, r2.outR.data() + i, (int) std::min ((size_t) 256, in.size() - i));
        }
        CHECK (rms (r2.outL, 5.0, 5.5) > rms (r2.outL, 0.4, 0.6) * 0.3f, "freeze holds the tail (%.3f vs %.3f)", rms (r2.outL, 5.0, 5.5), rms (r2.outL, 0.4, 0.6));
    }

    std::printf ("Stability: 60 s of random settings\n");
    {
        Run r; std::mt19937 g (7); std::uniform_real_distribution<float> u (0, 1);
        auto in = silence (60.0); for (auto& x : in) x = (u (g) * 2 - 1) * 0.5f;
        std::vector<float> oL (in.size()), oR (in.size());
        Settings s; float peak = 0; bool finite = true;
        for (size_t i = 0; i < in.size(); i += 256)
        {
            if ((i / 256) % 40 == 0)
            {
                s.clock = u (g); s.clockSteps = u (g) > 0.5f;
                s.wetOn = u (g) > 0.2f; s.wetMode = (WetMode) (int) (u (g) * 2.99f);
                s.wetTime = u (g); s.wetModify = u (g); s.wetMix = u (g); s.wetTone = u (g); s.wetDrift = u (g); s.wetWidth = u (g);
                s.freeze = u (g) > 0.8f; s.trails = u (g) > 0.5f;
                s.loopOn = u (g) > 0.4f; s.loopMode = (LoopMode) (int) (u (g) * 2.99f);
                s.loopLength = u (g); s.loopModify = u (g); s.loopLevel = u (g); s.loopFade = u (g); s.loopTone = u (g); s.loopDirt = u (g);
                s.reverse = u (g) > 0.5f;
                s.inToWet = u (g) > 0.3f; s.inToLoop = u (g) > 0.3f; s.loopToWet = u (g) > 0.5f; s.wetToLoop = u (g) > 0.5f;
                s.sync = u (g) > 0.5f; s.bpm = 60 + u (g) * 120;
            }
            r.e.process (s, in.data() + i, in.data() + i, oL.data() + i, oR.data() + i, 256, u (g) > 0.995f ? 1 : 0);
            for (size_t k = i; k < i + 256; ++k) { finite &= std::isfinite (oL[k]) && std::isfinite (oR[k]); peak = std::max (peak, std::max (std::abs (oL[k]), std::abs (oR[k]))); }
        }
        CHECK (finite, "no NaN or inf");
        CHECK (peak < 3.0f, "peak bounded (%.2f)", peak);
    }

    std::printf ("Clicks on wet mode switches\n");
    {
        Run r; Settings s; s.wetMix = 0.5f; s.inToLoop = false; s.wetModify = 0.4f;
        auto in = silence (4.0); addSine (in, 220, 0, 4, 0.3f);
        std::vector<float> oL (in.size()), oR (in.size());
        float worst = 0, base = 0;
        for (size_t i = 0; i < in.size(); i += 256)
        {
            s.wetMode = (WetMode) ((i / (size_t) (0.5 * SR)) % 3);
            r.e.process (s, in.data() + i, in.data() + i, oL.data() + i, oR.data() + i, 256);
        }
        for (size_t i = 2; i < in.size(); ++i)
        {
            const float d2 = std::abs (oL[i] - 2 * oL[i - 1] + oL[i - 2]);
            if (i < (size_t) (0.45 * SR) && i > (size_t) (0.2 * SR)) base = std::max (base, d2);
            worst = std::max (worst, d2);
        }
        CHECK (worst < base * 4.0f + 0.01f, "worst 2nd difference %.4f vs steady %.4f", worst, base);
    }

    std::printf ("CPU\n");
    {
        Run r; Settings s; s.clock = 1.0f; s.wetMode = WetMode::reverb; s.loopOn = true; s.loopMode = LoopMode::stretch;
        s.loopToWet = true; s.wetToLoop = true;
        auto in = silence (10.0); addSine (in, 330, 0, 10, 0.3f);
        std::vector<float> oL (in.size()), oR (in.size());
        auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < in.size(); i += 256)
            r.e.process (s, in.data() + i, in.data() + i, oL.data() + i, oR.data() + i, 256);
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  info  10 s of audio at Clock 2:1 (worst case) took %.0f ms (%.1f%% of real time)\n", ms, ms / 100.0);
        CHECK (ms < 1500, "fast enough");
    }

    std::printf (failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
