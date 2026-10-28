"""Cruz Drums: the six drum voices. Each returns (audio, descriptor words, note or None)."""

import numpy as np

from drumsynth import (SR, add_room, asym_drive, bitcrush, decay_env, drive, filt,
                       metal_bank, modal, noise, note_freq, note_name, peak, sweep_phase,
                       tape, vinyl, widen)


def U(rng, r):
    return rng.uniform(*r) if isinstance(r, (tuple, list)) else r


def vintage(x, rng, s):
    """Genre colour applied last: sampler crush, tape, dusty top end, vinyl."""
    if s.get("crush"):
        x = bitcrush(x, rate=rng.choice([26040, 22050, 32000]), bits=int(rng.choice([12, 12, 14])))
        x = filt(x, "lp", U(rng, s.get("crush_lp", (8000, 11000))), 2)
    if s.get("tape"):
        x = tape(x, U(rng, (1.2, 2.0)), U(rng, (12000, 16000)))
    if s.get("vinyl") and x.ndim == 1:
        env = np.maximum.accumulate(np.abs(x)[::-1])[::-1]
        x = x + vinyl(len(x), rng, 0.002) * env
    return x


# ------------------------------------------------------------------ kicks

def kick(rng, s, i):
    is808 = rng.random() < s.get("kick_808", 0.0)
    midi = int(rng.integers(*(s["kick_808_notes"] if is808 else s["kick_notes"])))
    f0 = note_freq(midi)
    decay = U(rng, s["kick_808_decay"] if is808 else s["kick_decay"])
    dur = decay + 0.05
    n = int(dur * SR)
    sweep = U(rng, s["kick_sweep"]) * (0.8 if is808 else 1.0)
    tau = U(rng, s["kick_tau"])
    ph = sweep_phase(n, f0, f0 * sweep, tau, tau2=0.002, start2=f0 * sweep * 0.6)
    body = np.sin(ph)
    hold = U(rng, (0.0, 0.03)) if not is808 else U(rng, (0.05, 0.25))
    env = decay_env(n, decay, attack=0.0004, hold=hold, curve=U(rng, (0.8, 1.3)))
    x = body * env

    click = U(rng, s["kick_click"])
    if click > 0.02:
        m = int(U(rng, (0.002, 0.006)) * SR)
        c = filt(noise(m, rng), "bp", (U(rng, (1500, 3500)), U(rng, (7000, 12000))))
        c *= np.exp(-np.arange(m) / (m / 4))
        x[:m] += c * click * 0.8
    knock = U(rng, s.get("kick_knock", 0.0))
    if knock > 0.02:
        k = peak(noise(n, rng), U(rng, (110, 220)), 6) * decay_env(n, 0.07) * knock * 3
        x += k
    amt = U(rng, s["kick_808_drive"] if is808 else s["kick_drive"])
    x = asym_drive(x, amt, bias=0.08) if amt > 1 else x
    x = filt(x, "lp", U(rng, s.get("kick_lp", (12000, 18000))), 2)
    x = vintage(x, rng, s)

    words = []
    if is808:
        words.append("808")
        words.append("Long" if decay < 1.6 else "Boomy")
    else:
        words.append("Tight" if decay < 0.3 else "Punchy" if decay < 0.48 else "Round")
    words.append("Clean" if amt < 1.6 else "Warm" if amt < 3 else "Driven" if amt < 4.8 else "Distorted")
    if click > 0.5:
        words.append("Clicky")
    if knock > 0.3:
        words.append("Knock")
    if s.get("crush"):
        words.append("Dusty")
    return x, words, note_name(f0)


# ----------------------------------------------------------------- snares

