#pragma once

// The Wet channel: Reverb, Delay and Slip.
// All three engines always run; only the selected one is fed input, so switching
// modes lets the old mode's tail ring out instead of cutting off.
// Runs at the Clock's internal rate. Times are converted with the reference
// (host) rate, so a lower Clock stretches everything.

#include "Common.h"
#include "Maps.h"
#include "Settings.h"
#include <array>

namespace tt
{
    //==========================================================================
    struct DelayMode
    {
        void prepare (float ref)
        {
            refRate = ref;
            bufL.allocate ((int) (ref * 2.2f)); bufR.allocate ((int) (ref * 2.2f));
            for (auto* f : { &lpL, &lpR }) f->setCutoff (5500.0f, ref);
            for (auto* f : { &hpL, &hpR }) f->setCutoff (60.0f, ref);
            time.setTime (ref * 0.12f);
            fbSm.setTime (ref * 0.02f);
            wander.prepare (ref, 0.7f, 0xBEEF);
            idleCoeff = 1.0f - std::exp (-1.0f / (0.05f * ref));
            reset();
        }
        void reset()
        {
            bufL.clear(); bufR.clear();
            lpL.clear(); lpR.clear(); hpL.clear(); hpR.clear();
            time.snap (time.target); fbSm.snap (fbSm.target);
        }
        void set (float seconds, float feedback, float drift)
        {
            time.target = std::min (seconds * refRate, (float) bufL.maxDelay() - 64.0f);
            fbSm.target = feedback;
            depth = drift * 0.006f * refRate;
            lfoInc = twoPi * 0.45f / refRate;
        }
        void process (float inL, float inR, bool active, bool freeze, float& outL, float& outR) noexcept
        {
            lfo += lfoInc; if (lfo > twoPi) lfo -= twoPi;
            const float mod = depth * (0.6f * std::sin (lfo) + 0.4f * wander.next());
            const float d = std::max (4.0f, time.next() + mod);
            const float yL = bufL.readHermite (d);
            const float yR = bufR.readHermite (d * 1.015f);

            idle += ((active ? 0.0f : 1.0f) - idle) * idleCoeff;
            float fb = fbSm.next();
            fb = lerp (fb, std::min (fb, 0.85f), idle);
            const bool hold = active && freeze;
            if (hold) { fb = 1.0f; inL = inR = 0.0f; }

            float fL = 0.8f * yL + 0.2f * yR;
            float fR = 0.8f * yR + 0.2f * yL;
            if (! hold) { fL = hpL.highpass (lpL.lowpass (fL)); fR = hpR.highpass (lpR.lowpass (fR)); }

            bufL.push (softClip (inL + fb * fL));
            bufR.push (softClip (inR + fb * fR));
            outL = yL; outR = yR;
        }

        float refRate = 48000.0f;
        DelayBuffer bufL, bufR;
        OnePole lpL, lpR, hpL, hpR;
        Smoother time, fbSm;
        Wander wander;
        float depth = 0.0f, lfo = 0.0f, lfoInc = 0.0f, idle = 0.0f, idleCoeff = 0.001f;
    };

    //==========================================================================
    // 8-line feedback delay network with Hadamard mixing and modulated lines.
    struct ReverbMode
    {
        static constexpr int N = 8;

