# Cruz Sidechain

A sidechain compressor for FL Studio on Mac (VST3 and AU) that shows you what it is doing:
the trigger and the ducked track as scrolling waveforms, a heat map of where their low end
clashes, and curves you can grab and drag.

Install and routing steps are in [INSTALL.md](INSTALL.md).

## The window

**Waveforms**: the trigger (cyan) on top, the ducked track below (violet after ducking,
grey before), and the gain reduction as a pink line hanging down from the top.

**Low-end clash map**: 20 to 285 Hz, six rows per octave, over the last three seconds.
Cyan is the trigger alone, violet is the ducked track alone, and red to yellow to white
means both are loud in the same band at the same time. The bars on the right add up the
clash per band, and the header names the worst note (for example "Most clash: 57 Hz (A#1)").
Switch between **Before ducking** and **After ducking** to see how much the ducking cleared.

**Compression curve**: drag the white point to move the threshold, the right-edge point
to change the ratio, the square on the curve to widen the knee, and the pink square on the
dashed line to set the range (the most it ever turns down). The moving dot is the trigger
level right now.

**Response shape**: one duck over time. Drag the white points to set attack, hold and
release, and the orange squares up or down to bend the attack and release curves. The
lines marked 1/16, 1/8, 1/4 follow FL Studio's tempo so you can fit the release to the
groove. Hold Shift while dragging for fine moves. Double-click any point to reset it.

## Controls

| Control | What it does |
|---------|--------------|
| Trigger | **Sidechain** (another track routed in), **Self** (this track), or **Tempo** (a ghost kick on the grid while FL plays). |
| Rate | Note length of the ghost kick in Tempo mode. |
| Detect | **Peak** reacts to every hit; **RMS** follows the average and feels smoother. |
| Duck lows only | Ducks only below the Split Freq, so the top of the bass or pad stays put. |
| Listen to trigger | Plays the filtered trigger, to hear what the detector hears. |
| Threshold, Ratio, Knee, Range | The compression curve. Ratio goes up to infinity. |
| Attack, Hold, Release | How fast it ducks, how long it stays down, how fast it comes back. |
| Attack Bend, Release Bend | The shape of the attack and release: positive moves fast first, negative starts slow. |
| Lookahead | Delays the track up to 10 ms so the duck starts before the kick lands (adds that much latency, which FL compensates). |
| Trigger Gain, HPF, LPF | Level and filters for the trigger, for example a low-pass so only the kick's thump triggers. |
| Mix, Output | Blend with the unducked sound, and output level. |

Plugin code `Crsc`, manufacturer `Cruz`.