def snare_core(rng, s):
    dur = 0.9
    n = int(dur * SR)
    f1 = U(rng, s["snare_tone"])
    bd = U(rng, (0.07, 0.18))
    ph1 = sweep_phase(n, f1, f1 * 1.35, 0.008)
    ph2 = sweep_phase(n, f1 * U(rng, (1.5, 1.75)), f1 * 2.1, 0.006)
    body = (np.sin(ph1) + 0.5 * np.sin(ph2)) * decay_env(n, bd, curve=1.2)

    nd = U(rng, s["snare_noise_decay"])
    nz = noise(n, rng)
    nz = filt(nz, "hp", U(rng, s["snare_hp"]), 2)
    nz = filt(nz, "lp", U(rng, (9000, 15000)), 2)
    ring = rng.random() < s.get("snare_ring", 0.3)
    if ring:
        nz += peak(nz, U(rng, (2500, 5000)), 4) * 1.2
    nz *= decay_env(n, nd, attack=0.0008, curve=U(rng, (0.7, 1.1)))

    crack = filt(noise(int(0.004 * SR), rng), "hp", 3000) * np.linspace(1, 0, int(0.004 * SR))
    mix = U(rng, (0.45, 0.75))
    x = body * (1 - mix) * 1.6 + nz * mix
    x[: len(crack)] += crack * 0.5
    return x, f1, nd, ring


def clap_core(rng, s, layers=1):
    n = int(1.0 * SR)
    out = np.zeros(n)
    center = U(rng, s["clap_center"])
    for L in range(layers):
        off = int(U(rng, (0, 0.012)) * SR) if L else 0
        bursts = int(rng.integers(3, 6))
        gap = U(rng, (0.006, 0.013))
        src = filt(noise(n, rng), "bp", (center * U(rng, (0.55, 0.75)), center * U(rng, (1.6, 2.6))), 2)
        env = np.zeros(n)
        t = np.arange(n) / SR
        for b in range(bursts):
            t0 = b * gap * U(rng, (0.8, 1.2))
            env += (t >= t0) * np.exp(-np.clip(t - t0, 0, None) / U(rng, (0.0025, 0.005))) * (0.75 + 0.25 * rng.random())
        t0 = bursts * gap
        tail = U(rng, s["clap_tail"])
        env += (t >= t0) * np.exp(-6.9 * np.clip(t - t0, 0, None) / tail)
        layer = src * np.minimum(env, 1.2)
        out[off:] += layer[: n - off] * (1.0 if L == 0 else U(rng, (0.5, 0.9)))
    return out, center


def snare(rng, s, i):
    x, f1, nd, ring = snare_core(rng, s)
    layered = rng.random() < s.get("snare_clap", 0.0)
    if layered:
        c, _ = clap_core(rng, s, 1)
        c = c[: len(x)]
        x = x + c * U(rng, (0.3, 0.6)) * np.max(np.abs(x)) / (np.max(np.abs(c)) + 1e-9)
    amt = U(rng, s["snare_drive"])
    x = drive(x / np.max(np.abs(x)), amt)
    x = vintage(x, rng, s)
    room = U(rng, s["snare_room"])
    if room > 0.03:
        x = add_room(x, rng, room, length=0.9, decay=U(rng, s.get("room_decay", (0.35, 0.7))),
                     lp=U(rng, s.get("room_lp", (5000, 9000))))
    words = ["Fat" if f1 < 200 else "Tight" if f1 > 240 else "Snappy"]
    if nd > 0.3:
        words.append("Long")
    if layered:
        words.append("Layered")
    if amt > 2.2:
        words.append("Crunchy")
    if ring:
        words.append("Ringy")
    words.append("Dry" if room < 0.12 else "Roomy" if room < 0.3 else "Big Room")
    if s.get("crush"):
        words.append("Dusty")
    return x, words, None


# ------------------------------------------------------------------ claps

def clap(rng, s, i):
    layers = int(rng.integers(*s["clap_layers"]))
    x, center = clap_core(rng, s, layers)
    amt = U(rng, s.get("clap_drive", (1.0, 2.0)))
    x = drive(x / np.max(np.abs(x)), amt)
    x = vintage(x, rng, s)
    room = U(rng, s["clap_room"])
    if room > 0.03:
        x = add_room(x, rng, room, length=1.2, decay=U(rng, s.get("room_decay", (0.4, 0.9))),
                     lp=U(rng, s.get("room_lp", (6000, 10000))))
    elif layers > 1:
        x = widen(x, rng, 0.35)
    words = ["Dark" if center < 1100 else "Bright" if center > 1500 else "Warm"]
    words.append("Single" if layers == 1 else "Stacked" if layers >= 3 else "Double")
    if amt > 2:
        words.append("Gritty")
    words.append("Dry" if room < 0.12 else "Roomy" if room < 0.3 else "Big Room")
    return x, words, None


