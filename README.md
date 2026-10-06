# TimeTwista mk1

A two-channel ambient plugin (VST3, Windows) for Ableton Live, by Hi-Fi Hopes.

One side is a **Wet** channel: Reverb, Delay or Slip.
The other side is an always-listening **micro-looper**: Env, Tape or Stretch.

A shared **Clock** runs both sides at a different internal sample rate, so one knob changes their speed, pitch and grit together.
The two sides can feed each other, and an **X/Y pad** bends both of them at once.

## Controls

| Section | Control | What it does |
|---|---|---|
| Clock | Clock | Internal rate from 1:4 to 2:1. Lower is slower, lower in pitch and grainier. |
| | Steps / Smooth | Steps snaps to musical ratios. Smooth sweeps freely, with a tape-motor glide. |
| Wet | Reverb / Delay / Slip | The mode. Tails from the old mode keep ringing when you switch. |
| | Time | Reverb: size. Delay and Slip: delay time. |
| | Modify | Reverb: decay. Delay: feedback, which self-oscillates near the top. Slip: the interval each repeat is re-pitched by (down an octave up to up an octave). |
| | Mix | Up to 50%, the wet signal is added and the dry stays full. Above 50%, the dry fades out. |
| | Tone | Tilt EQ, dark to bright. |
| | Drift | Wow and flutter on the wet signal. |
| | Width | Stereo width, from mono to extra wide. |
| | Freeze | Holds the current wet sound in place. |
| | Trails | When on, switching the Wet channel off lets its tail ring out. |
| Looper | Switch / Capture | Grabs the last *Length* of audio and loops it. Capture re-grabs a new loop. |
| | Env / Tape / Stretch | Env: loop level follows your playing (ducks below noon, swells above). Tape: varispeed from 0.25x to 2x. Stretch: granular speed change with the pitch kept. |
| | Length | Loop length for the next capture. |
| | Level | Loop volume. |
| | Fade | How much the loop fades on each pass. 100% holds it. |
| | Tone | Tilt EQ on the loop. |
| | Dirt | Saturation, plus bit-crush in the top 70% of the knob. |
| | Reverse | Plays the loop backwards. |
| Routing | In → Wet, In → Looper, Looper → Wet, Wet → Looper | What feeds what. With Looper → Wet on, you hear the loop only through the Wet channel, at full wet level. |
| Header | Sync | Snaps delay times and loop lengths to the host tempo. |
| X/Y | X / Y | X is Wet Modify, Y is Looper Modify. |

Double-click any knob to reset it. Hover over a knob to see its value. The window can be resized from the corner.

## Building

GitHub Actions builds the Windows VST3 on every push to `main`.
Get the finished plugin from either place:

- the `builds` branch: `TimeTwista-VST3-Windows.zip`
- the Actions tab: open the run, then **Artifacts**

The DSP lives in plain C++ headers in `Source/dsp/`. Test it with:

```
g++ -O2 -std=c++17 -I Source tests/dsp_test.cpp -o dsp_test && ./dsp_test
```

## Fonts

The UI uses Shrikhand, Space Grotesk and JetBrains Mono, all under the SIL Open Font License. The license texts are in `Resources/`.
