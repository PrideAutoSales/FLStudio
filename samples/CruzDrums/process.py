"""Cruz Drums: the 'knock' chain.

Hard-hitting hip hop drums get their weight from processing as much as from the
source: driving a console channel into soft clipping, distorting, reshaping with EQ
and distorting again, squashing the tail up with a compressor, a small real-sounding
room, and resampling (pitching down) for thickness.
"""

from fractions import Fraction

import numpy as np
from scipy import signal

from drumsynth import SR, filt, noise, bitcrush, room_ir


def norm(x):
    return x / (np.max(np.abs(x)) + 1e-12)


def console(x, drive, asym=0.1):
    """Console channel driven into the red: soft knee, slightly asymmetric clipping."""
    y = np.arctan(drive * (x + asym * x ** 2)) / np.arctan(drive)
    return filt(y, "hp", 8, 1)  # block the DC the asymmetry creates


def hardclip(x, ceiling=0.85):
    """Clipper with a tiny soft knee: the 'a little clipping makes it hit' stage."""
    k = 0.15
    a = np.abs(x)
    y = np.where(a < ceiling - k, a, ceiling - k + k * np.tanh((a - (ceiling - k)) / k))
    return np.sign(x) * y


def biquad(kind, f, gain_db=0.0, q=0.707):
    A = 10 ** (gain_db / 40)
    w = 2 * np.pi * min(f, SR * 0.45) / SR
    al = np.sin(w) / (2 * q)
    c = np.cos(w)
    if kind == "peak":
        b = [1 + al * A, -2 * c, 1 - al * A]
        a = [1 + al / A, -2 * c, 1 - al / A]
    elif kind == "low":
        sq = 2 * np.sqrt(A) * al
        b = [A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq)]
        a = [(A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq]
    else:  # high shelf
        sq = 2 * np.sqrt(A) * al
        b = [A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq)]
        a = [(A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq]
    return np.array(b) / a[0], np.array(a) / a[0]


def eq(x, kind, f, gain_db, q=0.707):
    b, a = biquad(kind, f, gain_db, q)
    return signal.lfilter(b, a, x, axis=-1)


def compress(x, thr_db=-18, ratio=4.0, attack=0.003, release=0.08, makeup=True):
    lvl = np.max(np.abs(np.atleast_2d(x)), axis=0)
    ar = np.exp(-1 / (release * SR))
    env = signal.lfilter([1 - ar], [1, -ar], lvl)
    env = np.maximum(env, 1e-9)
    thr = 10 ** (thr_db / 20)
    gain = np.where(env > thr, (env / thr) ** (1 / ratio - 1), 1.0)
    aa = np.exp(-1 / (attack * SR))
    gain = signal.lfilter([1 - aa], [1, -aa], gain - 1) + 1  # attack lets the transient through
    y = x * gain
    return norm(y) if makeup else y


def transient(x, amount):
    """Boost the first milliseconds of the hit relative to the body."""
    lvl = np.max(np.abs(np.atleast_2d(x)), axis=0)
    fast = signal.lfilter([1 - np.exp(-1 / (0.0007 * SR))], [1, -np.exp(-1 / (0.0007 * SR))], lvl)
    slow = signal.lfilter([1 - np.exp(-1 / (0.02 * SR))], [1, -np.exp(-1 / (0.02 * SR))], lvl)
    g = 1 + amount * np.clip((fast - slow) / (fast + 1e-9), 0, 1)
    return x * g


def varispeed(x, factor):
    """Resample like a sampler pitched down (factor < 1 = slower, lower, thicker)."""
    fr = Fraction(factor).limit_denominator(60)
    return signal.resample_poly(x, fr.denominator, fr.numerator, axis=-1)


def tape(x, drive=1.6):
    x = eq(x, "peak", 90, 2.0, 0.8)  # head bump
    x = np.tanh(drive * x) / np.tanh(drive)
    return filt(x, "lp", 15500, 1)


def room(x, rng, mix, size=0.5, damp=7000):
    """Small studio room: early reflections plus a short, dark tail. Returns stereo."""
    x = taper(x)
    ir = room_ir(rng, 0.25 + size * 0.9, 0.12 + size * 0.7, lp=damp, pre=0.002 + size * 0.01,
                 early=int(6 + size * 10))
    wet = np.array([signal.fftconvolve(x, ir[c]) for c in range(2)])
    wet = filt(wet, "hp", 250, 1)
    dry = np.zeros_like(wet)
    dry[:, : len(x)] = x
    wet *= np.max(np.abs(x)) / (np.max(np.abs(wet)) + 1e-12)
    return dry + mix * wet


def grit(x, rng, level=0.01, crackle=True):
    """Noise floor that rides the hit: hiss and dust that make it sound sampled."""
    n = x.shape[-1]
    env = np.maximum.accumulate(np.abs(np.atleast_2d(x)).max(0)[::-1])[::-1]
    hiss = filt(noise(n, rng), "bp", (1500, 11000)) * level
    if crackle:
        dust = np.zeros(n)
        k = rng.integers(0, n, int(n / SR * 40) + 1)
        dust[k] = rng.uniform(-1, 1, len(k)) * level * 8
        hiss += filt(dust, "hp", 1200, 1)
    return x + hiss * env


def taper(x, ms=25):
    """Fade the end of a source buffer so nothing downstream sees a cut-off edge."""
    m = min(int(ms / 1000 * SR), x.shape[-1] // 4)
    x = np.array(x, dtype=float)
    x[..., -m:] *= np.cos(np.linspace(0, np.pi / 2, m)) ** 2
    return x


def knock_chain(x, rng, st, bright=True):
    """Distort, reshape with EQ, distort again, then squash and clip."""
    x = norm(taper(x))
    x = console(x, rng.uniform(*st.get("drive1", (1.5, 4.0))))
    if bright:
        x = eq(x, "peak", rng.uniform(2200, 5000), rng.uniform(*st.get("presence", (2, 6))), 0.9)
        x = eq(x, "high", rng.uniform(9000, 12000), -rng.uniform(1, 4))
    x = console(norm(x), rng.uniform(*st.get("drive2", (1.2, 3.0))))
    x = compress(x, rng.uniform(-16, -8), rng.uniform(2, 4), rng.uniform(0.002, 0.008), rng.uniform(0.03, 0.08))
    return x


def finish_color(x, rng, st):
    """Genre colour at the end of every chain."""
    x = taper(x)
    if st.get("varispeed") and rng.random() < st.get("varispeed_p", 0.6):
        x = varispeed(x, rng.uniform(*st["varispeed"]))
    if st.get("crush"):
        x = bitcrush(x, rate=int(rng.choice([26040, 22050, 32000])), bits=int(rng.choice([12, 12, 14])))
        x = filt(x, "lp", rng.uniform(*st.get("crush_lp", (8500, 12000))), 2)
    if st.get("tape"):
        x = tape(norm(x), rng.uniform(1.2, 2.2))
    if st.get("grit"):
        x = grit(x, rng, rng.uniform(*st["grit"]), crackle=st.get("crush", False))
    return x


def loud(x, rng, st):
    """Final clip stage: loud and dense but below digital clipping."""
    x = norm(x)
    return hardclip(x * rng.uniform(*st.get("clip", (1.1, 1.6))), 0.9)
