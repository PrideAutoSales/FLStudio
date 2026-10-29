"""Cruz Drums: physical and found-sound sources.

Struck skins, snare wires, cymbal plates, hands, wood, cardboard, glass, cloth and
mouth sounds, all modelled from scratch. These carry the organic, slightly messy
texture that hard-hitting hip hop kits get from recording real objects.
"""

import numpy as np
from scipy import signal

from drumsynth import SR, filt, noise, peak

# Mode ratios of an ideal circular membrane (zeros of Bessel functions).
BESSEL = [1.0, 1.594, 2.136, 2.296, 2.653, 2.918, 3.156, 3.501, 3.600, 3.652,
          4.060, 4.154, 4.601, 4.832, 4.903, 5.131]


def t_axis(n):
    return np.arange(n) / SR


def membrane(n, rng, f0, decay, bend=0.15, bend_tau=0.02, modes=12, pos=None, damp=1.0):
    """Struck drum head: inharmonic membrane modes with tension pitch bend."""
    t = t_axis(n)
    pos = rng.uniform(0.15, 0.85) if pos is None else pos
    bendc = np.cumsum(1 + bend * np.exp(-t / bend_tau)) / SR
    out = np.zeros(n)
    for k, r in enumerate(BESSEL[:modes]):
        f = f0 * r * rng.uniform(0.985, 1.015)
        if f * (1 + bend) > SR * 0.45:
            break
        a = 1.0 if k == 0 else pos * rng.uniform(0.25, 0.9) / (1 + 0.2 * k)
        d = decay / (1 + damp * 0.55 * k)
        out += a * np.sin(2 * np.pi * f * bendc + rng.uniform(0, 6.28)) * np.exp(-6.9 * t / d)
    return out


def contact(n, rng, hardness=0.5, length=0.002):
    """Stick, beater or hand contact noise. hardness 0 (felt) .. 1 (wood on metal)."""
    m = max(int(length * SR), 8)
    out = np.zeros(n)
    burst = noise(m, rng) * np.hanning(2 * m)[m:] ** 1.5
    burst = filt(burst, "lp", 1500 + hardness * 14000, 2)
    out[:m] = burst
    return out


def follower(x, release):
    a = np.exp(-1.0 / (release * SR))
    return signal.lfilter([1 - a], [1, -a], np.abs(x))


def wires(n, rng, drive, f0, tight=0.12, buzz=0.5):
    """Snare wires: noise that rattles in sympathy with the head."""
    nz = filt(noise(n, rng), "bp", (rng.uniform(1200, 2500), rng.uniform(8000, 13000)), 2)
    env = follower(drive, tight / 5)
    env /= np.max(env) + 1e-12
    t = t_axis(n)
    mod = 1 + buzz * np.abs(np.sin(np.pi * f0 * t + rng.uniform(0, 3)))
    grit = 1 + 0.6 * filt(noise(n, rng), "lp", 300, 1) * 4  # uneven wire contact
    return nz * env * mod * np.clip(grit, 0.2, 2.0)


def modes(n, rng, lo, hi, count, dec, amp_tilt=0.0, log=True, bright=0.0):
    """Random inharmonic modes between lo and hi Hz: wood, cardboard, glass, metal, plates."""
    t = t_axis(n)
    f = np.exp(rng.uniform(np.log(lo), np.log(hi), count)) if log else rng.uniform(lo, hi, count)
    out = np.zeros(n)
    for fi in f:
        if fi > SR * 0.45:
            continue
        d = rng.uniform(*dec) if isinstance(dec, tuple) else dec
        a = rng.uniform(0.3, 1.0) * (fi / lo) ** amp_tilt
        out += a * np.sin(2 * np.pi * fi * t + rng.uniform(0, 6.28)) * np.exp(-6.9 * t / d)
    return out / np.sqrt(count)


def grains(n, rng, rate, grain_ms=(0.4, 2.0), band=(2000, 12000), shape=None):
    """Sparse tiny impacts: beads, cloth, paper, rattles. rate is grains/s (array or scalar)."""
    rate = np.broadcast_to(rate, (n,))
    level = rate / (np.max(rate) + 1e-9)  # sparse late grains are also quieter
    hits = (rng.random(n) < rate / SR) * rng.uniform(-1, 1, n) * level
    g = int(rng.uniform(*grain_ms) / 1000 * SR)
    kernel = noise(max(g, 4), rng) * np.hanning(max(g, 4))
    x = signal.fftconvolve(hits, kernel)[:n]
    if band:
        x = filt(x, "bp", band, 2)
    return x


def hand_clap(n, rng, center=None, q=None):
    """One pair of hands: a short burst ringing the cupped-palm cavity."""
    center = center or rng.uniform(750, 1700)
    q = q or rng.uniform(2.5, 7)
    m = min(int(rng.uniform(0.03, 0.06) * SR), n)
    burst = np.zeros(n)
    burst[:m] = noise(m, rng) * np.exp(-np.arange(m) / (rng.uniform(0.004, 0.009) * SR))
    x = peak(burst, center, q) * 2.5 + peak(burst, center * rng.uniform(1.8, 2.6), q * 1.3)
    x += filt(burst, "hp", 900, 1) * rng.uniform(0.4, 0.9)
    return x


def clap_group(n, rng, people, spread=0.022, tail_room=0.0):
    out = np.zeros(n)
    base = rng.uniform(800, 1500)
    for p in range(people):
        off = 0 if p == 0 else int(rng.uniform(0, spread) * SR)
        if off >= n:
            continue
        c = hand_clap(n - off, rng, center=base * rng.uniform(0.75, 1.3))
        out[off:] += c * rng.uniform(0.6, 1.0)
    return out


