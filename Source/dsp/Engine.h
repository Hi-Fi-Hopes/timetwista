#pragma once

// TimeTwista's top level: the Clock resampler wrapped around the Wet channel and
// the micro-looper, plus the routing between them and the dry/wet mix.
//
//   host in --(anti-alias)--> [ internal rate = host x ratio:  looper <-> wet ] --(smoothing)--> + dry --> out
//
// The filters around the internal domain are deliberately loose: a low Clock is
// meant to sound grainy and aliased.

#include "Common.h"
#include "Clock.h"
#include "Looper.h"
#include "Settings.h"
#include "WetEngine.h"

namespace tt
{
    struct Engine
    {
        void prepare (double hostSampleRate)
        {
            host = (float) hostSampleRate;
            wet.prepare (host);
            looper.prepare (host);
            dryGain.setTime (host * 0.02f);
            for (auto* s : { &wetGain, &gInToWet, &gInToLoop, &gLoopToWet, &gWetToLoop, &gLoopDirect })
                s->setTime (host * 0.015f);
            reset();
        }

        void reset()
        {
            wet.reset(); looper.reset();
            phase = 0.0f; prevL = prevR = 0.0f; yPrevL = yPrevR = yCurL = yCurR = 0.0f;
            for (auto* f : { &aaInL, &aaInR, &aaOutL, &aaOutR }) f->clear();
            logRatio = std::log2 (clockRatio (lastSettings.clock, lastSettings.clockSteps));
            prevLoopOn = false;
            fresh = true;
        }

        // captureRequests > 0 means the Capture button was pressed since the last block.
        void process (const Settings& s, const float* inL, const float* inR, float* outL, float* outR,
                      int numSamples, int captureRequests = 0)
        {
            lastSettings = s;
            wet.set (s);
            looper.set (s);

            if ((s.loopOn && ! prevLoopOn) || (captureRequests > 0 && s.loopOn))
                looper.capture();
            prevLoopOn = s.loopOn;

            // Clock target, smoothed in octaves. Smooth mode glides like a tape motor.
            const float targetLog = std::log2 (clockRatio (s.clock, s.clockSteps));
            const float glide = s.clockSteps ? 0.04f : 0.25f;
            const float glideCoeff = 1.0f - std::exp (-1.0f / (glide * host));

            const float mix = clamp01 (s.wetMix);
            dryGain.target   = s.wetOn ? (mix <= 0.5f ? 1.0f : 1.0f - (mix - 0.5f) * 2.0f) : 1.0f;
            wetGain.target   = s.loopToWet ? 1.0f : mix;
            gInToWet.target  = s.inToWet ? 1.0f : 0.0f;
            gInToLoop.target = s.inToLoop ? 1.0f : 0.0f;
            gLoopToWet.target = s.loopToWet ? 1.0f : 0.0f;
            gWetToLoop.target = s.wetToLoop ? 1.0f : 0.0f;
            gLoopDirect.target = s.loopToWet ? 0.0f : 1.0f;

            if (fresh)   // first block after a reset: start every glide at its destination
            {
                fresh = false;
                wet.snap(); looper.snap();
                for (auto* g : { &dryGain, &wetGain, &gInToWet, &gInToLoop, &gLoopToWet, &gWetToLoop, &gLoopDirect })
                    g->snap (g->target);
                logRatio = targetLog;
            }

            for (int n = 0; n < numSamples; ++n)
            {
                logRatio += (targetLog - logRatio) * glideCoeff;
                const float r = std::pow (2.0f, logRatio);

                if ((n & 31) == 0) updateFilters (r);

                float xL = inL[n], xR = inR[n];
                if (r < 0.999f) { xL = aaInL.process (xL); xR = aaInR.process (xR); }

                phase += r;
                while (phase >= 1.0f)
                {
                    phase -= 1.0f;
                    const float frac = 1.0f - phase / r;   // where this tick sits between the last two host samples
                    float yL, yR;
                    tick (lerp (prevL, xL, frac), lerp (prevR, xR, frac), yL, yR);
                    yPrevL = yCurL; yPrevR = yCurR;
                    yCurL = yL; yCurR = yR;
                }
                prevL = xL; prevR = xR;

                float oL = lerp (yPrevL, yCurL, phase);
                float oR = lerp (yPrevR, yCurR, phase);
                if (r < 0.999f) { oL = aaOutL.process (oL); oR = aaOutR.process (oR); }

                const float dg = dryGain.next();
                outL[n] = inL[n] * dg + oL;
                outR[n] = inR[n] * dg + oR;
            }

            // Safety: if anything ever goes non-finite, start clean rather than blast the speakers.
            for (int n = 0; n < numSamples; ++n)
                if (! std::isfinite (outL[n]) || ! std::isfinite (outR[n]))
                {
                    reset();
                    for (int k = 0; k < numSamples; ++k) { outL[k] = inL[k]; outR[k] = inR[k]; }
                    break;
                }
        }

        float currentRatio() const noexcept { return std::pow (2.0f, logRatio); }

        Looper looper;
        WetEngine wet;

    private:
        void updateFilters (float r)
        {
            const float c = std::min (r, 1.0f) * host * 0.45f;
            aaInL.setCutoff (c, host);  aaInR.setCutoff (c, host);
            aaOutL.setCutoff (c * 1.1f, host); aaOutR.setCutoff (c * 1.1f, host);
        }

        void tick (float inL, float inR, float& outL, float& outR) noexcept
        {
            float lL, lR;
            looper.process (std::max (std::abs (inL), std::abs (inR)), lL, lR);

            const float iw = gInToWet.next(), lw = gLoopToWet.next();
            float wL, wR;
            wet.process (inL * iw + lL * lw, inR * iw + lR * lw, wL, wR);

            const float il = gInToLoop.next(), wl = gWetToLoop.next();
            looper.record (inL * il + wL * wl, inR * il + wR * wl);

            const float wg = wetGain.next(), ld = gLoopDirect.next();
            outL = softClip (wL * wg + lL * ld);
            outR = softClip (wR * wg + lR * ld);
        }

        float host = 48000.0f;
        float phase = 0.0f, logRatio = 0.0f;
        float prevL = 0, prevR = 0, yPrevL = 0, yPrevR = 0, yCurL = 0, yCurR = 0;
        SVFLowpass aaInL, aaInR, aaOutL, aaOutR;
        Smoother dryGain, wetGain, gInToWet, gInToLoop, gLoopToWet, gWetToLoop, gLoopDirect;
        Settings lastSettings;
        bool prevLoopOn = false, fresh = true;
    };
}
