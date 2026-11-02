# Bass Guitar measurements

These scripts turn recordings of a real bass guitar into `Resources/BassGuitar.cgpd`, the data the Bass
Guitar model on the PIANO tab plays from. Like the Concert Grand ([tools/piano](../piano)), the plugin doesn't
play the recordings back. It rebuilds each note from measurements of how the string moves, with the same
analysis.

## What gets measured

The recordings are a fingered 5-string bass, recorded through its amp: all 42 notes from the low B
(30.9 Hz, MIDI 23, B1 as FL Studio names it) up to MIDI 64 (E5 in FL Studio), each plucked at four strengths
(p, mp, mf, f) four times. That is 672 notes, plus four recordings per note of the finger damping the string.

- **Overtones.** Each overtone of each of the 672 notes is fitted as a few decaying sine waves (the string's
  two directions of motion, slightly out of tune, fading at different rates), then refined against the
  recording wherever its envelope drifted. A bass sustains to the end of its recordings, so the noise floor
  comes from the amp's own hiss and hum (`noise.flac`, made from the quiet ends of the release recordings)
  instead of the end of each note. The library fades each note out from about 3.5 s, so the analysis stops
  at 3.4 s and the measured decay carries on from there.
- **Safe to turn.** A fit can sometimes match an overtone with near-identical modes whose large, opposite
  amplitudes cancel. That only holds while every mode plays exactly as fitted; Sustain stretches the decays,
  Strings the spacing, and the file keeps amplitudes to 1/256 dB, any of which would leave the overtone far too
  loud. `pack_bass.py` refits each such overtone (about one in five) from its envelope after the attack with the
  fewest modes that keep it within 3% and need no cancellation (a matrix pencil fit); what's left of the
  difference is at least 30 dB under the note's loudest overtone.
- **Pluck.** The first 0.2 s of every note (the finger leaving the string, the string against the frets and the
  ring that goes with them) is stored as a residual: whatever the modes don't explain, taken against the
  gain curve the synth plays, so the attack matches the recording sample for sample.
- **Takes.** All four takes of every strength are kept. Repeated notes cycle through them, as a real player's
  plucks are never quite the same twice. Each take keeps its own small difference in level and a few cents of
  tuning; each key's average is tuned to equal temperament.
- **Strengths.** Between two recorded strengths each overtone's level moves toward its level in the
  neighbouring strength, so the tone brightens smoothly with velocity.
- **Release.** The finger damping recordings are kept per note and take. On note-off the synth plays the
  take's release at the level the string had, and the string gives way to it.

## Running it

Needs Python 3 with `numpy`, `scipy` and `soundfile`. A full run takes about 15 minutes on four cores; keep
numpy to one thread per process (`OMP_NUM_THREADS=1`) or it takes hours.

```sh
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1
sh fetch.sh /tmp/bass                                            # about 1 GB download
python3 stage.py /tmp/bass/Samples/darkblack /tmp/bass/staged    # true pitch names, fade cut off, noise.flac
P=../piano
python3 $P/analyse.py /tmp/bass/staged /tmp/bass/modes --noise /tmp/bass/staged/noise.flac
python3 $P/refine.py  /tmp/bass/staged /tmp/bass/modes /tmp/bass/refined --noise /tmp/bass/staged/noise.flac
python3 $P/audit.py   /tmp/bass/staged /tmp/bass/refined --noise /tmp/bass/staged/noise.flac
python3 pack_bass.py  /tmp/bass/staged /tmp/bass/refined ../../Resources/BassGuitar.cgpd
```

## Credit

The recordings are the "darkblack" fingered bass from **Black And Blue Basses** v1.002 by Karoryfer Samples,
released to the public domain ([CC0](https://creativecommons.org/publicdomain/zero/1.0/)), from the
[sfzinstruments/karoryfer.black-and-blue-basses](https://github.com/sfzinstruments/karoryfer.black-and-blue-basses)
repository. Thanks to Karoryfer for giving them away.
