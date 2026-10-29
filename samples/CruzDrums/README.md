# Cruz Drums

An original drum sample pack, rendered entirely by code in this folder: 576 one-shots
(House, Hip Hop, Trap, Boom Bap, Pop, Disco, Funk, Drum and Bass; kicks, snares, claps,
rims, hats and percs, 12 each), 24-bit 48 kHz WAV. No recorded audio is used.

Version 2 aims for the textured, hard-knocking sound of top hip hop kits rather than
drum machine tones: organic layers pushed through console-style clipping.

- `drumsynth.py`: oscillators, filters, sampler crush, generated rooms, and the finishing
  pass (DC removal, trim, fades, -0.3 dBTP normalize).
- `organic.py`: modelled real-world sources: drum heads (membrane modes), snare wires,
  cymbal plates, hand claps, finger snaps, tables, boxes, glass, cans, keys, paper,
  shaker beads and beatbox mouth sounds.
- `process.py`: the knock chain: console clipping, distort-EQ-distort, compression,
  transient shaping, small rooms, tape, pitch-down resampling and grit.
- `instruments.py`: the six drum voices, each a set of layered recipes.
- `genres.py`: each genre as recipe weights and processing ranges.
- `generate.py`: renders 22 candidates per folder, rejects any that fail the checks
  (clipping, DC, clicks at either end, stray noise late in the tail, weak or misplaced
  low end, dull hats), and keeps
  the 12 most distinct.

The WAVs are not committed. `build-samples.yml` renders the pack on every change and
publishes `Cruz-Drums.zip` to the `samples` release. To render locally:
`pip install numpy scipy soundfile && python3 generate.py "Cruz Drums"`.
