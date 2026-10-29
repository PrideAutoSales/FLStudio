# Concert Grand measurements

These scripts turn recordings of a real grand piano into `Resources/ConcertGrand.cgpd`, the data the
Concert Grand model plays from. The plugin doesn't play the recordings back. It rebuilds each note from
measurements of how the strings move.

## What gets measured

For each of the 480 recordings (30 keys a minor third apart, each struck at 16 strengths):

- **Overtones.** The fundamental and inharmonicity are fitted from the spectrum, then each overtone is
  isolated with a narrow band-pass. A real overtone is not one sine wave. It is the two or three strings of the
  note, each moving in two directions, slightly out of tune with each other and fading at different rates.
  That gives the beating and the two-stage decay (a quick drop, then a long quiet "aftersound"). Each
  overtone is fitted as a few decaying sine waves, with their own frequency, decay rate, level and phase in
  each stereo channel, so the plugin reproduces the beating and decay exactly.
- **Attack.** The first 50 to 150 ms (the hammer's knock and the strings settling) are stored as a short
  stereo residual: whatever the modes don't explain. The modes fade in under it with a measured gain curve,
  so the attack matches the recording sample for sample.
- **Level.** The loudness of each recording, evened out across neighbouring keys and made to rise steadily
  with strike strength.

## Running it

Needs Python 3 with `numpy`, `scipy` and `soundfile` (`pip install -r requirements.txt`). A full run takes a
few hours on four cores.

```sh
sh fetch_recordings.sh /tmp/salamander                  # about 700 MB
python3 analyse.py /tmp/salamander/Samples /tmp/grand/modes           # 1: modes of every recording
python3 audit.py   /tmp/salamander/Samples /tmp/grand/modes           # 2: where the fit drifts
python3 refine.py  /tmp/salamander/Samples /tmp/grand/modes /tmp/grand/refined   # 3: refit those overtones
python3 audit.py   /tmp/salamander/Samples /tmp/grand/refined         # check again
python3 pack_grand.py /tmp/salamander/Samples /tmp/grand/refined ../../Resources/ConcertGrand.cgpd   # 4: pack
```

`modal.py` holds the analysis itself. `refine.py` adjusts each drifting overtone's modes against the
recording (and tries an extra slow mode for long tails), keeping a refit only if it follows the real
envelope more closely.

## Credit

The recordings are the **Salamander Grand Piano V3** by Alexander Holm (a Yamaha C5 recorded at 48 kHz with
two AKG C414s), licensed [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/). They come from the
[sfzinstruments/SalamanderGrandPiano](https://github.com/sfzinstruments/SalamanderGrandPiano) repository,
which maps them for sfz. `ConcertGrand.cgpd` is derived from these recordings and is shared under the same
licence, with this credit.
