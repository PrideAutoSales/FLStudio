"""Pack the analysed bass recordings into the data file the Bass Guitar plays from.

Usage: python3 pack_bass.py <staged dir> <refined dir> ../../Resources/BassGuitar.cgpd

Like the grand's packer (tools/piano/pack_grand.py), with what a bass needs:
- every take of every strength is kept, so repeated notes cycle through the
  four real plucks (round robins);
- the attack residual covers the first 0.2 s of every note (the finger leaving
  the string, the string slapping the frets and the inharmonic ring that goes
  with it), and is taken against the 24-point gain curve the synth actually
  plays, so the attack matches the recording exactly;
- the release recordings (the finger damping the string) are kept per key
  and take, trimmed where they fall into the noise, with their level over the
  first 60 ms so the synth can play them at the level of the string it stops;
- one channel (the bass was recorded through its amp, in mono).
Layout (version 2, little-endian, zlib-compressed) matches Source/GrandData.cpp.
"""
import sys, os, glob, struct, zlib, argparse
import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'piano'))
from pack_grand import prune_safe, overtone_centres, overtone_levels  # noqa: E402

NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
LAYERS, TAKES = 4, 4
ATTACK = 0.2


def name_of(n):
    return '%s%d' % (NAMES[n % 12], n // 12 - 1)


FIXED = {"refit": 0, "errs": []}


def pencil(y, p):
    """The p poles of an evenly sampled sum of decaying tones (matrix pencil)."""
    n = len(y)
    L = n // 3
    Y = np.lib.stride_tricks.sliding_window_view(y, L + 1)
    try:
        U, S, Vh = np.linalg.svd(Y, full_matrices=False)
    except np.linalg.LinAlgError:
        return None
    if p > len(S) or S[p - 1] < S[0] * 1e-9:
        return None
    V = Vh[:p].T   # rows of Vh span [1, z, z^2, ...]
    return np.linalg.eigvals(np.linalg.pinv(V[:-1]) @ V[1:])


def sanitize(md, ratio=8.0, max_modes=10):
    """Overtones whose fit leans on modes with large, opposite amplitudes are refitted.

    A fit can trade a slightly bent envelope for a few near-identical modes that
    cancel each other. That holds only as long as every mode plays exactly as
    fitted: the Sustain knob stretches the decays and the Strings knob the
    spacing, the data file keeps amplitudes to 1/256 dB, and any of that undoes
    the cancellation, so the overtone comes out far too loud. An overtone whose
    modes add up to more than `ratio` times its own level after the attack is
    refitted from its envelope (from the end of the attack on, sampled every
    10 ms) with the fewest modes (matrix pencil) that keep it within 3% (rms)
    and need no such cancellation. Within the attack nothing changes: the
    residual is taken against what the synth plays."""
    tu = np.arange(ATTACK * 0.75, 3.4, 0.01)
    out = []
    for k in np.unique(md[:, 0]):
        rows = md[md[:, 0] == k]
        A = rows[:, 3] + 1j * rows[:, 4]
        s = -rows[:, 2] + 2j * np.pi * rows[:, 1]
        fref = rows[0, 1]
        E = np.exp(np.outer(tu, s - 2j * np.pi * fref))
        y = E @ A
        peak = np.abs(y).max()
        # how loud the overtone is in the attack (the residual is taken against
        # the modes, so a refit mustn't swell far past it going back to the start)
        ta = np.linspace(0.0, ATTACK * 0.75, 61)
        pa = max(np.abs(np.exp(np.outer(ta, s - 2j * np.pi * fref)) @ A).max(), peak)

        def risk(Ex, Ax):
            return (np.abs(Ex) @ np.abs(Ax)).max() / max(peak, 1e-30)

        if peak <= 0 or risk(E, A) <= ratio:
            out.append(rows)
            continue
        cands = []
        for p in range(1, max_modes + 1):
            z = pencil(y, p)
            if z is None:
                break
            d = np.maximum(-np.log(np.abs(z) + 1e-30) / 0.01, 0.02)
            s2 = -d + 1j * np.angle(z) / 0.01 + 2j * np.pi * fref
            E2 = np.exp(np.outer(tu, s2 - 2j * np.pi * fref))
            a2 = np.linalg.lstsq(E2, y, rcond=None)[0]
            back = np.abs(a2).sum() / pa
            cands.append((np.linalg.norm(E2 @ a2 - y) / np.linalg.norm(y), max(risk(E2, a2), back), s2, a2))
            if cands[-1][0] < 0.03 and cands[-1][1] <= 2.0:
                break
        # the fewest modes within 3% that neither cancel nor swell going back into
        # the attack; failing that, a little of either, then a little more; failing
        # that, the closest fit with a little more
        best = None
        for lim in (2.0, 4.0, ratio):
            ok = [c for c in cands if c[1] <= lim and c[0] < 0.03]
            if ok:
                best = (ok[0][0], ok[0][2], ok[0][3])
                break
        if best is None:
            ok = [c for c in cands if c[1] <= ratio]
            if ok:
                c = min(ok, key=lambda c: c[0])
                best = (c[0], c[2], c[3])
        if best is None:
            # the slowest mode alone, at the overtone's level
            i = int(np.argmin(rows[:, 2] + 1e3 * rows[:, 7]))
            s2 = s[[i]]
            E2 = np.exp(np.outer(tu, s2 - 2j * np.pi * fref))
            a2 = np.linalg.lstsq(E2, y, rcond=None)[0]
            best = (np.linalg.norm(E2 @ a2 - y) / np.linalg.norm(y), s2, a2)
        err, s2, a2 = best
        FIXED["refit"] += 1
        FIXED["errs"].append(err)
        FIXED.setdefault("lv", []).append((err, peak))
        new = np.repeat(rows[[int(np.argmin(rows[:, 2] + 1e3 * rows[:, 7]))]], len(s2), 0)
        new[:, 1], new[:, 2], new[:, 7] = s2.imag / (2 * np.pi), -s2.real, 0
        new[:, 3], new[:, 4] = a2.real, a2.imag
        new[:, 5], new[:, 6] = a2.real, a2.imag   # recorded in mono
        out.append(new)
    return np.vstack(out)


def as_stored(md):
    """The modes' amplitudes as the data file stores them (1/256 dB and 16-bit phase),
    so the attack residual is taken against exactly what the synth plays."""
    out = md.copy()
    for c in (3, 5):
        a = md[:, c] + 1j * md[:, c + 1]
        db = np.clip(np.round(20 * np.log10(np.abs(a) + 1e-30) * 256), -32768, 32767) / 256
        ph = np.round(np.angle(a) / np.pi * 32767) * np.pi / 32767
        q = 10 ** (db / 20) * np.exp(1j * ph)
        out[:, c], out[:, c + 1] = q.real, q.imag
    return out


def attack(md, x, sr):
    """Gain curve for the modes over the attack (24 points, as the synth plays it)
    and the residual that makes the attack match the recording exactly."""
    n = int(ATTACK * sr)
    t = np.arange(n) / sr
    E = np.exp(np.outer(t, -md[:, 2] + 2j * np.pi * md[:, 1]))
    m = np.real(E @ (md[:, 3] + 1j * md[:, 4]))
    xs = x[0, :n]
    w = int(0.001 * sr)
    nb = n // w
    g = np.zeros(nb)
    for b in range(nb):
        sl = slice(b * w, (b + 1) * w)
        g[b] = np.clip((xs[sl] * m[sl]).sum() / ((m[sl] ** 2).sum() + 1e-12), 0, 1.2)
    g = np.maximum.accumulate(np.minimum(g, 1.0))
    gt = np.interp(t, (np.arange(nb) + 0.5) * w / sr, g, left=0.0)
    ramp = np.clip((t - 0.6 * ATTACK) / (0.4 * ATTACK), 0, 1)
    gt = gt * (1 - ramp) + ramp
    gtab = np.interp(np.linspace(0, ATTACK, 24), t, gt).astype(np.float32)
    played = np.interp(t, np.linspace(0, ATTACK, 24), gtab)
    resid = xs - played * m
    fade = np.clip((n - np.arange(n)) / (0.25 * n), 0, 1)
    resid *= 0.5 - 0.5 * np.cos(np.pi * fade)
    return gtab, resid.astype(np.float32)


def release(path):
    """The release recording from its start to where it falls 45 dB below its
    loudest 10 ms (or into the noise, or 0.6 s), faded out; and its level over the first 60 ms."""
    x, sr = sf.read(path)
    h = int(0.01 * sr)
    env = np.array([np.sqrt((x[i:i + h] ** 2).mean()) for i in range(0, len(x) - h, h)])
    floor = np.percentile(env, 5)
    lim = max(env.max() * 10 ** (-45 / 20), floor * 2.0)
    above = np.where(env > lim)[0]
    end = min(len(x), int(0.6 * sr), (above[-1] + 2) * h if len(above) else len(x))
    y = x[:end].copy()
    f = int(0.03 * sr)
    y[-f:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, f))
    ref = float(np.sqrt((x[: int(0.06 * sr)] ** 2).mean()))
    return y.astype(np.float32), 20 * np.log10(ref + 1e-12)


