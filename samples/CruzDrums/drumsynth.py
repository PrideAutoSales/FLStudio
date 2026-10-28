"""Cruz Drums: synthesis building blocks.

Every sound in the pack is built from scratch here (oscillators, filtered noise,
modal resonators, saturation, generated room impulses). No recorded or third
party audio is used anywhere.
"""

import numpy as np
from scipy import signal

SR = 48000


def secs(n):
    return np.arange(int(n * SR)) / SR


def note_freq(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def note_name(freq):
    midi = int(round(69 + 12 * np.log2(freq / 440.0)))
    return NOTE_NAMES[midi % 12]


# ---------------------------------------------------------------- envelopes

def decay_env(n, decay, attack=0.0005, hold=0.0, curve=1.0):
    """Attack, hold, then exponential decay reaching -60 dB at `decay` seconds."""
    t = np.arange(n) / SR
    env = np.ones(n)
    a = max(int(attack * SR), 1)
    env[:a] = np.linspace(0, 1, a) ** 0.5
    rel = np.clip(t - attack - hold, 0, None)
    env *= np.exp(-6.9 * (rel / decay) ** curve)
    return env


# -------------------------------------------------------------- oscillators

def sweep_phase(n, f_end, f_start, tau, tau2=None, start2=None):
    t = np.arange(n) / SR
    f = f_end + (f_start - f_end) * np.exp(-t / tau)
    if tau2 is not None:
        f += (start2 or 0) * np.exp(-t / tau2)
    return 2 * np.pi * np.cumsum(f) / SR


def noise(n, rng, color="white"):
    x = rng.standard_normal(n)
    if color == "pink":
        b = [0.049922035, -0.095993537, 0.050612699, -0.004408786]
        a = [1, -2.494956002, 2.017265875, -0.522189400]
        x = signal.lfilter(b, a, x) * 6
    return x


def sos(kind, freq, order=2, q=None):
    nyq = SR / 2
    if kind == "bp":
        lo, hi = freq
        return signal.butter(order, [max(lo, 20) / nyq, min(hi, nyq * 0.95) / nyq], "bandpass", output="sos")
    return signal.butter(order, min(freq, nyq * 0.95) / nyq, kind, output="sos")


def filt(x, kind, freq, order=2):
    return signal.sosfilt(sos(kind, freq, order), x, axis=-1)


def peak(x, freq, q):
    b, a = signal.iirpeak(min(freq, SR * 0.47), q, fs=SR)
    return signal.lfilter(b, a, x)


def modal(n, rng, freqs, decays, amps, exciter_len=0.002, bright=8000.0):
    """Bank of decaying sines (struck bodies: wood, skins, bells)."""
    t = np.arange(n) / SR
    out = np.zeros(n)
    for f, d, a in zip(freqs, decays, amps):
        if f >= SR * 0.45:
            continue
        ph = rng.uniform(0, 0.3)
        out += a * np.sin(2 * np.pi * f * t + ph) * np.exp(-6.9 * t / d)
    m = int(exciter_len * SR)
    if m > 4:
        burst = filt(noise(m, rng), "lp", bright) * np.hanning(2 * m)[m:]
        out[:m] += burst * 0.4 * np.max(np.abs(amps))
    return out


def metal_bank(n, scale, rng, ratios=None, shape="square"):
    """Six detuned pulse oscillators, the classic analog cymbal source."""
    ratios = ratios or [205.3, 304.4, 369.6, 522.7, 540.0, 800.0]
    t = np.arange(n) / SR
    out = np.zeros(n)
    for r in ratios:
        f = r * scale * rng.uniform(0.995, 1.005)
        ph = 2 * np.pi * f * t + rng.uniform(0, 2 * np.pi)
        out += np.sign(np.sin(ph)) if shape == "square" else np.sin(ph)
    return out / len(ratios)


# --------------------------------------------------------------- processing

def drive(x, amount):
    if amount <= 1.0:
        return x
    return np.tanh(x * amount) / np.tanh(amount)


def asym_drive(x, amount, bias=0.15):
    y = np.tanh((x + bias) * amount) - np.tanh(bias * amount)
    return y / (np.max(np.abs(y)) + 1e-12)


def bitcrush(x, rate=26040, bits=12):
    """Sample-and-hold rate reduction and requantization (vintage sampler feel)."""
    step = SR / rate
    idx = (np.floor(np.arange(x.shape[-1]) / step) * step).astype(int)
    y = x[..., idx]
    q = 2 ** (bits - 1)
    m = np.max(np.abs(y)) + 1e-12
    return np.round(y / m * q) / q * m


def tape(x, amount=1.5, lp=14000):
    return filt(drive(x, amount), "lp", lp, 1)


def room_ir(rng, length, decay, lp=7000, pre=0.008, early=6, stereo=True):
    n = int(length * SR)
    chans = []
    for c in range(2 if stereo else 1):
        r = np.random.default_rng(rng.integers(1 << 30))
        ir = r.standard_normal(n) * np.exp(-6.9 * np.arange(n) / SR / decay)
        ir = filt(ir, "lp", lp, 2)
        ir = filt(ir, "hp", 180, 1)
        for _ in range(early):
            k = int(r.uniform(0.002, 0.03) * SR)
            if k < n:
                ir[k] += r.uniform(-1, 1) * 2.5
        p = int(pre * SR)
        ir = np.concatenate([np.zeros(p), ir])
        chans.append(ir / np.sqrt(np.sum(ir ** 2)))
    return np.array(chans)


def add_room(x, rng, mix, length=0.8, decay=0.6, lp=7000, pre=0.008):
    """Mono dry signal plus stereo generated room. Returns (2, n)."""
    ir = room_ir(rng, length, decay, lp, pre)
    wet = np.array([signal.fftconvolve(x, ir[c]) for c in range(2)])
    dry = np.zeros_like(wet)
    dry[:, : len(x)] = x
    wet *= np.max(np.abs(x)) / (np.max(np.abs(wet)) + 1e-12)
    return dry + mix * wet


def widen(x, rng, amount=0.3, delay_ms=7):
    """Turn mono into stereo with a short decorrelated delay on the side channel."""
    d = int(delay_ms / 1000 * SR)
    side = np.concatenate([np.zeros(d), x])[: len(x)]
    side = filt(side, "hp", 400, 1) * amount
    return np.array([x + side, x - side])


def vinyl(n, rng, level=0.004):
    hiss = filt(noise(n, rng), "bp", (800, 9000)) * level
    crackle = np.zeros(n)
    for _ in range(int(n / SR * 25)):
        k = rng.integers(0, n)
        crackle[k] = rng.uniform(-1, 1) * level * 12
    crackle = filt(crackle, "hp", 1500, 1)
    return hiss + crackle


# ---------------------------------------------------------------- finishing

TARGET_TP = 10 ** (-0.3 / 20)  # true peak ceiling, -0.3 dBTP


def finish(x, tail_db=-66.0, fade_ms=8.0, max_len=None):
    x = np.atleast_2d(np.asarray(x, dtype=np.float64))
    x = signal.sosfilt(sos("hp", 9.0, 1), x, axis=-1)  # remove DC, keep 808 sub
    x[np.abs(x) < 1e-12] = 0

    # trim tail where the smoothed envelope stays below tail_db for good
    env = np.max(np.abs(x), axis=0)
    win = int(0.005 * SR)
    smooth = np.convolve(env, np.ones(win) / win, mode="same")
    thresh = np.max(smooth) * 10 ** (tail_db / 20)
    above = np.nonzero(smooth > thresh)[0]
    end = above[-1] + win if len(above) else x.shape[1]
    if max_len:
        end = min(end, int(max_len * SR))
    x = x[:, :end]

    fi = 12  # 0.25 ms fade-in: no DC step, keeps the transient
    x[:, :fi] *= np.linspace(0, 1, fi)
    fo = min(int(fade_ms / 1000 * SR), x.shape[1] // 3)
    x[:, -fo:] *= (np.cos(np.linspace(0, np.pi, fo)) * 0.5 + 0.5)

    tp = np.max(np.abs(signal.resample_poly(x, 4, 1, axis=-1)))
    x *= TARGET_TP / (tp + 1e-12)
    if x.shape[0] == 2 and np.max(np.abs(x[0] - x[1])) < 1e-6:
        x = x[:1]
    return x
