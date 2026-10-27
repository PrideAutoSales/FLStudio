# Cruz Verb

An algorithmic reverb with a visual decay display, in the spirit of today's best
"draw your reverb" plugins, with its own code and its own look.

## What's in it

- **Space, Decay, Pre-delay**: room size, tail length (0.1 to 20 s) and the gap before the reverb starts.
- **Distance**: close (lots of early reflections) to far away (mostly smooth tail).
- **Character**: clean and still at 0 %, slowly moving and lush at 100 %.
- **Brightness** and **Width** for the overall tone and stereo spread.
- **Decay EQ**: drag the three amber nodes to make lows or highs ring longer or shorter than the mids.
  The line shows the real decay time at every frequency.
- **Post EQ**: low cut, low shelf, high shelf and high cut applied to the reverb only (teal nodes).
- **Ducking**: the reverb gets out of the way while the dry sound plays, and blooms in the gaps.
- **Freeze**: holds the current tail forever.
- Live spectrum of the reverb behind the curves, and ten factory presets.

Zero latency. Double-click a knob or a node to reset it.

## How it works

A 16-line feedback delay network with a Hadamard mixing matrix. Each line has its own
two-shelf decay filter designed so every frequency band hits -60 dB at the time you set,
and lines are slowly modulated (Character) to avoid metallic ringing. Early reflections
come from a multi-tap delay after the pre-delay, and four allpass diffusers smear the input
before it enters the tank.

Install steps are in [INSTALL.md](INSTALL.md).
