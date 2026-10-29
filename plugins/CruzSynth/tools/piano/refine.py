"""Step 3: refit the overtones whose envelope drifts from the recording.

Usage: python3 refine.py <recordings dir> <analysis dir> <refined dir> [names...] [-j 4]

For each overtone the audit flags (within 45 dB of the loudest and more than
1.5 dB RMS off), the decay rate and frequency of every string mode are
adjusted to match the recording's band-passed signal from the end of the
attack to where the overtone reaches the noise floor. The amplitudes are
solved exactly at each step (variable projection, with Kaufman's Jacobian).
One or two extra slow modes, taken from what the fit leaves in the tail, are
also tried, for the long "aftersound" some strings have. A refit is kept only
if it follows the real envelope more closely, and an extra mode only if it
cuts the error by 15%. After the attack is solved again, each overtone keeps
whichever version (original or refit) is closer to the recording.
"""
import os, sys, argparse
import numpy as np
from scipy.optimize import least_squares
from modal import pencil, stage_signal, attack_refit, basis
import audit as AU

def band_data(X, N, sr, n, t0s, fk, half, lo, hi_cap=None):
    """Band-passed recording, its time from the onset, and weights that make every part of the decay count."""
    y, T, fc, sig_t = stage_signal(X, N, sr, fk, half)
    env = np.sqrt((np.abs(y) ** 2).mean(0))
    last = int((n / sr - 3 * sig_t) / T)
    i0 = int(np.ceil((t0s + max(lo, 3.5 * sig_t)) / T))
    if last - i0 < 8:
        return None
    floor = np.percentile(env[max(i0, last - int(2.0 / T)): last], 20)
    above = np.where(env[i0:last] > floor * 4)[0]
    if len(above) < 5:
        return None
    i1 = i0 + above[-1] + 1
    if hi_cap is not None:
        i1 = min(i1, int((t0s + hi_cap) / T))
    if i1 - i0 < 5:
        return None
    seg = y[:, i0:i1]
    tau = np.arange(i0, i1) * T - t0s
    edb = 20 * np.log10(np.sqrt((np.abs(seg) ** 2).mean(0)) + 1e-12)
    k = max(1, int(0.1 / T))
    trend = np.convolve(np.pad(edb, (k // 2, k - 1 - k // 2), mode='edge'), np.ones(k) / k, mode='valid')
    trend = np.maximum(trend, edb.max() - 60)
    w = 10 ** (-trend / 20) * np.sqrt(T)
    return dict(seg=seg, tau=tau, w=w, fc=fc, sig_t=sig_t, T=T)


class Problem:
    def __init__(self, bands, rise_s):
        self.bands = bands
        self.rise_s = np.asarray(rise_s, complex)
        self.fc = bands[0]['fc']
        self._key = None

    def design(self, s_free, deriv=False):
        s = np.concatenate([s_free, self.rise_s])
        rows, drows, rhs = [], [], []
        for b in self.bands:
            sb = s - 2j * np.pi * b['fc']
            r = basis(b['tau'], sb, b['sig_t'], deriv)
            if deriv:
                drows.append(r[1] * b['w'][:, None])
                r = r[0]
            rows.append(r * b['w'][:, None])
            rhs.append((b['seg'] * b['w'][None, :]).T)
        return np.vstack(rows), (np.vstack(drows) if deriv else None), np.vstack(rhs), s

    def solve(self, s_free):
        A, _, Y, s = self.design(s_free)
        amp = np.linalg.lstsq(A, Y, rcond=None)[0]
        return A, Y, s, amp

    def _eval(self, theta):
        key = theta.tobytes()
        if key == self._key:
            return self._cache
        s_free = self.to_s(theta)
        A, dA, Y, s = self.design(s_free, deriv=True)
        Q, _ = np.linalg.qr(A)
        amp = np.linalg.lstsq(A, Y, rcond=None)[0]
        R = A @ amp - Y
        # Kaufman's approximation to the variable-projection Jacobian:
        # d r / d theta_j = P_perp (dA/d theta_j) amp, only mode j's column moves
        m = len(s_free)
        d = np.exp(theta[m:])
        J = np.empty((2 * R.size, 2 * m))
        for i in range(m):
            for j, ds in ((i, 2j * np.pi), (m + i, -d[i])):
                v = dA[:, i:i + 1] * ds * amp[i:i + 1, :]
                v = v - Q @ (Q.conj().T @ v)
                v = v.ravel()
                J[:, j] = np.concatenate([v.real, v.imag])
        r = R.ravel()
        self._key, self._cache = key, (np.concatenate([r.real, r.imag]), J)
        return self._cache

    def resid(self, theta):
        return self._eval(theta)[0]

    def jac(self, theta):
        return self._eval(theta)[1]

    @staticmethod
    def to_theta(s):
        return np.concatenate([s.imag / (2 * np.pi), np.log(np.maximum(-s.real, 1e-3))])

    @staticmethod
    def to_s(theta):
        m = len(theta) // 2
        return -np.exp(theta[m:]) + 2j * np.pi * theta[:m]


def fit(prob, s0, fspan=3.0):
    th0 = Problem.to_theta(s0)
    m = len(s0)
    lo = np.concatenate([th0[:m] - fspan, np.full(m, np.log(0.01))])
    hi = np.concatenate([th0[:m] + fspan, np.full(m, np.log(400.0))])
    th0 = np.clip(th0, lo + 1e-9, hi - 1e-9)
    r = least_squares(prob.resid, th0, jac=prob.jac, bounds=(lo, hi), x_scale='jac', max_nfev=300, xtol=1e-9, ftol=1e-9)
    return Problem.to_s(r.x), r.cost


def score(X, N, sr, n, t0, f0, B, k, fc, rows):
    r = AU.overtone_error(X, N, sr, n, t0, f0, B, k, fc, rows)
    if r is None:
        return None
    t, rd, md_ = r
    w = np.gradient(np.log(t))
    e = md_ - rd
    return float(np.sqrt((w * e ** 2).sum() / w.sum()))


def to_rows(k, s, amp, fc, t0s, rise_flags):
    ph = np.exp(2j * np.pi * fc * t0s)
    out = []
    for i, si in enumerate(s):
        aL, aR = amp[i, 0] * ph, amp[i, -1] * ph
        out.append([k, si.imag / (2 * np.pi), -si.real, aL.real, aL.imag, aR.real, aR.imag, float(rise_flags[i])])
    return np.array(out)


def refine_overtone(X, N, sr, n, t0, f0, B, k, fc, rows, verbose=False):
    t0s = t0 / sr
    sp = f0 * np.sqrt(1 + 3 * B * k * k)
    base = score(X, N, sr, n, t0, f0, B, k, fc, rows)
    if base is None:
        return rows, None, None
    bands = [b for b in (band_data(X, N, sr, n, t0s, fc, min(0.5 * sp, 100.0), 0.0, 0.5),
                         band_data(X, N, sr, n, t0s, fc, min(0.5 * sp, 20.0), 0.3)) if b is not None]
    if not bands:
        return rows, base, base
    rise = rows[:, 7] > 0.5
    s_all = -rows[:, 2] + 2j * np.pi * rows[:, 1]
    prob = Problem(bands, s_all[rise])
    s_free0 = s_all[~rise]
    cands = []
    # 1: the same modes, refined
    s1, c1 = fit(prob, s_free0)
    cands.append(s1)
    # 2, 3: plus one or two slow modes taken from what the fit leaves in the tail
    nb = bands[-1]
    A, Y, s, amp = prob.solve(s1)
    nrow = len(nb['tau'])
    res_tail = (Y - A @ amp)[-nrow:] / nb['w'][:, None]
    half = len(res_tail) // 3
    tail = res_tail[half:].T
    if tail.shape[1] > 12:
        try:
            z = pencil(tail, 2, rel_db=-30)
            sb_new = np.log(z) / nb['T']
            sb_new = sb_new[(-sb_new.real > 0.01) & (-sb_new.real < 20)]
            new = sb_new + 2j * np.pi * nb['fc']
            for j in range(1, min(2, len(new)) + 1):
                sj, _ = fit(prob, np.concatenate([s1, new[:j]]))
                cands.append(sj)
        except np.linalg.LinAlgError:
            pass
    best, best_score = rows, base
    for c in cands:
        A, Y, s, amp = prob.solve(c)
        flags = np.concatenate([np.zeros(len(c)), np.ones(rise.sum())])
        r = to_rows(k, s, amp, prob.fc, t0s, flags)
        sc = score(X, N, sr, n, t0, f0, B, k, fc, r)
        if verbose:
            print('   cand %d modes: %.2f dB' % (len(c), sc))
        # an extra mode has to earn its place
        need = best_score * (0.85 if len(r) > len(best) else 0.97)
        if sc is not None and sc < need:
            best, best_score = r, sc
    return best, base, best_score


def tidy(rows, original):
    """Merge modes the fit left at the same pole (their sum is one mode), and fall back
    to the original fit if an overtone still relies on huge modes cancelling each
    other, which the 16-bit amplitudes in the data file can't reproduce."""
    out = []
    for r in rows:
        for o in out:
            if o[7] == r[7] and abs(o[1] - r[1]) < 1e-4 and abs(o[2] - r[2]) < 1e-4 * max(1.0, r[2]):
                o[3:7] += r[3:7]
                break
        else:
            out.append(r.copy())
    out = np.array(out)
    t = np.concatenate([np.linspace(0, 0.2, 41), np.linspace(0.25, 10, 40)])
    E = np.exp(np.outer(t, -out[:, 2] + 2j * np.pi * (out[:, 1] - out[0, 1])))
    peak = max(np.abs(E @ (out[:, 3] + 1j * out[:, 4])).max(), np.abs(E @ (out[:, 5] + 1j * out[:, 6])).max())
    big = np.abs(out[:, 3:7]).max()
    if big > 30 * peak:
        return original
    return out


def early_band(X, N, sr, n, t0, f0, B, k, fc):
    """The recording's wider band around overtone k from just before the onset to 0.4 s."""
    sp = f0 * np.sqrt(1 + 3 * B * k * k)
    y, T, fcb, sig_t = stage_signal(X, N, sr, fc, min(0.5 * sp, 100.0))
    t0s = t0 / sr
    i0, i1 = int(np.ceil(max(t0s - 2.5 * sig_t, 0) / T)), int((t0s + 0.4) / T)
    tau = np.arange(i0, i1) * T - t0s
    # in terms of amplitudes at the onset: y = exp(-j 2 pi fc t0) sum a G(tau)
    return dict(seg=y[:, i0:i1] * np.exp(2j * np.pi * fcb * t0s), tau=tau, fc=fcb, sig_t=sig_t, T=T)


def early_error(eb, rows):
    """RMS dB difference from the recording between 30 ms (or once the band settles) and 0.3 s."""
    G = basis(eb['tau'], -rows[:, 2] + 2j * np.pi * (rows[:, 1] - eb['fc']), eb['sig_t'])
    m = np.stack([G @ (rows[:, 3] + 1j * rows[:, 4]), G @ (rows[:, 5] + 1j * rows[:, 6])])
    sel = (eb['tau'] >= max(0.03, 3.5 * eb['sig_t'])) & (eb['tau'] <= 0.3)
    if sel.sum() < 3:
        return 0.0
    r = np.sqrt((np.abs(eb['seg'][:, sel]) ** 2).mean(0))
    mm = np.sqrt((np.abs(m[:, sel]) ** 2).mean(0))
    return float(np.sqrt(np.mean((20 * np.log10((mm + 1e-12) / (r + 1e-12))) ** 2)))


def fix_rise(eb, rows):
    """Solve the rise modes' amplitudes again against the recording's onset, the
    refined string modes held as they are."""
    rise = rows[:, 7] > 0.5
    if not rise.any():
        return rows
    out = rows.copy()
    G = basis(eb['tau'], -rows[:, 2] + 2j * np.pi * (rows[:, 1] - eb['fc']), eb['sig_t'])
    seg = eb['seg']
    edb = 20 * np.log10(np.sqrt((np.abs(seg) ** 2).mean(0)) + 1e-12)
    kk = max(1, int(0.02 / eb['T']))
    trend = np.convolve(np.pad(edb, (kk // 2, kk - 1 - kk // 2), mode='edge'), np.ones(kk) / kk, mode='valid')
    w = 10 ** (-np.maximum(trend, edb.max() - 60) / 20)
    for ch, c in ((0, 3), (1, 5)):
        want = seg[ch] - G[:, ~rise] @ (rows[~rise, c] + 1j * rows[~rise, c + 1])
        b = np.linalg.lstsq(G[:, rise] * w[:, None], want * w, rcond=None)[0]
        out[rise, c], out[rise, c + 1] = b.real, b.imag
    return out


def settle(X, N, sr, n, t0, f0, B, k, fc, new, orig):
    """A refit overtone, tidied and with its onset re-anchored to the recording; the
    original if the refit is further from the recording just after the attack."""
    new = tidy(new, orig)
    if new is orig or not len(orig):
        return new
    eb = early_band(X, N, sr, n, t0, f0, B, int(k), fc)
    new = fix_rise(eb, new)
    return new if early_error(eb, new) <= early_error(eb, orig) + 0.5 else orig


def keep_better(md, d, s, data):
    """The final say: any overtone that now follows the recording less closely than
    the original fit goes back to the original."""
    md3 = d['modes']
    s3 = AU.summarise(AU.audit(d, data))
    rows = []
    for k in sorted(set(md[:, 0]) | set(md3[:, 0])):
        a, b = s.get(int(k)), s3.get(int(k))
        worse = a is not None and (b is None or b[1] > a[1])
        rows.append(md[md[:, 0] == k] if worse else md3[md3[:, 0] == k])
    return np.vstack(rows)


def run1(src, ana, out_dir, name, verbose=False, min_rms=1.5, min_lev=-45.0):
    z = np.load(os.path.join(ana, name + '.npz'))
    md = z['modes'].copy()
    f0, B, t0 = float(z['f0']), float(z['B']), int(z['t0'])
    x, sr, X, N = data = AU.prepare(os.path.join(src, name + '.flac'))
    n = x.shape[1]
    cs = AU.centres(md)
    s = AU.summarise(AU.audit(z, data))
    out = [md[md[:, 0] == k] for k in sorted(cs)]
    log = []
    for i, k in enumerate(sorted(cs)):
        if k not in s:
            continue
        lev, rms, worst, tend = s[int(k)]
        if lev < min_lev or rms < min_rms:
            continue
        rows, a, b = refine_overtone(X, N, sr, n, t0, f0, B, int(k), cs[k], md[md[:, 0] == k], verbose)
        out[i] = rows
        log.append('k%d %.1f->%.1f' % (k, a, b))
    md2 = np.vstack(out)
    # the rise modes' amplitudes are solved again against the raw attack
    res = dict(f0=f0, sr=sr, modes=[dict(k=r[0], f=r[1], d=r[2], aL=r[3] + 1j * r[4], aR=r[5] + 1j * r[6], rise=r[7] > 0.5)
                                    for r in md2])
    attack_refit(x[:, t0:], sr, res)
    md2 = np.array([(m['k'], m['f'], m['d'], m['aL'].real, m['aL'].imag, m['aR'].real, m['aR'].imag, float(m['rise']))
                    for m in res['modes']])
    # Re-solving the attack can upset an overtone's later envelope (the lowest
    # keys' tightly packed overtones mostly): keep whichever version of each
    # overtone follows the recording more closely.
    d = dict(z)
    d['modes'] = md2
    s2 = AU.summarise(AU.audit(d, data))
    rows = []
    for k in sorted(set(md[:, 0]) | set(md2[:, 0])):
        a, b = s.get(int(k)), s2.get(int(k))
        orig, new = md[md[:, 0] == k], md2[md2[:, 0] == k]
        if b is not None and (a is None or b[1] <= a[1]) or (a is None and b is None):
            rows.append(orig if len(orig) and np.array_equal(new, orig) else settle(X, N, sr, n, t0, f0, B, k, cs.get(k, new[0, 1]), new, orig))
        else:
            rows.append(orig)
    d['modes'] = np.vstack(rows)
    d['modes'] = keep_better(md, d, s, data)
    np.savez(os.path.join(out_dir, name + '.npz'), **d)
    return ' '.join(log)


def run(job):
    try:
        if os.path.exists(os.path.join(job[2], job[3] + '.npz')):
            return job[3], 'cached'
        return job[3], run1(*job)
    except Exception as e:
        return job[3], 'FAILED %r' % e


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('src')
    ap.add_argument('analysis')
    ap.add_argument('out')
    ap.add_argument('names', nargs='*')
    ap.add_argument('-j', type=int, default=os.cpu_count())
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    names = a.names or sorted(f[:-4] for f in os.listdir(a.analysis) if f.endswith('.npz'))
    from multiprocessing import Pool
    with Pool(a.j) as p:
        for name, msg in p.imap_unordered(run, [(a.src, a.analysis, a.out, n) for n in names]):
            print(name, msg, flush=True)
