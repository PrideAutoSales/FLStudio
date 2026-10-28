# Cruz Drums

An original drum sample pack, rendered entirely by code in this folder: 576 one-shots
(House, Hip Hop, Trap, Boom Bap, Pop, Disco, Funk, Drum and Bass; kicks, snares, claps,
rims, hats and percs, 12 each), 24-bit 48 kHz WAV. No recorded audio is used.

- `drumsynth.py`: oscillators, filtered noise, modal resonators, saturation, sampler crush,
  generated rooms, and the finishing pass (DC removal, trim, fades, -0.3 dBTP normalize).
- `instruments.py`: the six drum voices.
- `genres.py`: each genre's sound as parameter ranges.
- `generate.py`: renders 22 candidates per folder, rejects any that fail the checks
  (clipping, DC, clicks at either end, weak or misplaced low end, dull hats), and keeps
  the 12 most distinct.

The WAVs are not committed. `build-samples.yml` renders the pack on every change and
publishes `Cruz-Drums.zip` to the `samples` release. To render locally:
`pip install numpy scipy soundfile && python3 generate.py "Cruz Drums"`.
