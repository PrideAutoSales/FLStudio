"""Modal analysis of one grand piano recording.

Every overtone k is isolated with a Gaussian band-pass around its measured
frequency, moved to baseband and decimated, then fitted with a few damped
complex exponentials (matrix pencil), jointly for both channels. The fit is
staged: the slow modes from the tail first, then faster ones on what is left,
each on an exponentially re-weighted signal so the quiet end counts as much as
the loud start. The amplitudes are then solved again over the whole note with
the band-pass response of a mode that starts at the onset, plus three "rise"
modes that let an overtone swell in, and those are refined against the raw
waveform of the attack.

An overtone is then sum_i Re(a_ci exp((-d_i + j 2 pi f_i) t)), t from the onset,
for each channel c.
"""
import numpy as np
import soundfile as sf
from scipy.special import wofz

NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']

def midi_of(n):
    return NAMES.index(n[:-1]) + 12 * (int(n[-1]) + 1)

def load(path):
    x, sr = sf.read(path, always_2d=True)
    return x.T.astype(np.float64), sr

def onset(x, sr):
    m = np.abs(x).max(0)
    hop = int(sr * 0.0005)
    nb = min(len(m), sr) // hop
    env = m[: nb * hop].reshape(nb, hop).max(1)
    pk = env[: int(0.3 * sr / hop)].max()
    i = int(np.argmax(env > 0.1 * pk))
    while i > 0 and env[i - 1] > 0.004 * pk:
        i -= 1
    return i * hop

def _peak(spec, freqs, fe, half):
    lo, hi = np.searchsorted(freqs, fe - half), np.searchsorted(freqs, fe + half)
    if hi <= lo + 2:
        return None, -300.0, -300.0
    i = lo + int(np.argmax(spec[lo:hi]))
    if 0 < i < len(spec) - 1:
        a, b, c = np.log(spec[i - 1] + 1e-30), np.log(spec[i] + 1e-30), np.log(spec[i + 1] + 1e-30)
        den = a - 2 * b + c
        dlt = 0.5 * (a - c) / den if den != 0 else 0.0
        f = freqs[i] + dlt * (freqs[1] - freqs[0])
    else:
        f = freqs[i]
    local = np.median(spec[max(0, lo - (hi - lo)): hi + (hi - lo)])
    return f, 20 * np.log10(spec[i] + 1e-30), 20 * np.log10(local + 1e-30)

def _spectrum(xm, sr, a, b):
    seg = xm[a:b] * np.hanning(b - a)
    N = 1 << int(np.ceil(np.log2((b - a) * 4)))
    return np.abs(np.fft.rfft(seg, N)), np.fft.rfftfreq(N, 1 / sr)

def find_partials(xm, sr, t0, note, fmax):
    f0n = 440 * 2 ** ((note - 69) / 12)
    longS, longF = _spectrum(xm, sr, t0 + int(0.03 * sr), t0 + int(min(3.0, 0.03 + 400 / f0n) * sr))
    shortS, shortF = _spectrum(xm, sr, t0 + int(0.01 * sr), t0 + int(max(0.25, 12 / f0n) * sr))
    f0, B = f0n, 1e-4
    found, misses, k = [], 0, 1
    while True:
        fe = k * f0 * np.sqrt(1 + B * k * k)
        if fe > fmax:
            break
        win = min(0.22 * f0, 0.05 * fe + 2)
        ok = False
        for S, F, need in ((longS, longF, 15), (shortS, shortF, 10)):
            f, lev, loc = _peak(S, F, fe, win)
            if f is not None and lev > loc + need:
                found.append((k, f, lev))
                ok = True
                break
        if ok:
            misses = 0
            if len(found) >= 3:
                K = np.array([p[0] for p in found], float)
                Fk = np.array([p[1] for p in found])
                A = np.vstack([np.ones_like(K), K ** 2]).T
                c = np.linalg.lstsq(A, (Fk / K) ** 2, rcond=None)[0]
                if c[0] > 0 and c[1] > 0:
                    f0, B = np.sqrt(c[0]), c[1] / c[0]
        else:
            misses += 1
            if misses > 8 and k > 12:
                break
        k += 1
    return f0, B, found

