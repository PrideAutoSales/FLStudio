"""Step 4: pack the analysed recordings into the data file the Concert Grand plays from.

Usage: python3 pack_grand.py <recordings dir> <refined dir> ../../Resources/ConcertGrand.cgpd

Each record keeps the modes that can change its overtones' envelopes by more
than a fraction of a dB (sorted loudest first), a gain curve for the modes
over the attack and the attack residual that makes the first 50 to 150 ms
match the recording exactly. The level each key plays at is evened across
neighbouring keys and made to rise steadily with strike strength.
Layout (little-endian, zlib-compressed) matches Source/GrandData.cpp.
"""
import sys, os, glob, struct, zlib, argparse
import numpy as np
import soundfile as sf

NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']


def attack(md, x, sr, f0, B):
    """Gain curve g for the modes over the attack, and the residual that makes
    the attack match the recording exactly: x = g * modes + residual."""
    sp = f0 * np.sqrt(1 + 3 * B)
    half = min(0.5 * sp, 100.0)
    sig_t = 1 / (2 * np.pi * half / 4)
    Ta = float(np.clip(3.2 * sig_t * 1.6, 0.05, 0.15))
    n = int(Ta * sr)
    t = np.arange(n) / sr
    E = np.exp(np.outer(t, -md[:, 2] + 2j * np.pi * md[:, 1]))
    m = np.stack([np.real(E @ (md[:, 3] + 1j * md[:, 4])), np.real(E @ (md[:, 5] + 1j * md[:, 6]))])
    xs = x[:, :n]
    w = int(0.001 * sr)
    nb = n // w
    g = np.zeros(nb)
    for b in range(nb):
        sl = slice(b * w, (b + 1) * w)
        g[b] = np.clip((xs[:, sl] * m[:, sl]).sum() / ((m[:, sl] ** 2).sum() + 1e-12), 0, 1.2)
    g = np.maximum.accumulate(np.minimum(g, 1.0))
    gt = np.interp(t, (np.arange(nb) + 0.5) * w / sr, g, left=0.0)
    ramp = np.clip((t - 0.6 * Ta) / (0.4 * Ta), 0, 1)
    ramp = 0.5 - 0.5 * np.cos(np.pi * ramp)
    gt = gt * (1 - ramp) + ramp
    resid = xs - gt[None, :] * m
    fade = np.clip((n - np.arange(n)) / (0.25 * n), 0, 1)
    resid *= (0.5 - 0.5 * np.cos(np.pi * fade))[None, :]
    gtab = np.interp(np.linspace(0, Ta, 24), t, gt)
    return Ta, gtab.astype(np.float32), resid.astype(np.float32)


def overtone_centres(md):
    """Energy-weighted centre frequency of each overtone k (ignoring rise modes)."""
    centres = {}
    for k in np.unique(md[:, 0]):
        sel = (md[:, 0] == k) & (md[:, 7] < 0.5)
        if not sel.any():
            sel = md[:, 0] == k
        m = md[sel]
        E = (m[:, 3] ** 2 + m[:, 4] ** 2 + m[:, 5] ** 2 + m[:, 6] ** 2) / np.maximum(m[:, 2], 0.02)
        centres[k] = float((m[:, 1] * E).sum() / max(E.sum(), 1e-30))
    return centres


def overtone_levels(md, centres, t=0.05):
    out = []
    for k, fc in sorted(centres.items()):
        m = md[md[:, 0] == k]
        e = np.exp((-m[:, 2] + 2j * np.pi * (m[:, 1] - fc)) * t)
        aL = ((m[:, 3] + 1j * m[:, 4]) * e).sum()
        aR = ((m[:, 5] + 1j * m[:, 6]) * e).sum()
        lev = 0.5 * (abs(aL) + abs(aR))
        out.append((fc, 20 * np.log10(lev + 1e-12)))
    return out