        void prepare (float ref)
        {
            refRate = ref;
            const float k = ref / 48000.0f;
            static constexpr int base[N] = { 1031, 1327, 1523, 1871, 2053, 2381, 2729, 3001 };
            for (int i = 0; i < N; ++i)
            {
                baseLen[i] = (float) base[i] * k;
                lines[i].allocate ((int) (baseLen[i] * 1.7f) + 256);
                damp[i].setCutoff (8500.0f, ref);
                lfoInc[i] = twoPi * (0.11f + 0.07f * (float) i) / ref;
                lfo[i] = (float) i * 0.8f;
            }
            static constexpr int ap[4] = { 142, 107, 379, 277 };
            for (int i = 0; i < 4; ++i) { difL[i].allocate ((int) (ap[i] * k)); difR[i].allocate ((int) (ap[i] * k * 1.07f)); }
            size.setTime (ref * 0.25f);
            reset();
        }
        void reset()
        {
            for (auto& l : lines) l.clear();
            for (auto& d : damp)  d.clear();
            for (auto& a : difL)  a.clear();
            for (auto& a : difR)  a.clear();
            size.snap (size.target);
        }
        void set (float sizeFactor, float decaySeconds, float drift)
        {
            size.target = sizeFactor;
            depth = drift * 14.0f * refRate / 48000.0f;
            const float idleRt = std::min (decaySeconds, 3.0f);
            for (int i = 0; i < N; ++i)
            {
                const float len = baseLen[i] * sizeFactor;
                gainActive[i] = std::pow (10.0f, -3.0f * len / (decaySeconds * refRate));
                gainIdle[i]   = std::pow (10.0f, -3.0f * len / (idleRt * refRate));
            }
        }
        void process (float inL, float inR, bool active, bool freeze, float& outL, float& outR) noexcept
        {
            const bool hold = active && freeze;
            const float g = hold ? 0.0f : 1.0f;
            float xL = inL * g, xR = inR * g;
            for (auto& a : difL) xL = a.process (xL, 0.6f);
            for (auto& a : difR) xR = a.process (xR, 0.6f);

            const float s = size.next();
            const float* gains = active ? gainActive : gainIdle;
            float y[N];
            for (int i = 0; i < N; ++i)
            {
                lfo[i] += lfoInc[i]; if (lfo[i] > twoPi) lfo[i] -= twoPi;
                const float len = baseLen[i] * s;
                y[i] = lines[i].readHermite (len + depth * std::sin (lfo[i]));
                if (! hold)
                    y[i] = damp[i].lowpass (y[i]) * gains[i];
            }

            outL = 0.35f * (y[0] - y[2] + y[4] - y[6]);
            outR = 0.35f * (y[1] - y[3] + y[5] - y[7]);

            // Fast Walsh-Hadamard, normalised.
            for (int h = 1; h < N; h <<= 1)
                for (int i = 0; i < N; i += h << 1)
                    for (int j = i; j < i + h; ++j)
                    {
                        const float a = y[j], b = y[j + h];
                        y[j] = a + b; y[j + h] = a - b;
                    }
            const float norm = 0.35355339f;
            for (int i = 0; i < N; ++i)
                lines[i].push (y[i] * norm + ((i & 1) ? xR : xL) * 0.5f);
        }

        float refRate = 48000.0f;
        std::array<DelayBuffer, N> lines;
        std::array<OnePole, N> damp;
        std::array<Allpass, 4> difL, difR;
        float baseLen[N] {}, lfo[N] {}, lfoInc[N] {}, gainActive[N] {}, gainIdle[N] {};
        Smoother size;
        float depth = 0.0f;
    };

    //==========================================================================
    // Two-head crossfading pitch shifter (classic delay-line method).
    struct PitchShifter
    {
        void prepare (float ref)
        {
            window = 0.06f * ref;
            buf.allocate ((int) (window * 1.2f) + 16);
            reset();
        }
        void reset() { buf.clear(); phase = 0.0f; }
        float process (float x, float ratio) noexcept
        {
            buf.push (x);
            phase += (1.0f - ratio) / window;
            phase -= std::floor (phase);
            const float p2 = phase + 0.5f - std::floor (phase + 0.5f);
            const float a = buf.readHermite (2.0f + phase * window);
            const float b = buf.readHermite (2.0f + p2 * window);
            return a * std::sin (pi * phase) + b * std::sin (pi * p2);
        }
        DelayBuffer buf;
        float window = 2880.0f, phase = 0.0f;
    };

    // Slip: a delay whose repeats get re-pitched every pass, so they climb or fall.
    struct SlipMode
    {
        void prepare (float ref)
        {
            refRate = ref;
            bufL.allocate ((int) (ref * 2.2f)); bufR.allocate ((int) (ref * 2.2f));
            psL.prepare (ref); psR.prepare (ref);
            for (auto* f : { &lpL, &lpR }) f->setCutoff (7000.0f, ref);
            for (auto* f : { &hpL, &hpR }) f->setCutoff (90.0f, ref);
            time.setTime (ref * 0.12f);
            semis.setTime (ref * 0.05f);
            fbSm.setTime (ref * 0.05f); fbSm.snap (0.62f);
            wander.prepare (ref, 0.5f, 0xC0FFEE);
            reset();
        }
        void reset()
        {
            bufL.clear(); bufR.clear(); psL.reset(); psR.reset();
            lpL.clear(); lpR.clear(); hpL.clear(); hpR.clear();
            time.snap (time.target); semis.snap (semis.target);
        }
        void set (float seconds, float intervalSemis, float drift)
        {
            time.target = std::min (seconds * refRate, (float) bufL.maxDelay() - 64.0f);
            semis.target = intervalSemis;
            driftAmt = drift;
        }
        void process (float inL, float inR, bool active, bool freeze, float& outL, float& outR) noexcept
        {
            const float w = wander.next();
            const float d = std::max (4.0f, time.next() * (1.0f + 0.01f * driftAmt * w));
            const float yL = bufL.readHermite (d);
            const float yR = bufR.readHermite (d * 1.02f);

            const float st = semis.next() + driftAmt * 0.15f * w;
            const float ratio = std::pow (2.0f, st / 12.0f);
            const float sL = psL.process (yL, ratio);
            const float sR = psR.process (yR, ratio * 1.002f);

            const bool hold = active && freeze;
            fbSm.target = hold ? 0.92f : (active ? 0.62f : 0.5f);
            const float fb = fbSm.next();
            const float g = hold ? 0.0f : 1.0f;

            bufL.push (softClip (g * inL + fb * hpL.highpass (lpL.lowpass (sL))));
            bufR.push (softClip (g * inR + fb * hpR.highpass (lpR.lowpass (sR))));
            outL = 0.6f * yL + 0.6f * sL;
            outR = 0.6f * yR + 0.6f * sR;
        }

