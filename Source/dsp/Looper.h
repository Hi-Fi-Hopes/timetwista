#pragma once

// The micro-looper. It is always listening: a ring buffer records continuously.
// Turning it on (or pressing Capture) grabs the last Length of audio and loops it.
// Modes: Env (loop level follows your playing), Tape (varispeed), Stretch (granular,
// pitch stays put). Runs at the Clock's internal rate.

#include "Common.h"
#include "Maps.h"
#include "Settings.h"
#include <array>

namespace tt
{
    struct Looper
    {
        static constexpr int numBars = 48;
        static constexpr float maxSeconds = 16.0f;

        void prepare (float ref)
        {
            refRate = ref;
            cap = (int) (ref * (maxSeconds + 0.5f));
            ringL.assign ((size_t) cap, 0.0f); ringR.assign ((size_t) cap, 0.0f);
            loopL.assign ((size_t) cap, 0.0f); loopR.assign ((size_t) cap, 0.0f);
            outGain.setTime (ref * 0.008f);
            speed.setTime (ref * 0.15f);
            levelSm.setTime (ref * 0.02f);
            env.setTime (ref * 0.18f); envRelease = env.coeff;
            env.setTime (ref * 0.004f); envAttack = env.coeff;
            tiltL.prepare (ref); tiltR.prepare (ref);
            reset();
        }

        void reset()
        {
            std::fill (ringL.begin(), ringL.end(), 0.0f); std::fill (ringR.begin(), ringR.end(), 0.0f);
            ringW = 0; ringFilled = 0;
            playing = false; loopLen = 0; pos = 0.0;
            outGain.snap (0.0f); speed.snap (1.0f); levelSm.snap (levelSm.target); env.snap (0.0f);
            tiltL.clear(); tiltR.clear();
            loopPeaks.fill (0.0f); listenBars.fill (0.0f); listenIdx = 0; barCount = 0; barPeak = 0.0f;
            voices[0] = voices[1] = {};
        }

        void set (const Settings& s)
        {
            mode = s.loopMode;
            modify = s.loopModify;
            reverse = s.reverse;
            levelSm.target = s.loopLevel * 1.25f;
            fade = maps::fadePerPass (s.loopFade);
            tiltL.setTone (s.loopTone); tiltR.setTone (s.loopTone);
            dirt = s.loopDirt;
            const float sec = maps::syncedLoopSeconds (maps::loopSeconds (s.loopLength), s.sync, s.bpm, maxSeconds);
            lengthSamples = std::max (64, std::min (cap - 2048, (int) (sec * refRate)));
            enabled = s.loopOn;
            outGain.target = enabled && playing ? 1.0f : 0.0f;
        }

        // Grab the last Length of the ring buffer and start looping it.
        void capture()
        {
            const int x = std::max (16, std::min ((int) (0.012f * refRate), lengthSamples / 4));
            const int n = std::min (lengthSamples, ringFilled - x);
            if (n < 64) return;

            const int start = ((ringW - (n + x)) % cap + cap) % cap;
            auto c = [&] (const std::vector<float>& r, int k) { return r[(size_t) ((start + k) % cap)]; };

            for (int j = 0; j < n; ++j)
            {
                float l = c (ringL, x + j), r = c (ringR, x + j);
                if (j >= n - x)   // bake a crossfade into the loop point so it never clicks
                {
                    const float w = (float) (j - (n - x)) / (float) x;
                    const float a = std::cos (w * pi * 0.5f), b = std::sin (w * pi * 0.5f);
                    l = l * a + c (ringL, j - n + x) * b;
                    r = r * a + c (ringR, j - n + x) * b;
                }
                loopL[(size_t) j] = l; loopR[(size_t) j] = r;
            }

            loopLen = n;
            pos = stretchPos = reverse ? (double) (n - 1) : 0.0;
            passGain = 1.0f;
            playing = true;
            outGain.target = 1.0f;
            grainCounter = 0;
            voices[0] = voices[1] = {};

            // Waveform for the display
            for (int b = 0; b < numBars; ++b)
            {
                const int i0 = (int) ((int64_t) b * n / numBars), i1 = (int) ((int64_t) (b + 1) * n / numBars);
                float p = 0.0f;
                for (int i = i0; i < i1; ++i) p = std::max (p, std::max (std::abs (loopL[(size_t) i]), std::abs (loopR[(size_t) i])));
                loopPeaks[(size_t) b] = p;
            }
        }

        // One internal tick. envIn is the player's input level, for Env mode.
        void process (float envIn, float& outL, float& outR) noexcept
        {
            env.target = envIn;
            env.coeff = envIn > env.value ? envAttack : envRelease;
            const float e = env.next();

            const float g = outGain.next();
            if (! playing || g < 1.0e-5f)
            {
                outL = outR = 0.0f;
                if (! enabled && g < 1.0e-5f) playing = false;
                return;
            }

            const float dir = reverse ? -1.0f : 1.0f;
            float l = 0.0f, r = 0.0f, modGain = 1.0f;

            if (mode == LoopMode::stretch)
            {
                processGrains (dir, l, r);
            }
            else
            {
                speed.target = mode == LoopMode::tape ? maps::tapeSpeed (modify) : 1.0f;
                pos += (double) (speed.next() * dir);
                wrap (pos);
                readLoop (pos, l, r);

                if (mode == LoopMode::env)
                {
                    const float amt = maps::envAmount (modify);
                    const float lvl = std::min (1.0f, e * 5.0f);
                    modGain = amt >= 0.0f ? (1.0f - amt) + amt * lvl : 1.0f + amt * lvl;
                }
            }

            l = tiltL.process (l); r = tiltR.process (r);
            if (dirt > 0.001f) { l = crush (l); r = crush (r); }

            const float k = g * passGain * modGain * levelSm.next();
            outL = l * k; outR = r * k;
        }