def prune_safe(md, rel_db=-30.0, drop_partial_db=-65.0, max_modes=760):
    """Drop modes that can't change their overtone's envelope by more than a
    fraction of a dB at any time, and whole overtones too quiet to matter.
    Modes are never dropped from a cancelling pair (that would leave the other
    one far too loud)."""
    tg = np.concatenate([np.linspace(0, 0.1, 51), np.geomspace(0.1, 25, 150)[1:]])
    s = (-md[:, 2] + 2j * np.pi * md[:, 1])
    ks = np.unique(md[:, 0])
    keep = np.zeros(len(md), bool)
    peaks, energies, envs = {}, {}, {}
    for k in ks:
        sel = np.where(md[:, 0] == k)[0]
        E = np.exp(np.outer(tg, s[sel] - 2j * np.pi * md[sel[0], 1]))
        aL = md[sel, 3] + 1j * md[sel, 4]; aR = md[sel, 5] + 1j * md[sel, 6]
        env = 0.5 * (np.abs(E @ aL) + np.abs(E @ aR))
        envs[k] = (sel, E, aL, aR, env)
        peaks[k] = env.max()
        energies[k] = np.trapezoid(env ** 2, tg)
    top = max(peaks.values())
    etop = max(energies.values())
    for k in ks:
        sel, E, aL, aR, env = envs[k]
        if peaks[k] < top * 10 ** (drop_partial_db / 20) and energies[k] < etop * 10 ** (drop_partial_db / 10):
            continue
        floor = max(env.max() * 10 ** (-70 / 20), top * 10 ** (-100 / 20))
        ref = np.maximum(env, floor)
        mag = 0.5 * (np.abs(aL) + np.abs(aR))
        contrib = mag[None, :] * np.exp(-md[sel, 2][None, :] * tg[:, None])
        ratio = (contrib / ref[:, None]).max(0)
        k_keep = ratio >= 10 ** (rel_db / 20)
        # verify: the kept modes alone must stay within 1 dB wherever the overtone is audible
        for _ in range(8):
            envk = 0.5 * (np.abs(E[:, k_keep] @ aL[k_keep]) + np.abs(E[:, k_keep] @ aR[k_keep]))
            audible = env > floor * 3
            dev = np.abs(20 * np.log10((envk[audible] + 1e-30) / (env[audible] + 1e-30))) if audible.any() else np.zeros(1)
            if dev.max() < 1.0 or k_keep.all():
                break
            dropped = np.where(~k_keep)[0]
            k_keep[dropped[np.argsort(-ratio[dropped])][: max(1, len(dropped) // 2)]] = True
        keep[sel[k_keep]] = True
    keep &= md[:, 1] < 20000
    idx = np.where(keep)[0]
    if len(idx) > max_modes:
        # over budget: drop whole overtones, quietest first
        order = sorted(ks, key=lambda k: energies[k])
        for k in order:
            if keep.sum() <= max_modes:
                break
            keep[md[:, 0] == k] = False
        idx = np.where(keep)[0]
    w = (md[idx, 3] ** 2 + md[idx, 4] ** 2 + md[idx, 5] ** 2 + md[idx, 6] ** 2)
    return idx[np.argsort(-w)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('samples')
    ap.add_argument('src')
    ap.add_argument('out')
    ap.add_argument('--max-modes', type=int, default=960)
    a = ap.parse_args()

    recs = {}
    for p in glob.glob(os.path.join(a.src, '*.npz')):
        z = np.load(p)
        recs[(int(z['note']), int(z['vel']))] = z
    notes = sorted(set(n for n, _ in recs))
    layers = sorted(set(v for _, v in recs))
    missing = [(n, v) for n in notes for v in layers if (n, v) not in recs]
    if missing:
        sys.exit('missing recordings: %s' % missing)

    # The level each key should play at: the recordings' loudness, evened out
    # across neighbouring keys and made to rise steadily with strike strength.
    loud = np.array([[20 * np.log10(float(recs[(n, v)]['rms300'])) for v in layers] for n in notes])
    target = loud.copy()
    for i in range(len(notes)):
        lo, hi = max(0, i - 1), min(len(notes) - 1, i + 1)
        target[i] = np.median(loud[[lo, i, hi]], axis=0)
    for i in range(len(notes)):
        x = np.arange(len(layers))
        c = np.polyfit(x, target[i], 2)
        fit = np.polyval(c, x)
        target[i] = np.maximum.accumulate(fit)

    body = bytearray()
    body += b'CGPD' + struct.pack('<i', 1)
    body += struct.pack('<fHH', 48000.0, len(notes), len(layers))
    body += bytes(notes)
    total_modes = total_res = 0
    for n in notes:
        for v in layers:
            z = recs[(n, v)]
            md = z['modes']
            idx = prune_safe(md, rel_db=-30.0, max_modes=a.max_modes)
            centres = overtone_centres(md)
            ot = overtone_levels(md, centres)
            m = md[idx]
            x, sr = sf.read(os.path.join(a.samples, '%s%dv%d.flac' % (NAMES[n % 12], n // 12 - 1, v)), always_2d=True)
            x = x.T[:, int(z['t0']):]
            Ta, g, res = attack(m, x, sr, float(z['f0']), float(z['B']))
            loud_db = loud[notes.index(n), layers.index(v)]
            target_db = target[notes.index(n), layers.index(v)]
            scale = float(np.abs(res).max()) if res.size else 0.0
            q = np.round(res / max(scale, 1e-12) * 32767).astype('<i2') if res.size else np.zeros((2, 0), '<i2')
            body += struct.pack('<ffff', float(z['f0']), loud_db, target_db, Ta)
            body += g.astype('<f4').tobytes()
            body += struct.pack('<HHif', len(m), len(ot), res.shape[1], scale)
            for fc, lev in ot:
                body += struct.pack('<ff', fc, lev)
            for row in m:
                k, f, d = row[0], row[1], row[2]
                fc = centres[k]
                aL = row[3] + 1j * row[4]
                aR = row[5] + 1j * row[6]
                vals = []
                for amp in (aL, aR):
                    db = 20 * np.log10(abs(amp) + 1e-30)
                    vals += [int(np.clip(round(db * 256), -32768, 32767)), int(round(np.angle(amp) / np.pi * 32767))]
                body += struct.pack('<fff4h', fc, f - fc, d, *vals)
            # stored as differences from the previous frame, which pack much smaller
            qd = np.diff(q.astype(np.int32), axis=1, prepend=0)
            qd = np.clip(qd, -32768, 32767).astype('<i2')
            assert np.array_equal(np.cumsum(qd.astype(np.int32), axis=1), q.astype(np.int32))
            body += qd.T.reshape(-1).tobytes()
            total_modes += len(m)
            total_res += res.shape[1]
            print('%3d v%-2d %4d of %4d modes, %3d overtones, %4d attack frames, %.1f dB (target %.1f)' % (n, v, len(m), len(md), len(ot), res.shape[1], loud_db, target_db))
    packed = zlib.compress(bytes(body), 9)
    open(a.out, 'wb').write(packed)
    print('records %d, modes %d (avg %.0f), residual frames %d, raw %.2f MB, packed %.2f MB'
          % (len(notes) * len(layers), total_modes, total_modes / (len(notes) * len(layers)), total_res, len(body) / 1e6, len(packed) / 1e6))


if __name__ == '__main__':
    main()
