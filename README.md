# Cruz EQ

A visual parametric EQ plugin for FL Studio on Mac (VST3 and AU), aiming for the
feature set and performance of today's top mixing EQs, with its own code and its own look.

This is the first of a set of mixing tools for personal use in FL Studio sessions.

## Getting the plugin

Every change pushed to `main` is built automatically on GitHub's Mac machines and
published under **Releases > Latest build**. See [docs/INSTALL.md](docs/INSTALL.md)
for step-by-step install instructions. Pull requests are built too, and their downloads
appear under the **Artifacts** section of each run on the **Actions** tab.

## Roadmap

1. **Project setup** (done): plugin skeleton, automatic Mac build, install guide.
2. **First playable EQ** (done): 24 zero-latency bands with bell, shelf, tilt,
   cut (6 to 96 dB/oct), notch and band pass shapes, draggable band dots, and a live
   pre/post spectrum analyzer.
3. **Feature parity** (in progress):
   - 3a (this version): per-band stereo placement (left, right, mid, side), slopes for
     shelves, notches and band passes, a flat tilt shape, auto gain and gain scale.
   - 3b: dynamic EQ on every band.
   - 3c: linear phase and natural phase modes.
   - 3d: EQ match and spectral dynamics.
4. **Polish and benchmark**: CPU, latency and sound compared side by side in real sessions.

## Using the EQ

- **Add a band:** double-click anywhere on the display.
- **Move a band:** drag its dot. Left and right is frequency. Up and down is gain, or Q
  for cuts, notches and band passes. Hold Shift for fine moves.
- **Change Q:** scroll over a dot, or use the Q knob.
- **Bypass a band:** Option-click its dot, or tick Bypass.
- **Remove a band:** double-click its dot, or click Remove.
- **More options:** right-click a dot to change its shape, slope or stereo placement.
- **Stereo placement:** pick Stereo, Left, Right, Mid or Side in the band's Stereo box.
  The dot shows L, R, M or S, and the display draws a second curve (with a legend)
  for the other channel.
- **Flat Tilt:** tilts the whole spectrum at a steady dB per octave around the band's
  frequency. Gain sets the total change from 20 Hz to 20 kHz.
- **Auto gain:** keeps the overall level about the same while you boost or cut. The
  amount it applies is shown next to the switch.
- **Gain Scale:** scales every band's gain at once (100% is as set, 0% is flat,
  200% doubles it).

## How the filters work

The zero-latency mode uses "matched" filters: each band's response is computed so it
follows the ideal analog curve all the way up to the top of the spectrum, instead of
bunching up near 20 kHz like many digital EQs. The engine runs in double precision with
smoothed parameter changes. `Tests/DspTests.cpp` checks every shape against its analog
curve, checks stability across the full parameter range, and measures CPU use.

## Building it yourself (optional)

Requires CMake 3.22+ and Xcode. JUCE is downloaded automatically.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Licence note

Cruz EQ is built on [JUCE](https://juce.com), which is free for personal use under
its open-source licence. Check JUCE's licence terms before selling or distributing it.
