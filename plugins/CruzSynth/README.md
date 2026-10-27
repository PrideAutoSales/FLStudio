# Cruz Synth

A wavetable synthesizer instrument (VST3 + AU) for FL Studio on Mac.

- **Two wavetable oscillators** with up to 16-voice unison (detune, blend, width), phase/random phase,
  and 11 warp modes: Sync, Bend +/-, Asym, PWM, Mirror, Flip, Quantize, and FM/AM/RM from the other oscillator.
  Each frame is stored band-limited per octave, so high notes stay clean.
- **14 original factory wavetables**, generated in code. Drop any WAV on the wave display to load your own.
- **Wavetable editor**: draw frames, edit harmonics, add/delete frames, crossfade or spectral fill, normalize,
  smooth, invert, reverse, import and export WAV.
- **Sub and noise oscillators** (5 noise types with a color control).
- **Filter** with 12 types: Low 12/24, Ladder, High 12/24, Band 12/24, Notch, Peak, Comb +/-, Vowel, plus drive, mix and keytracking.
- **Modulation**: 3 envelopes (Env 1 is the amp), 4 LFOs with drawable shapes, tempo sync and Free/Retrig/Envelope modes,
  4 macros, velocity, note, mod wheel, aftertouch, pitch bend and per-note random.
  Drag any source onto a knob; 16 routings shown on the MATRIX page.
- **FX rack** in any order: Distortion (6 modes), Chorus, Phaser, Delay (sync, ping-pong), Reverb,
  Compressor (single or 3-band with upward compression), EQ.
- Mono/legato/glide, up to 16 voices, 12 factory presets, user presets saved to `~/Music/Cruz Synth Presets`.

Install steps: [INSTALL.md](INSTALL.md).
