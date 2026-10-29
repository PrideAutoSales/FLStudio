# Cruz Pop Mix template

An FL Studio mixing template for modern pop, built from Cruz plugins. There are
two copies: **Cruz Pop Mix** for FL Studio 2024 and newer, and
**Cruz Pop Mix (FL 20)** for FL Studio 20. They differ only in the chorus and
phaser sends, because FL 20 has no Hyper Chorus or Vintage Phaser.

## Install

Quit FL Studio, then paste this into Terminal:

```
cd /tmp && rm -rf CruzPopMix-mac CruzPopMix-mac.zip && curl -fL -o CruzPopMix-mac.zip https://github.com/PrideAutoSales/FLStudio/releases/download/latest/CruzPopMix-mac.zip && ditto -x -k CruzPopMix-mac.zip . && bash CruzPopMix-mac/install.sh
```

The installer copies the template into
`<FL user data>/Projects/Templates/Cruz/` and installs any Cruz plugin the
template uses that the Mac doesn't have yet (that step asks for the Mac
password). Open it from **File > New from template > Cruz**.

## What's inside

**Signal flow:** tracks > DRUM BUS / INST BUS / VOCAL BUS > MIX BUS > Master.
Every bus, send and the master starts with a Cruz EQ. The master ends with
Cruz Limiter (no FL limiter anywhere).

| Insert | Chain | Goes to |
| --- | --- | --- |
| 1-6 Kick, Snare, Clap, Hats, Perc, Rim | Cruz EQ (Snare adds Cruz FET) | DRUM BUS |
| 7 SC Trigger | none | sidechain to SC BUS only, silent in the mix |
| 10 Bass, 11 Keys | Cruz EQ (Bass adds Cruz Opto) | SC BUS |
| 12 Lead | Cruz EQ > Cruz Drive | INST BUS |
| 14 Vocal Rec | Cruz EQ > FET > Opto; pick your mic input here | VOCAL BUS |
| 15-18 Lead Vox, Vox Double, Harmonies, Adlibs | Cruz EQ > FET/Opto | VOCAL BUS |
| 20 DRUM BUS | Cruz EQ > FET > Tape | MIX BUS |
| 21 SC BUS | Cruz Sidechain (keyed by SC Trigger) > Cruz EQ | INST BUS |
| 22 INST BUS | Cruz EQ | MIX BUS |
| 23 VOCAL BUS | EQ > Opto (off) > FET (off) > EQ > Drive > EQ > Opto | MIX BUS |
| 24 MIX BUS | Cruz EQ > Opto > Tape | Master |
| 26 FX Reverb | Cruz Verb > Cruz EQ band-pass 300 Hz-8 kHz | MIX BUS |
| 27-30 FX Delay 1/2, 1/4, 1/8, 1/16 | Cruz Echo (tempo synced) > Cruz EQ band-pass | MIX BUS |
| 31 FX Slap | Cruz Echo 95 ms > Cruz EQ band-pass | MIX BUS |
| 32 FX Chorus | Hyper Chorus (FL 20: Vintage Chorus) > Cruz EQ band-pass | MIX BUS |
| 33 FX Phaser | Vintage Phaser (FL 20: Fruity Phaser) > Cruz EQ band-pass | MIX BUS |

On every send the EQ comes after the effect, so the whole return is filtered.
Sends from the tracks are already set at sensible levels.

**Channel rack:** six drum sample channels loaded with Cruz Drums Pop
one-shots, the SC Trigger kick, and three Cruz Synth channels (Soft Rhodes on
Keys, Round Bass, Smooth Lead).

**Playlist:** a 92-bar pop arrangement at 120 BPM with colored, labeled
section blocks and time markers (Intro, Verse, Pre-Chorus, Chorus,
Post-Chorus, Build-Up, Bridge, Break, Final Chorus, Outro), and a named lane
for each part. Every pattern is empty: there is no MIDI. Write chords into the
Chords pattern and they play wherever a Chords clip already sits.

## Building it

`make_template.py` writes both .flp files straight in FL 20.8's project
format, so FL itself isn't needed. `check_template.py` checks the result. The
`build-cruzpopmix.yml` workflow runs both, adds the samples from the Cruz Drums
release, and publishes `CruzPopMix-mac.zip` to the "latest" release.

```
python3 make_template.py dist/CruzPopMix-mac
python3 check_template.py "dist/CruzPopMix-mac/Cruz Pop Mix"   # after adding Samples/
```

`synth_states/*.xml` are Cruz Synth factory presets saved by
`tools/synth_state`, a small JUCE program that loads the synth, picks a preset
and writes its state.
