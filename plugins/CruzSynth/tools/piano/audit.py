"""Step 2 (and a check after step 3): how closely each record's modes follow the recording.

Usage: python3 audit.py <recordings dir> <analysis dir> [names...]

For every overtone the recording and the modes are put through the same
narrow band-pass (the modes analytically) and their envelopes compared from
0.1 s to where the overtone reaches the noise floor. Prints, per record, the
overtones within 30 dB of the loudest that drift by more than 2 dB RMS.
"""
import os, sys, argparse
import numpy as np
from modal import load, stage_signal, basis


def centres(md):
    """Energy-weighted centre frequency of each overtone (ignoring rise modes)."""
    out = {}
    for k in np.unique(md[:, 0]):
        sel = (md[:, 0] == k) & (md[:, 7] < 0.5)
        if not sel.any():
            sel = md[:, 0] == k
        m = md[sel]
        E = (m[:, 3] ** 2 + m[:, 4] ** 2 + m[:, 5] ** 2 + m[:, 6] ** 2) / np.maximum(m[:, 2], 0.02)
        out[k] = float((m[:, 1] * E).sum() / max(E.sum(), 1e-30))
    return out


def prepare(path):
    x, sr = load(path)
    n = x.shape[1]
    N = 1 << int(np.ceil(np.log2(n * 2)))
    return x, sr, np.fft.rfft(x, N, axis=1), N


def overtone_error(X, N, sr, n, t0, f0, B, k, fk, rows, half=None):
    """Smoothed dB envelopes of the recording and the modes for overtone k, where it is above the noise."""
    sp = f0 * np.sqrt(1 + 3 * B * k * k)
    half = half or min(0.5 * sp, 20.0)
    y, T, fc, sig_t = stage_signal(X, N, sr, fk, half)
    env = np.sqrt((np.abs(y) ** 2).mean(0))
    last = int((n / sr - 3 * sig_t) / T)
    i0 = int((t0 / sr + max(0.1, 3.5 * sig_t)) / T)
    if last - i0 < 10:
        return None
    floor = np.percentile(env[max(i0, last - int(2.0 / T)): last], 20)
    tau = np.arange(i0, last) * T - t0 / sr
    sb = (-rows[:, 2] + 2j * np.pi * rows[:, 1]) - 2j * np.pi * fc
    E = basis(tau, sb, sig_t)
    mL = E @ (rows[:, 3] + 1j * rows[:, 4])
    mR = E @ (rows[:, 5] + 1j * rows[:, 6])
    menv = np.sqrt(0.5 * (np.abs(mL) ** 2 + np.abs(mR) ** 2))
    renv = env[i0:last]
    ok = renv > floor * 4
    if ok.sum() < 5:
        return None
    # smooth over ~50 ms in dB so single-sample noise doesn't count
    kk = max(1, int(0.05 / T))
    sm = lambda v: np.convolve(np.pad(v, (kk // 2, kk - 1 - kk // 2), mode='edge'), np.ones(kk) / kk, mode='valid')
    rdb = sm(20 * np.log10(renv + 1e-12))
    mdb = sm(20 * np.log10(menv + 1e-12))
    return tau[ok], rdb[ok], mdb[ok]


def audit(z, data):
    md = z['modes']
    f0, B, t0 = float(z['f0']), float(z['B']), int(z['t0'])
    x, sr, X, N = data
    res = {}
    for k, fc in centres(md).items():
        r = overtone_error(X, N, sr, x.shape[1], t0, f0, B, k, fc, md[md[:, 0] == k])
        if r is not None:
            res[int(k)] = r
    return res


def summarise(res):
    """Per overtone: (level at 0.2 s relative to the loudest, RMS dB error over log time, worst error, end time)."""
    if not res:
        return {}
    lev = {k: np.interp(0.2, t, r) for k, (t, r, m) in res.items()}
    top = max(lev.values())
    out = {}
    for k, (t, r, m) in res.items():
        w = np.gradient(np.log(t))
        e = m - r
        out[k] = (lev[k] - top, float(np.sqrt((w * e ** 2).sum() / w.sum())), float(e[np.argmax(np.abs(e))]), float(t[-1]))
    return out


def run(job):
    src, ana, name = job
    try:
        z = np.load(os.path.join(ana, name + '.npz'))
        return name, summarise(audit(z, prepare(os.path.join(src, name + '.flac'))))
    except Exception as ex:
        return name, repr(ex)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('src')
    ap.add_argument('analysis')
    ap.add_argument('names', nargs='*')
    ap.add_argument('-j', type=int, default=os.cpu_count())
    a = ap.parse_args()
    names = a.names or sorted(f[:-4] for f in os.listdir(a.analysis) if f.endswith('.npz'))
    from multiprocessing import Pool
    with Pool(a.j) as p:
        for name, s in p.imap_unordered(run, [(a.src, a.analysis, n) for n in names]):
            if isinstance(s, str):
                print(name, s, flush=True)
                continue
            bad = [(k, v) for k, v in sorted(s.items()) if v[0] > -30 and v[1] > 2.0]
            print('%-7s %3d overtones, drifting: %s' % (name, len(s), ' '.join(
                'k%d(%.0fdB rms %.1f)' % (k, v[0], v[1]) for k, v in bad) or 'none'), flush=True)
