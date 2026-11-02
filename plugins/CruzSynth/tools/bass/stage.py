"""Step 0 for the bass: put the recordings in the form the piano tools read.

Usage: python3 stage.py <Samples/darkblack dir> <staged dir>

The library names its notes an octave above where they sound (its b1 is the
low B string, 30.9 Hz, MIDI 23). Each sustained note is written as
<note><octave>v<n>.flac with its true pitch, n = 4 * strength + take + 1
(strengths p, mp, mf, f; takes 1 to 4), cut at 3.4 s where the library's own
fade-out begins. The release recordings are copied as <note><octave>r<take>.flac.
noise.flac is the amplifier's own hiss and hum, taken from the ends of the release
recordings once the strings are quiet, so the analysis can tell a long sustain
from the noise floor.
"""
import os, sys, re
import numpy as np
import soundfile as sf

NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
LIB = {'c': 0, 'db': 1, 'd': 2, 'eb': 3, 'e': 4, 'f': 5, 'gb': 6, 'g': 7, 'ab': 8, 'a': 9, 'bb': 10, 'b': 11}
LAYERS = ['p', 'mp', 'mf', 'f']
CUT = 3.4


def midi(libname):
    m = re.fullmatch(r'([a-g]b?)(\d)', libname)
    return LIB[m.group(1)] + 12 * (int(m.group(2)) + 1) - 12   # an octave below the name


def name_of(n):
    return '%s%d' % (NAMES[n % 12], n // 12 - 1)


def main(src, out):
    os.makedirs(out, exist_ok=True)
    tails = []
    for f in sorted(os.listdir(os.path.join(src, 'reg'))):
        m = re.fullmatch(r'darkblack_([a-g]b?\d)_(p|mp|mf|f)_rr(\d)\.wav', f)
        if not m:
            continue
        x, sr = sf.read(os.path.join(src, 'reg', f))
        idx = 4 * LAYERS.index(m.group(2)) + int(m.group(3))
        sf.write(os.path.join(out, '%sv%d.flac' % (name_of(midi(m.group(1))), idx)), x[: int(CUT * sr)], sr, subtype='PCM_24')
    for f in sorted(os.listdir(os.path.join(src, 'rel'))):
        m = re.fullmatch(r'darkblack_([a-g]b?\d)_rel_rr(\d)\.wav', f)
        if not m:
            continue
        x, sr = sf.read(os.path.join(src, 'rel', f))
        sf.write(os.path.join(out, '%sr%s.flac' % (name_of(midi(m.group(1))), m.group(2))), x, sr, subtype='PCM_24')
        # the last 0.25 s of the higher notes' releases: the strings have stopped
        if midi(m.group(1)) >= 40:
            t = x[-int(0.25 * sr):].copy()
            fade = np.minimum(1, np.minimum(np.arange(len(t)), np.arange(len(t))[::-1]) / (0.01 * sr))
            tails.append(t * fade)
    sf.write(os.path.join(out, 'noise.flac'), np.concatenate(tails), sr, subtype='PCM_24')


if __name__ == '__main__':
    main(*sys.argv[1:3])