def snap(n, rng):
    """Finger snap: sharp click, a resonant 'pop' and a little skin noise."""
    t = t_axis(n)
    f = rng.uniform(1500, 2600)
    x = np.sin(2 * np.pi * f * t) * np.exp(-6.9 * t / rng.uniform(0.012, 0.03))
    x += peak(contact(n, rng, 0.9, 0.0015), f * 1.4, 4) * 3
    x += filt(noise(n, rng), "bp", (2000, 8000)) * np.exp(-6.9 * t / 0.04) * 0.3
    return x


def mouth(n, rng, kind):
    """Beatbox-style mouth sounds: 'b' kick, 'k' snare, 'ts' hat, 'pf' clap, tongue click."""
    t = t_axis(n)
    nz = noise(n, rng)
    if kind == "b":
        f = rng.uniform(55, 90)
        body = np.sin(2 * np.pi * np.cumsum(f * (1 + 2.5 * np.exp(-t / 0.015))) / SR) * np.exp(-6.9 * t / rng.uniform(0.15, 0.35))
        pop = filt(nz, "lp", 900, 2) * np.exp(-t / 0.006)
        return body + pop * 0.8
    if kind == "k":
        fs = [rng.uniform(1200, 1800), rng.uniform(2400, 3200), rng.uniform(4000, 5500)]
        burst = nz * np.exp(-6.9 * t / rng.uniform(0.08, 0.18))
        x = sum(peak(burst, f, rng.uniform(3, 7)) for f in fs) * 2
        x += np.sin(2 * np.pi * rng.uniform(150, 220) * t) * np.exp(-6.9 * t / 0.06) * 0.6
        return x
    if kind == "ts":
        a = rng.uniform(0.003, 0.012)
        env = np.where(t < a, t / a, np.exp(-6.9 * (t - a) / rng.uniform(0.04, 0.2)))
        return filt(nz, "hp", rng.uniform(4500, 7000), 2) * env + peak(nz, rng.uniform(6000, 9000), 3) * env
    if kind == "pf":
        env = np.exp(-6.9 * t / rng.uniform(0.05, 0.12))
        return (peak(nz, rng.uniform(900, 1500), 3) * 2 + filt(nz, "hp", 2500) * 0.5) * env
    if kind == "click":
        f = rng.uniform(900, 2200)
        return np.sin(2 * np.pi * f * t) * np.exp(-6.9 * t / 0.02) + contact(n, rng, 0.7, 0.001) * 0.7
    raise ValueError(kind)


def table_slam(n, rng, low=60):
    """Palm or fist on a wooden table: low thump, dense wood modes, loose objects rattling."""
    t = t_axis(n)
    thump = np.sin(2 * np.pi * np.cumsum(low * rng.uniform(1, 1.6) * (1 + np.exp(-t / 0.01))) / SR)
    thump *= np.exp(-6.9 * t / rng.uniform(0.06, 0.14))
    wood = modes(n, rng, 120, 2200, 40, (0.02, 0.12), amp_tilt=-0.3)
    rattle = grains(n, rng, 900 * np.exp(-t / 0.05), (0.3, 1.0), (1500, 9000))
    return thump + wood * 1.4 + rattle * 0.25 + contact(n, rng, 0.3, 0.004) * 0.8


def box_hit(n, rng):
    """Cardboard box or case: boxy low-mid resonance with papery noise."""
    t = t_axis(n)
    x = modes(n, rng, 80, 600, 18, (0.03, 0.09), amp_tilt=-0.5) * 2
    x += filt(noise(n, rng), "bp", (500, 4000)) * np.exp(-6.9 * t / 0.05) * 0.4
    return x + contact(n, rng, 0.2, 0.005)


def glass_tap(n, rng):
    return modes(n, rng, 1200, 7000, 7, (0.08, 0.4)) + contact(n, rng, 1.0, 0.0006) * 0.4


def metal_tap(n, rng):
    return modes(n, rng, 600, 9000, 14, (0.05, 0.35), amp_tilt=0.1) + contact(n, rng, 1.0, 0.0008) * 0.5


def keys(n, rng):
    t = t_axis(n)
    out = np.zeros(n)
    for h in range(int(rng.integers(3, 8))):
        k = int(abs(rng.normal(0, 0.015)) * SR)
        if k < n:
            out[k:] += modes(n - k, rng, 3000, 14000, 6, (0.02, 0.08)) * rng.uniform(0.4, 1)
    return out + grains(n, rng, 600 * np.exp(-t / 0.04), (0.2, 0.6), (4000, 15000)) * 0.3


def cymbal(n, rng, decay, closed=True, size=1.0):
    """Hi-hat plates: dense inharmonic modes, a 'chick' when closed, stick tick."""
    t = t_axis(n)
    count = int(rng.integers(70, 130))
    f = np.exp(rng.uniform(np.log(2500 / size), np.log(16500), count))
    body = np.zeros(n)
    for fi in f:
        d = decay * rng.uniform(0.4, 1.3)
        body += rng.uniform(0.2, 1.0) * np.sin(2 * np.pi * fi * t + rng.uniform(0, 6.28)) * np.exp(-6.9 * t / d)
    body /= np.sqrt(count)
    wash = filt(noise(n, rng), "hp", 6000, 2) * np.exp(-6.9 * t / decay) * rng.uniform(0.3, 0.8)
    x = body + wash
    if closed:
        chick = filt(noise(n, rng), "bp", (600, 2500)) * np.exp(-6.9 * t / rng.uniform(0.008, 0.02))
        x += chick * rng.uniform(0.2, 0.6)
    x += contact(n, rng, 0.8, 0.0012) * rng.uniform(0.2, 0.6)
    return filt(x, "hp", 2000 if closed else 1200, 2)