        float refRate = 48000.0f;
        DelayBuffer bufL, bufR;
        PitchShifter psL, psR;
        OnePole lpL, lpR, hpL, hpR;
        Smoother time, semis, fbSm;
        Wander wander;
        float driftAmt = 0.0f;
    };

    //==========================================================================
    struct WetEngine
    {
        void prepare (float ref)
        {
            refRate = ref;
            delay.prepare (ref); reverb.prepare (ref); slip.prepare (ref);
            tiltL.prepare (ref); tiltR.prepare (ref);
            inGate.setTime (ref * 0.01f);
            outGate.setTime (ref * 0.02f);
            width.setTime (ref * 0.02f);
            for (auto* a : { &actReverb, &actDelay, &actSlip }) a->setTime (ref * 0.012f);
            reset();
        }
        void reset()
        {
            delay.reset(); reverb.reset(); slip.reset();
            tiltL.clear(); tiltR.clear();
            inGate.snap (inGate.target); outGate.snap (outGate.target); width.snap (width.target);
            for (auto* a : { &actReverb, &actDelay, &actSlip }) a->snap (a->target);
        }

        // Jump every glide to its target (first block after a reset, so nothing sweeps in).
        void snap()
        {
            delay.time.snap (delay.time.target); delay.fbSm.snap (delay.fbSm.target);
            reverb.size.snap (reverb.size.target);
            slip.time.snap (slip.time.target); slip.semis.snap (slip.semis.target);
            inGate.snap (inGate.target); outGate.snap (outGate.target); width.snap (width.target);
            for (auto* a : { &actReverb, &actDelay, &actSlip }) a->snap (a->target);
        }

        void set (const Settings& s)
        {
            mode   = s.wetMode;
            actReverb.target = mode == WetMode::reverb ? 1.0f : 0.0f;
            actDelay.target  = mode == WetMode::delay  ? 1.0f : 0.0f;
            actSlip.target   = mode == WetMode::slip   ? 1.0f : 0.0f;
            freeze = s.freeze && s.wetOn;
            inGate.target  = s.wetOn ? 1.0f : 0.0f;
            outGate.target = (s.wetOn || s.trails) ? 1.0f : 0.0f;
            width.target   = s.wetWidth * 2.0f;
            tiltL.setTone (s.wetTone); tiltR.setTone (s.wetTone);

            delay.set (maps::syncedSeconds (maps::delaySeconds (s.wetTime), s.sync, s.bpm, 2.0f),
                       maps::delayFeedback (s.wetModify), s.wetDrift);
            reverb.set (maps::reverbSize (s.wetTime), maps::reverbDecaySeconds (s.wetModify), s.wetDrift);
            slip.set (maps::syncedSeconds (maps::slipSeconds (s.wetTime), s.sync, s.bpm, 2.0f),
                      maps::slipSemis[maps::slipIndex (s.wetModify)], s.wetDrift);
        }

        void process (float inL, float inR, float& outL, float& outR) noexcept
        {
            const float gi = inGate.next();
            inL *= gi; inR *= gi;

            const float ar = actReverb.next(), ad = actDelay.next(), as = actSlip.next();
            float rL, rR, dL, dR, sL, sR;
            reverb.process (inL * ar, inR * ar, mode == WetMode::reverb, freeze, rL, rR);
            delay.process  (inL * ad, inR * ad, mode == WetMode::delay,  freeze, dL, dR);
            slip.process   (inL * as, inR * as, mode == WetMode::slip,   freeze, sL, sR);

            float l = tiltL.process (rL + dL + sL);
            float r = tiltR.process (rR + dR + sR);

            const float mid = 0.5f * (l + r), side = 0.5f * (r - l) * width.next();
            const float go = outGate.next();
            outL = (mid - side) * go;
            outR = (mid + side) * go;
        }

        float refRate = 48000.0f;
        WetMode mode = WetMode::reverb;
        bool freeze = false;
        DelayMode delay; ReverbMode reverb; SlipMode slip;
        Tilt tiltL, tiltR;
        Smoother inGate, outGate, width, actReverb, actDelay, actSlip;
    };
}