# ------------------------------------------------------------------- rims

def rim(rng, s, i):
    kinds = s.get("rim_kinds", ["808", "wood", "click"])
    kind = kinds[i % len(kinds)]
    n = int(0.6 * SR)
    if kind == "808":
        f = U(rng, (1500, 1900))
        f2 = U(rng, (420, 520))
        t = np.arange(n) / SR
        x = (np.sin(2 * np.pi * f * t) * np.exp(-6.9 * t / U(rng, (0.02, 0.045)))
             + 0.7 * np.sin(2 * np.pi * f2 * t) * np.exp(-6.9 * t / U(rng, (0.03, 0.06))))
        x = filt(asym_drive(x, U(rng, (2, 4))), "hp", 300)
        word, note = "Electro", note_name(f)
    elif kind == "wood":
        f = U(rng, (380, 900))
        x = modal(n, rng, [f, f * 2.32, f * 3.7, f * 5.9], [0.07, 0.04, 0.025, 0.012],
                  [1, 0.6, 0.35, 0.2], exciter_len=0.003, bright=9000)
        x += filt(noise(n, rng), "hp", 2500) * decay_env(n, 0.03) * 0.5
        word, note = "Cross Stick", note_name(f)
    else:
        f = U(rng, (2200, 4200))
        x = modal(n, rng, [f, f * 1.48, f * 2.6], [0.03, 0.02, 0.012], [1, 0.5, 0.3],
                  exciter_len=0.0015, bright=14000)
        x += filt(noise(n, rng), "bp", (1800, 6000)) * decay_env(n, 0.02) * 0.6
        word, note = "Crisp", note_name(f)
    x = vintage(x / np.max(np.abs(x)), rng, s)
    room = U(rng, s.get("rim_room", (0.05, 0.25)))
    if room > 0.06:
        x = add_room(x, rng, room, length=0.6, decay=U(rng, (0.25, 0.5)), lp=U(rng, (6000, 9000)))
    return x, [word, "Dry" if room < 0.12 else "Roomy"], note


# ------------------------------------------------------------------- hats

def hat(rng, s, i):
    is_open = (i % 10) < int(round(s.get("open_frac", 0.4) * 10))
    decay = U(rng, s["hat_open"] if is_open else s["hat_closed"])
    n = int((decay + 0.05) * SR)
    acoustic = rng.random() < s.get("hat_acoustic", 0.0)
    scale = U(rng, s.get("hat_scale", (1.0, 1.6)))
    metal = metal_bank(n, scale, rng)
    if acoustic:
        nz = noise(n, rng)
        x = sum(peak(nz, rng.uniform(3500, 13000), rng.uniform(3, 8)) for _ in range(6)) * 0.5
        x += metal * 0.35 + nz * 0.15
        x = filt(x, "hp", U(rng, (3500, 6000)), 2)
        x[: int(0.002 * SR)] += filt(noise(int(0.002 * SR), rng), "bp", (1500, 5000)) * 0.8  # stick tick
    else:
        mix = U(rng, s.get("hat_metal", (0.5, 0.8)))
        src = metal * mix + noise(n, rng) * (1 - mix) * 0.6
        x = filt(src, "bp", (U(rng, (6000, 8000)), 16000), 2)
        x = filt(x, "hp", U(rng, s.get("hat_hp", (6500, 8500))), 2)
    env = decay_env(n, decay, attack=0.0003, curve=U(rng, (0.7, 1.2) if is_open else (1.0, 1.6)))
    if is_open and rng.random() < 0.4:  # choked-sizzle shape
        env *= 1 - 0.3 * np.exp(-np.arange(n) / SR / 0.01)
    x = x * env
    amt = U(rng, s.get("hat_drive", (1.0, 2.0)))
    x = drive(x / np.max(np.abs(x)), amt)
    x = vintage(x, rng, s)
    room = U(rng, s.get("hat_room", (0.0, 0.15))) if is_open else 0.0
    if room > 0.05:
        x = add_room(x, rng, room, length=0.6, decay=0.4, lp=12000)
    words = ["Open" if is_open else "Closed"]
    words.append(("Tight" if decay < 0.045 else "Short") if not is_open else ("Long" if decay > 0.45 else "Medium"))
    if acoustic:
        words.append("Live")
    words.append("Shimmer" if scale > 1.45 else "Crisp" if scale > 1.15 else "Metallic")
    if amt > 1.7:
        words.append("Sizzly")
    if s.get("crush"):
        words.append("Dusty")
    return x, words, None