def band_baseband(X, N, sr, fc, sig_f, out_rate):
    """Complex baseband of the band around fc (Gaussian window), at about out_rate.
    x_band(t) = Re(y(t) exp(j 2 pi fc' t)) with fc' the returned centre."""
    df = sr / N
    M = 1 << int(np.ceil(np.log2(max(8, out_rate / df))))
    c = int(round(fc / df))
    idx = np.arange(c - M // 2, c + M // 2)
    idx = idx[(idx >= 0) & (idx < X.shape[1])]
    g = np.exp(-0.5 * ((idx * df - fc) / sig_f) ** 2)
    Y = np.zeros((X.shape[0], M), complex)
    Y[:, (idx - c) % M] = X[:, idx] * g[None, :]
    y = np.fft.ifft(Y, axis=1) * (M / N) * 2
    return y, M * df, c * df

def pencil(y, p_max, rel_db=-45.0):
    C, n = y.shape
    L = max(2, min(n // 2, 400))
    Y = np.vstack([np.lib.stride_tricks.sliding_window_view(y[ch], L + 1) for ch in range(C)])
    _, s, Vh = np.linalg.svd(Y, full_matrices=False)
    p = int(np.sum(s > s[0] * 10 ** (rel_db / 20)))
    p = max(1, min(p, p_max))
    V = Vh[:p].T
    z = np.linalg.eigvals(np.linalg.pinv(V[:-1]) @ V[1:])
    z = z[np.abs(z) > 0.2]
    if len(z) == 0:
        z = np.array([0.999 + 0j])
    return z

def H(s, sig_t):
    return np.exp(s ** 2 * sig_t ** 2 / 2)

def basis(tau, sb, sig_t, deriv=False):
    """Band-passed response of modes switched on at the onset, and optionally d/ds of it."""
    zarg = (tau[:, None] + sb[None, :] * sig_t ** 2) / sig_t
    with np.errstate(all='ignore'):
        phi = np.exp(np.outer(tau, sb) + (sb ** 2 * sig_t ** 2 / 2)[None, :])
        dphi = (tau[:, None] + sb[None, :] * sig_t ** 2) * phi if deriv else None
    near = ~(zarg.real > 4)
    if near.any():
        u = -1j * zarg[near] / np.sqrt(2)
        g = np.exp(-np.broadcast_to(tau[:, None], zarg.shape)[near] ** 2 / (2 * sig_t ** 2)) * 0.5
        wu = wofz(u)
        phi[near] = g * wu
        if deriv:
            dphi[near] = g * (-2 * u * wu + 2j / np.sqrt(np.pi)) * (-1j * sig_t / np.sqrt(2))
    phi = np.nan_to_num(phi)
    return (phi, np.nan_to_num(dphi)) if deriv else phi


def stage_signal(X, N, sr, fk, half):
    sig_f = half / 4.0
    y, rate, fc = band_baseband(X, N, sr, fk, sig_f, 2.5 * half)
    return y, 1.0 / rate, fc, 1 / (2 * np.pi * sig_f)

def fit_partial(X, N, sr, fk, t0s, tend, sp, p_stage=(3, 3, 4), joint=True, rise=(30.0, 90.0, 250.0)):
    halfs = (min(0.5 * sp, 6.0), min(0.5 * sp, 20.0), min(0.5 * sp, 100.0))
    bounds = [(1.2, None), (0.25, 1.2), (0.0, 0.25)]
    S_abs, A_abs = [], []   # modes in absolute terms: x(t) = Re(a exp(s t)), t from file start
    for st, ((ta, tb), half, pmax) in enumerate(zip(bounds, halfs, p_stage)):
        y, T, fc, sig_t = stage_signal(X, N, sr, fk, half)
        lo = t0s + max(ta, 3.2 * sig_t)
        hi = tend if tb is None else min(tend, t0s + tb + (0.0 if st == 2 else 0.3))
        if tb is None and hi - lo < 0.8:
            continue
        i0, i1 = int(np.ceil(lo / T)), int(hi / T)
        if i1 - i0 < 10:
            continue
        seg = y[:, i0:i1].copy()
        # remove what the slower stages already explain
        tt = (np.arange(i0, i1) * T)
        for s, a in zip(S_abs, A_abs):
            sb = s - 2j * np.pi * fc
            seg -= (a[:, None] * H(sb, sig_t)) * np.exp(sb * tt)[None, :]
        rel = np.arange(i1 - i0) * T
        e = 20 * np.log10(np.sqrt((np.abs(seg) ** 2).mean(0)) + 1e-12)
        slope = np.polyfit(rel, e, 1)[0] if len(rel) > 3 else 0.0
        dw = max(0.0, -slope * np.log(10) / 20) * 0.9
        wgt = np.exp(dw * rel)
        z = pencil(seg * wgt[None, :], pmax, rel_db=-40) * np.exp(-dw * T)
        # a string mode always decays; near-zero or growing poles are noise
        z = z[-np.log(np.abs(z)) / T > 0.015]
        if len(z) == 0:
            continue
        Z = z[None, :] ** np.arange(i1 - i0)[:, None]
        a = np.linalg.lstsq(Z * wgt[:, None], (seg * wgt[None, :]).T, rcond=None)[0].T   # amps at i0
        sb = np.log(z) / T                       # baseband s
        for m in range(len(z)):
            # amplitude at baseband time 0 (file start), unfiltered
            a0 = a[:, m] * np.exp(-sb[m] * i0 * T) / H(sb[m], sig_t)
            s_abs = sb[m] + 2j * np.pi * fc
            # x = Re(y e^{j2pi fc t}) and y = a0 e^{sb t}  ->  x = Re(a0 e^{s_abs t})
            S_abs.append(s_abs); A_abs.append(a0)
    if not S_abs:
        return [], []
    S_abs = np.array(S_abs); A_abs = np.array(A_abs)
    if joint:
        # Each partial may swell in rather than start at full level: add
        # "rise" poles at the strongest mode's frequency that let the fit
        # cancel the mode at the onset.
        y, T, fc, sig_t = stage_signal(X, N, sr, fk, halfs[2])
        tq = t0s + 3.5 * sig_t
        lv = np.abs(A_abs).max(1) * np.exp(S_abs.real * tq)
        sd = S_abs[int(np.argmax(lv))]
        S_all = np.concatenate([S_abs, sd - np.array(rise)])
        rows, rhs = [], []
        for half, (ta, tb) in ((halfs[2], (None, 0.4)), (halfs[1], (0.3, 1.5)), (halfs[0], (1.0, None))):
            y, T, fc, sig_t = stage_signal(X, N, sr, fk, half)
            lo = t0s - 2.5 * sig_t if ta is None else t0s + max(ta, 3.2 * sig_t)
            hi = tend if tb is None else min(tend, t0s + tb)
            i0, i1 = int(np.ceil(max(lo, 0) / T)), int(hi / T)
            if i1 - i0 < 4:
                continue
            seg = y[:, i0:i1]
            tt = np.arange(i0, i1) * T
            sb = S_all - 2j * np.pi * fc
            tau = tt - t0s
            # band-limited response of a mode switched on at the onset
            zarg = (tau[:, None] + sb[None, :] * sig_t ** 2) / sig_t
            with np.errstate(all='ignore'):
                direct = np.exp(np.outer(tau, sb) + (sb ** 2 * sig_t ** 2 / 2)[None, :])
                near = np.exp(-tau[:, None] ** 2 / (2 * sig_t ** 2)) * 0.5 * wofz(-1j * zarg / np.sqrt(2))
            Z = np.where(zarg.real > 4, direct, near) * np.exp(sb * t0s)[None, :]
            Z = np.nan_to_num(Z)
            edb = 20 * np.log10(np.sqrt((np.abs(seg) ** 2).mean(0)) + 1e-12)
            k = max(1, int((0.4 if ta is not None else 0.02) / T))
            trend = np.convolve(np.pad(edb, (k // 2, k - 1 - k // 2), mode='edge'), np.ones(k) / k, mode='valid')
            trend = np.maximum(trend, edb.max() - 60)
            w = 10 ** (-trend / 20) * np.sqrt(T)
            rows.append(Z * w[:, None]); rhs.append((seg * w[None, :]).T)
        if rows:
            A_abs = np.linalg.lstsq(np.vstack(rows), np.vstack(rhs), rcond=None)[0]
            S_abs = S_all
    return S_abs, A_abs

def analyze(path, note, fmax=18000.0, joint=True):
    x, sr = load(path)
    t0 = onset(x, sr)
    n = x.shape[1]
    f0, B, found = find_partials(x.mean(0), sr, t0, note, min(fmax, 0.45 * sr))
    N = 1 << int(np.ceil(np.log2(n * 2)))
    X = np.fft.rfft(x, N, axis=1)
    modes = []
    for k, fk, lev in found:
        sp = f0 * np.sqrt(1 + 3 * B * k * k)
        # end of useful signal: where the band reaches its noise floor
        y, T, fc, sig_t = stage_signal(X, N, sr, fk, min(0.5 * sp, 20.0))
        env = np.sqrt((np.abs(y) ** 2).mean(0))
        last = int((n / sr - 3 * sig_t) / T)
        i0 = int((t0 / sr + 3.2 * sig_t) / T)
        if last - i0 < 10: continue
        floor = np.percentile(env[max(i0, last - int(2.0 / T)): last], 20)
        ref = env[i0: i0 + max(3, int(0.15 / T))].max()
        if ref < floor * 4: continue
        above = np.where(env[i0:last] > max(floor * 3, ref * 1e-4))[0]
        tend = (i0 + (above[-1] + 1 if len(above) else 10)) * T
        S, A = fit_partial(X, N, sr, fk, t0 / sr, tend, sp, joint=joint)
        nrise = 3 if joint and len(S) > 3 else 0
        for i, (s, a) in enumerate(zip(S, A)):
            # move phase reference to the onset
            a_on = a * np.exp(s * t0 / sr)
            d = -s.real; f = s.imag / (2 * np.pi)
            if d < -0.01: continue
            modes.append(dict(k=k, f=float(f), d=float(max(d, 0.001)), aL=complex(a_on[0]), aR=complex(a_on[-1]), tend=tend - t0 / sr,
                              rise=i >= len(S) - nrise))
    res = dict(sr=sr, t0=t0, f0=f0, B=B, modes=modes, n=n, partials=[(k, f) for k, f, _ in found])
    if joint:
        attack_refit(x[:, t0:], sr, res)
    return res

def attack_refit(x, sr, res, Tw=None):
    """Re-solve the rise modes' amplitudes against the raw waveform of the attack."""
    if Tw is None:
        Tw = float(np.clip(6.0 / res['f0'], 0.04, 0.12))
    n = int(Tw * sr)
    t = np.arange(n) / sr
    fixed = [m for m in res['modes'] if not m['rise']]
    rise = [m for m in res['modes'] if m['rise'] and m['f'] < 0.45 * sr]
    if not rise:
        return
    base = np.zeros((2, n))
    for m in fixed:
        e = np.exp((-m['d'] + 2j * np.pi * m['f']) * t)
        base[0] += np.real(m['aL'] * e); base[1] += np.real(m['aR'] * e)
    E = np.exp(np.outer(t, np.array([-m['d'] + 2j * np.pi * m['f'] for m in rise])))
    Acols = np.hstack([E.real, -E.imag])
    lam = 1e-3 * np.linalg.norm(Acols) / np.sqrt(Acols.shape[1])
    Areg = np.vstack([Acols, lam * np.eye(Acols.shape[1])])
    for ch, key in ((0, 'aL'), (1, 'aR')):
        rhs = np.concatenate([x[ch, :n] - base[ch], np.zeros(Acols.shape[1])])
        sol = np.linalg.lstsq(Areg, rhs, rcond=None)[0]
        c = sol[:len(rise)] + 1j * sol[len(rise):]
        for m, ci in zip(rise, c):
            m[key] = complex(ci)
