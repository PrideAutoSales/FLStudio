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
   - 3a (done): per-band stereo placement (left, right, mid, side), slopes for
     shelves, notches and band passes, a flat tilt shape, auto gain and gain scale.
   - 3b (done): dynamic EQ on every bell, shelf and tilt band.
   - 3c (done): linear phase and natural phase modes.
   - 3d: EQ match and spectral dynamics.
4. **Polish and benchmark**: CPU, latency and sound compared side by side in real sessions.

## Using the EQ

- **Add a band:** click anywhere on the display (keep holding to drag it into place).
  The shape depends on where you click: a low cut at the very bottom (below 30 Hz),
  a low shelf just above it (up to 150 Hz), bells through the middle, a high shelf
  from 5 kHz and a high cut above 14 kHz. A faint preview and label under the mouse
  show what a click will add. Option-click adds a dynamic band.
- **Move a band:** drag its dot. Left and right is frequency. Up and down is gain, or Q
  for cuts, notches and band passes. Hold Shift for fine moves.
- **Change Q:** scroll over a dot, or use the Q knob. Cmd-scroll changes gain.
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
- **Dynamic EQ:** select a bell, shelf or tilt band and drag the small diamond under
  its dot. Dragging it makes the band dynamic, and where you leave it is how far the
  band can move. Option-scroll over a dot does the same, and the Dynamic box and the
  right-click menu still work. By default the **Threshold** is **Auto**: it follows the
  band's own level, so the band reacts to whatever stands out from the rest of the
  sound, with nothing to set. Turn the Threshold knob down from Auto to set a fixed
  level instead. While the sound around the band's frequency is above the threshold,
  the band's gain moves by up to **Range**: a negative range cuts when it gets loud (for harsh notes,
  sibilance or boomy bass), a positive range boosts when it gets loud. **Attack** and
  **Release** set how fast it reacts and recovers. The selected band shows the area it
  can move through, a line from its dot to the end of the range, and a small dot where
  its gain is right now. The band listens to the plugin's input, so the order of the
  bands does not matter.
- **Processing mode:** the box at the top picks how the EQ works.
  - **Zero Latency** (default) adds no delay, so it suits tracking and live playing.
  - **Natural Phase** shapes the sound like an analog EQ, with the same tone in the
    highs as in the lows. It adds a short delay of 4 ms at 48 kHz.
  - **Linear Phase** changes the tone without shifting the timing between frequencies.
    It suits mastering and parallel tracks. The box next to it sets the resolution:
    higher settings keep narrow low bands exact but add more delay (from about 24 ms on
    Low to 344 ms on Maximum at 48 kHz).

  The delay is shown next to the boxes, and FL Studio makes up for it automatically.
  Switching modes mutes the sound for a moment while the new filters are built.

## How the filters work

The zero-latency mode uses "matched" filters: each band's response is computed so it
follows the ideal analog curve all the way up to the top of the spectrum, instead of
bunching up near 20 kHz like many digital EQs. The engine runs in double precision with
smoothed parameter changes. `Tests/DspTests.cpp` checks every shape against its analog
curve, checks stability across the full parameter range, and measures CPU use.

Natural and linear phase turn the whole EQ, including mid/side bands and dynamic bands
as they move, into long FIR filters. A background thread rebuilds them whenever a band
changes. The audio thread applies them with a partitioned FFT convolver that crossfades
to each new set, so moving a band never clicks. The tests check each mode against the
analog curve (linear within 0.25 dB, natural within 0.2 dB and 2 degrees) and check the
convolver against direct convolution.

## Building it yourself (optional)

Requires CMake 3.22+ and Xcode. JUCE is downloaded automatically.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Licence note

Cruz EQ is built on [JUCE](https://juce.com), which is free for personal use under
its open-source licence. Check JUCE's licence terms before selling or distributing it.