# ------------------------------------------------------------------ percs

def _tone(n, rng, f, start, tau, decay):
    return np.sin(sweep_phase(n, f, f * start, tau)) * decay_env(n, decay)


def perc_voice(kind, rng):
    n = int(1.6 * SR)
    t = np.arange(n) / SR
    if kind == "conga":
        f = rng.uniform(170, 330)
        x = modal(n, rng, [f, f * 1.52, f * 2.07, f * 2.68], [0.3, 0.12, 0.07, 0.05], [1, .4, .25, .15])
        x += _tone(n, rng, f, 1.15, 0.01, 0.25) * 0.6
        x += filt(noise(n, rng), "bp", (1200, 5000)) * decay_env(n, 0.03) * rng.uniform(0.2, 0.7)
        return x, f, "Conga"
    if kind == "bongo":
        f = rng.uniform(380, 620)
        x = modal(n, rng, [f, f * 1.6, f * 2.2], [0.15, 0.07, 0.04], [1, .4, .2])
        x += filt(noise(n, rng), "bp", (2000, 7000)) * decay_env(n, 0.02) * 0.4
        return x, f, "Bongo"
    if kind == "tom":
        f = rng.uniform(80, 200)
        x = _tone(n, rng, f, rng.uniform(1.4, 2.0), 0.03, rng.uniform(0.3, 0.7))
        x += filt(noise(n, rng), "lp", 3000) * decay_env(n, 0.05) * 0.25
        return x, f, "Tom"
    if kind == "syndrum":
        f = rng.uniform(110, 260)
        x = np.sin(sweep_phase(n, f * 0.6, f * rng.uniform(2.0, 3.2), rng.uniform(0.08, 0.2)))
        x *= decay_env(n, rng.uniform(0.4, 0.9))
        return x, f, "Syndrum"
    if kind == "cowbell":
        s = rng.uniform(0.9, 1.2)
        x = (np.sign(np.sin(2 * np.pi * 540 * s * t)) + np.sign(np.sin(2 * np.pi * 800 * s * t))) * 0.5
        x = filt(x, "bp", (450, 3500), 2)
        x *= 0.6 * decay_env(n, 0.04) + 0.4 * decay_env(n, rng.uniform(0.25, 0.45))
        return x, 540 * s, "Cowbell"
    if kind == "agogo":
        f = rng.uniform(700, 1100)
        x = modal(n, rng, [f, f * 2.5, f * 4.1], [0.5, 0.25, 0.12], [1, .35, .15], bright=10000)
        return x, f, "Agogo"
    if kind == "clave":
        f = rng.uniform(2000, 2800)
        x = modal(n, rng, [f, f * 2.75], [0.07, 0.03], [1, .2], exciter_len=0.001)
        return x, f, "Clave"
    if kind == "woodblock":
        f = rng.uniform(600, 1400)
        x = modal(n, rng, [f, f * 2.6, f * 4.15], [0.08, 0.04, 0.02], [1, .5, .25], exciter_len=0.002)
        return x, f, "Woodblock"
    if kind == "timbale":
        f = rng.uniform(380, 650)
        x = modal(n, rng, [f, f * 1.59, f * 2.14, f * 2.3, f * 3.6], [0.4, 0.3, 0.2, 0.2, 0.1], [1, .6, .4, .3, .2])
        x += filt(noise(n, rng), "hp", 3000) * decay_env(n, 0.03) * 0.6
        return x, f, "Timbale"
    if kind == "triangle":
        f = rng.uniform(1100, 2200)
        x = modal(n, rng, [f, f * 2.76, f * 5.4, f * 8.93], [1.4, 1.0, 0.7, 0.4], [1, .5, .3, .2], exciter_len=0.0007)
        return x, f, "Triangle"
    if kind == "shaker":
        x = filt(noise(n, rng), "bp", (rng.uniform(3500, 5500), 13000), 2)
        a, d = rng.uniform(0.012, 0.035), rng.uniform(0.06, 0.14)
        env = np.where(t < a, (t / a) ** 2, np.exp(-6.9 * (t - a) / d))
        if rng.random() < 0.5:  # back-and-forth grains
            t2 = rng.uniform(0.06, 0.1)
            env += np.where(t > t2, np.exp(-6.9 * (t - t2) / d) * 0.5 * np.clip((t - t2) / a, 0, 1), 0)
        return x * env, None, "Shaker"
    if kind == "tambourine":
        x = np.zeros(n)
        for h in range(int(rng.integers(2, 4))):
            k = int(h * rng.uniform(0.006, 0.012) * SR)
            fs = rng.uniform(4000, 10000, 8)
            x[k:] += modal(n - k, rng, fs, rng.uniform(0.08, 0.25, 8), rng.uniform(0.3, 1, 8)) * (0.8 ** h)
        x += filt(noise(n, rng), "hp", 6000) * decay_env(n, 0.15) * 0.4
        return x, None, "Tambourine"
    if kind == "guiro":
        x = np.zeros(n)
        k = 0
        for j in range(int(rng.integers(8, 15))):
            m = int(0.004 * SR)
            x[k:k + m] += filt(noise(m, rng), "bp", (2000, 6000)) * np.hanning(m) * (0.6 + 0.4 * np.sin(j / 3))
            k += int(rng.uniform(0.008, 0.014) * SR)
        return x, None, "Guiro"
    if kind == "snap":
        x = filt(noise(n, rng), "bp", (rng.uniform(1300, 2000), rng.uniform(3500, 6000)), 2)
        x *= decay_env(n, rng.uniform(0.03, 0.07), attack=0.0005)
        x += _tone(n, rng, rng.uniform(1600, 2400), 1.0, 0.01, 0.015) * 0.5
        return x, None, "Snap"
    if kind == "pluck":
        f = note_freq(int(rng.integers(64, 82)))
        idx = rng.uniform(2, 6) * np.exp(-t / rng.uniform(0.02, 0.06))
        ratio = rng.choice([1.0, 2.0, 3.5, 1.41])
        x = np.sin(2 * np.pi * f * t + idx * np.sin(2 * np.pi * f * ratio * t)) * decay_env(n, rng.uniform(0.15, 0.45))
        return x, f, "Pluck"
    if kind == "metal":
        f = rng.uniform(500, 1500)
        r = np.sort(rng.uniform(1, 7, 6))
        r[0] = 1
        x = modal(n, rng, f * r, rng.uniform(0.15, 0.6, 6), rng.uniform(0.3, 1, 6), bright=12000)
        return x, f, "Metal Hit"
    if kind == "zap":
        x = np.sin(sweep_phase(n, rng.uniform(120, 250), rng.uniform(2500, 5000), rng.uniform(0.008, 0.02)))
        x *= decay_env(n, rng.uniform(0.08, 0.2))
        return x, None, "Zap"
    if kind == "tick":
        f = rng.uniform(1200, 3200)
        x = _tone(n, rng, f, 1.5, 0.002, rng.uniform(0.02, 0.05))
        x += filt(noise(n, rng), "hp", 5000) * decay_env(n, 0.01) * 0.4
        return x, f, "Tick"
    raise ValueError(kind)


def perc(rng, s, i):
    kinds = s["percs"]
    kind = kinds[i % len(kinds)]
    x, f, word = perc_voice(kind, rng)
    amt = U(rng, s.get("perc_drive", (1.0, 1.8)))
    x = drive(x / np.max(np.abs(x)), amt)
    x = vintage(x, rng, s)
    room = U(rng, s.get("perc_room", (0.05, 0.25)))
    if room > 0.06:
        x = add_room(x, rng, room, length=0.8, decay=U(rng, (0.3, 0.6)), lp=U(rng, (7000, 11000)))
    words = [word, "Dry" if room < 0.12 else "Roomy"]
    return x, words, note_name(f) if f else None


VOICES = {"Kicks": kick, "Snares": snare, "Claps": clap, "Rims": rim, "Hats": hat, "Percs": perc}