BITS = 13   # the recorded parts are kept to 13 bits of their own peak (78 dB), which packs far smaller


def pcm(res):
    peak = float(np.abs(res).max()) if res.size else 0.0
    top = 2 ** (BITS - 1) - 1
    q = np.round(res / max(peak, 1e-12) * top).astype(np.int32)
    scale = peak * 32767 / top    # the synth reads a sample value of 32767 as this
    qd = np.diff(q, prepend=0)
    qd = np.clip(qd, -32768, 32767).astype('<i2')
    assert np.array_equal(np.cumsum(qd.astype(np.int32)), q)
    return scale, qd.tobytes()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('staged')
    ap.add_argument('src')
    ap.add_argument('out')
    ap.add_argument('--max-modes', type=int, default=700)
    a = ap.parse_args()

    recs = {}
    for p in glob.glob(os.path.join(a.src, '*.npz')):
        z = np.load(p)
        v = int(z['vel']) - 1
        recs[(int(z['note']), v // TAKES, v % TAKES)] = z
    notes = sorted(set(k[0] for k in recs))
    missing = [(n, l, t) for n in notes for l in range(LAYERS) for t in range(TAKES) if (n, l, t) not in recs]
    if missing:
        sys.exit('missing recordings: %s' % missing)

    # Each strength's loudness is the mean of its four takes (each take keeps its
    # own small difference from it); the level a key plays at is that, evened
    # across neighbouring keys and made to rise steadily with strength.
    loud = {k: 20 * np.log10(float(z['rms300'])) for k, z in recs.items()}
    mean = np.array([[np.mean([loud[(n, l, t)] for t in range(TAKES)]) for l in range(LAYERS)] for n in notes])
    target = mean.copy()
    for i in range(len(notes)):
        lo, hi = max(0, i - 1), min(len(notes) - 1, i + 1)
        target[i] = np.median(mean[[lo, i, hi]], axis=0)
    for i in range(len(notes)):
        x = np.arange(LAYERS)
        target[i] = np.maximum.accumulate(np.polyval(np.polyfit(x, target[i], 2), x))

    body = bytearray()
    body += b'CGPD' + struct.pack('<i', 2)
    body += struct.pack('<fHHHH', 44100.0, len(notes), LAYERS, TAKES, 1)
    body += bytes(notes)
    total_modes = total_res = 0
    cents = []
    for i, n in enumerate(notes):
        for l in range(LAYERS):
            for t in range(TAKES):
                z = recs[(n, l, t)]
                md = sanitize(z['modes'])
                idx = prune_safe(md, rel_db=-30.0, max_modes=a.max_modes)
                centres = overtone_centres(md)
                ot = overtone_levels(md, centres, t=0.3)   # after the pluck, where the tone settles
                m = as_stored(md[idx])
                x, sr = sf.read(os.path.join(a.staged, '%sv%d.flac' % (name_of(n), l * TAKES + t + 1)), always_2d=True)
                assert sr == 44100
                x = x.T[:, int(z['t0']):]
                g, res = attack(m, x, sr)
                scale, raw = pcm(res)
                f0 = float(z['f0'])
                cents.append(1200 * np.log2(f0 / (440 * 2 ** ((n - 69) / 12))))
                body += struct.pack('<ffff', f0, mean[i, l], target[i, l], ATTACK)
                body += g.astype('<f4').tobytes()
                body += struct.pack('<HHif', len(m), len(ot), len(res), scale)
                for fc, lev in ot:
                    body += struct.pack('<ff', fc, lev)
                for row in m:
                    k, f, d = row[0], row[1], row[2]
                    fc = centres[k]
                    vals = []
                    for amp in (row[3] + 1j * row[4], row[5] + 1j * row[6]):
                        db = 20 * np.log10(abs(amp) + 1e-30)
                        vals += [int(np.clip(round(db * 256), -32768, 32767)), int(round(np.angle(amp) / np.pi * 32767))]
                    body += struct.pack('<fff4h', fc, f - fc, d, *vals)
                body += raw
                total_modes += len(m)
                total_res += len(res)
                print('%3d %s take %d: %3d of %3d modes, %3d overtones, %+5.1f cents, %.1f dB (target %.1f)'
                      % (n, ['p', 'mp', 'mf', 'f'][l], t + 1, len(m), len(md), len(ot), cents[-1], loud[(n, l, t)], target[i, l]))
    # the releases: per key, per take
    total_rel = 0
    for n in notes:
        for t in range(TAKES):
            y, ref = release(os.path.join(a.staged, '%sr%d.flac' % (name_of(n), t + 1)))
            scale, raw = pcm(y)
            body += struct.pack('<iff', len(y), scale, ref)
            body += raw
            total_rel += len(y)
    packed = zlib.compress(bytes(body), 9)
    open(a.out, 'wb').write(packed)
    nrec = len(notes) * LAYERS * TAKES
    e = np.array(FIXED['errs'] or [0.0])
    print('overtones refitted without cancelling modes: %d (rms error median %.1f%%, 99th %.1f%%)'
          % (FIXED['refit'], 100 * np.median(e), 100 * np.percentile(e, 99)))
    print('records %d, modes %d (avg %.0f), attack frames %d, release frames %d, tuning %+.1f to %+.1f cents, raw %.2f MB, packed %.2f MB'
          % (nrec, total_modes, total_modes / nrec, total_res, total_rel, min(cents), max(cents), len(body) / 1e6, len(packed) / 1e6))


if __name__ == '__main__':
    main()
