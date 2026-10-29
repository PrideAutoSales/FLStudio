"""Step 1: take every recording apart into its string modes.

Usage: python3 analyse.py <recordings dir> <analysis dir> [names...] [-j 4]

Writes one <name>.npz per recording (for example C4v12.npz: middle C, 12th of
16 strike strengths) holding the modes (k, f, decay, aL re/im, aR re/im, rise),
the fundamental, inharmonicity, onset and the level of the first 300 ms.
"""
import os, sys, argparse
import numpy as np
import modal as M


def run1(src, out_dir, name):
    out = os.path.join(out_dir, name + '.npz')
    if os.path.exists(out):
        return 'cached'
    note = M.midi_of(name.split('v')[0])
    path = os.path.join(src, name + '.flac')
    r = M.analyze(path, note)
    x, sr = M.load(path)
    x = x[:, r['t0']:]
    md = np.array([(m['k'], m['f'], m['d'], m['aL'].real, m['aL'].imag, m['aR'].real, m['aR'].imag, float(m['rise']))
                   for m in r['modes']], float)
    rms300 = float(np.sqrt((x[:, :int(0.3 * sr)] ** 2).mean()))
    peak = float(np.abs(x[:, :int(0.3 * sr)]).max())
    np.savez(out, modes=md, f0=r['f0'], B=r['B'], t0=r['t0'], rms300=rms300, peak=peak,
             note=note, vel=int(name.split('v')[1]), dur=x.shape[1] / sr)
    return '%d modes' % len(md)


def run(job):
    try:
        return job[2], run1(*job)
    except Exception as e:
        return job[2], 'FAILED %r' % e


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('src')
    ap.add_argument('out')
    ap.add_argument('names', nargs='*')
    ap.add_argument('-j', type=int, default=os.cpu_count())
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    # the sustained notes: A0v1.flac ... C8v16.flac (not the release or pedal samples)
    names = a.names or sorted(f[:-5] for f in os.listdir(a.src)
                              if f.endswith('.flac') and f[0] in 'ABCDEFG' and 'v' in f and f[:-5].split('v')[1].isdigit())
    from multiprocessing import Pool
    with Pool(a.j) as p:
        for name, msg in p.imap_unordered(run, [(a.src, a.out, n) for n in names]):
            print(name, msg, flush=True)