        // Always listening: every tick goes into the ring buffer.
        void record (float l, float r) noexcept
        {
            ringL[(size_t) ringW] = l; ringR[(size_t) ringW] = r;
            if (++ringW == cap) ringW = 0;
            if (ringFilled < cap) ++ringFilled;

            barPeak = std::max (barPeak, std::max (std::abs (l), std::abs (r)));
            if (++barCount >= std::max (1, lengthSamples / numBars))
            {
                listenBars[(size_t) listenIdx] = barPeak;
                listenIdx = (listenIdx + 1) % numBars;
                barCount = 0; barPeak = 0.0f;
            }
        }

        // For the UI: 48 bar heights (0..1) and the playhead (0..1, or -1 when listening).
        void getDisplay (float* bars, float& playhead) const
        {
            if (playing && loopLen > 0)
            {
                for (int i = 0; i < numBars; ++i) bars[i] = loopPeaks[(size_t) i];
                const double p = mode == LoopMode::stretch ? stretchPos : pos;
                playhead = (float) (p / (double) loopLen);
            }
            else
            {
                for (int i = 0; i < numBars; ++i) bars[i] = listenBars[(size_t) ((listenIdx + i) % numBars)];
                playhead = -1.0f;
            }
        }

        bool isPlaying() const noexcept { return playing; }
        void snap() { levelSm.snap (levelSm.target); speed.snap (speed.target); }

    private:
        struct Voice { double readPos = 0.0; int age = 0, len = 0; bool active = false; };

        void wrap (double& p) noexcept
        {
            const double n = (double) loopLen;
            if (p >= n)       { p -= n; passGain *= fade; }
            else if (p < 0.0) { p += n; passGain *= fade; }
            if (p < 0.0 || p >= n) p = 0.0;   // guard against huge jumps
        }

        void readLoop (double p, float& l, float& r) const noexcept
        {
            const int i0 = (int) p;
            const float f = (float) (p - (double) i0);
            const int i1 = (i0 + 1 >= loopLen) ? 0 : i0 + 1;
            l = lerp (loopL[(size_t) i0], loopL[(size_t) i1], f);
            r = lerp (loopR[(size_t) i0], loopR[(size_t) i1], f);
        }

        void processGrains (float dir, float& l, float& r) noexcept
        {
            const int grainLen = std::max (64, std::min ((int) (0.09f * refRate), loopLen / 2));
            const int hop = grainLen / 2;

            stretchPos += (double) (maps::stretchRate (modify) * dir);
            wrap (stretchPos);

            if (grainCounter <= 0)
            {
                grainCounter = hop;
                auto& v = voices[nextVoice]; nextVoice ^= 1;
                double start = stretchPos + (double) (rng.next() * 0.004f * refRate);
                wrap (start);
                v = { start, 0, grainLen, true };
            }
            --grainCounter;

            l = r = 0.0f;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                const float ph = (float) v.age / (float) v.len;
                const float s = std::sin (pi * ph);
                float vl, vr;
                readLoop (v.readPos, vl, vr);
                l += vl * s * s; r += vr * s * s;
                v.readPos += (double) dir;
                if (v.readPos >= (double) loopLen) v.readPos -= (double) loopLen;
                if (v.readPos < 0.0) v.readPos += (double) loopLen;
                if (++v.age >= v.len) v.active = false;
            }
        }

        float crush (float x) const noexcept
        {
            const float drive = 1.0f + dirt * 6.0f;
            x = std::tanh (x * drive) / std::tanh (drive);
            if (dirt > 0.3f)
            {
                const float levels = std::pow (2.0f, 12.0f - (dirt - 0.3f) / 0.7f * 8.0f);
                x = std::round (x * levels) / levels;
            }
            return x;
        }

        float refRate = 48000.0f;
        int cap = 0;
        std::vector<float> ringL, ringR, loopL, loopR;
        int ringW = 0, ringFilled = 0;

        LoopMode mode = LoopMode::tape;
        float modify = 0.5f, fade = 1.0f, dirt = 0.0f, passGain = 1.0f, envAttack = 1.0f, envRelease = 1.0f;
        bool reverse = false, enabled = false, playing = false;
        int lengthSamples = 48000, loopLen = 0;
        double pos = 0.0, stretchPos = 0.0;

        Smoother outGain, speed, levelSm, env;
        Tilt tiltL, tiltR;
        Rng rng;

        Voice voices[2];
        int nextVoice = 0, grainCounter = 0;

        std::array<float, numBars> loopPeaks {}, listenBars {};
        int listenIdx = 0, barCount = 0;
        float barPeak = 0.0f;
    };
}
