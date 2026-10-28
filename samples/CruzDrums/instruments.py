"""Cruz Drums: the six drum voices, each built from layered recipes.

Every voice picks a recipe (a set of layered sources), runs the knock chain, then the
genre's colour. Returns (audio, descriptor words, note or None).
"""

import numpy as np

from drumsynth import SR, decay_env, filt, noise, note_freq, note_name, sweep_phase
from organic import (box_hit, clap_group, contact, cymbal, glass_tap, grains, keys, membrane,
                     metal_tap, modes, mouth, snap, table_slam, wires)
from process import console, eq, finish_color, knock_chain, loud, norm, room, transient


def U(rng, r):
    return rng.uniform(*r) if isinstance(r, (tuple, list)) else r


def pick(rng, weights):
    names = list(weights)
    p = np.array([weights[k] for k in names], float)
    return names[rng.choice(len(names), p=p / p.sum())]


def sub(n, f0, decay, sweep=2.5, tau=0.02, hold=0.0):
    x = np.sin(sweep_phase(n, f0, f0 * sweep, tau))
    return x * decay_env(n, decay, attack=0.0004, hold=hold)


def add_room(x, rng, st, key, default=(0.0, 0.2)):
    mix = U(rng, st.get(key, default))
    if mix > 0.04:
        return room(x, rng, mix, size=U(rng, st.get("room_size", (0.2, 0.6))),
                    damp=U(rng, st.get("room_damp", (5000, 9000)))), mix
    return x, 0.0


def space_word(mix):
    return "Dry" if mix < 0.1 else "Roomy" if mix < 0.3 else "Big Room"


# ------------------------------------------------------------------ kicks

def kick(rng, st, i):
    recipe = pick(rng, st["kick"])
    f0 = note_freq(int(rng.integers(*st.get("kick_notes", (31, 37)))))
    if recipe == "808":
        decay = U(rng, st.get("k808_decay", (1.0, 2.2)))
        n = int((decay + 0.1) * SR)
        body = sub(n, f0, decay, U(rng, (2.0, 3.5)), 0.03, hold=U(rng, (0.05, 0.2)))
        body = console(body, U(rng, (2, 6)))
        top = norm(membrane(n, rng, U(rng, (70, 110)), 0.08, 0.4, 0.01, 6, damp=2)) * U(rng, (0.25, 0.5))
        x = body + top + contact(n, rng, 0.6, 0.002) * 0.4
        x = eq(x, "peak", U(rng, (110, 200)), U(rng, (2, 5)), 1.0)
        x = console(norm(x), U(rng, (1.5, 3)))
        words = ["808", "Long" if decay < 1.5 else "Boomy"]
    else:
        decay = U(rng, st.get("kick_decay", (0.25, 0.5)))
        n = int((decay + 0.25) * SR)
        s = sub(n, f0, decay, U(rng, (2.0, 3.5)), U(rng, (0.012, 0.03)))
        if recipe == "Knock":
            head = membrane(n, rng, U(rng, (55, 85)), decay * 0.45, U(rng, (0.3, 0.8)), 0.015, 9,
                            pos=U(rng, (0.1, 0.4)), damp=2)
            x = s * U(rng, (0.7, 1.0)) + norm(head) * U(rng, (0.5, 0.9)) \
                + contact(n, rng, U(rng, (0.4, 0.9)), U(rng, (0.001, 0.003))) * U(rng, (0.5, 1.1))
        elif recipe == "Thump":
            head = membrane(n, rng, U(rng, (48, 66)), decay * 0.8, 0.25, 0.02, 10, pos=U(rng, (0.2, 0.6)))
            x = norm(head) + s * 0.5 + contact(n, rng, 0.1, 0.006) * 0.6
        elif recipe == "Slam":
            x = norm(table_slam(n, rng, low=f0)) * 0.8 + s * U(rng, (0.6, 0.9))
        elif recipe == "Box":
            x = norm(box_hit(n, rng)) * 0.7 + s
        elif recipe == "Mouth":
            x = norm(mouth(n, rng, "b")) + s * 0.7
        else:  # Punch
            head = membrane(n, rng, U(rng, (70, 100)), 0.06, 0.6, 0.008, 6, damp=2)
            x = s + norm(head) * 0.4 + contact(n, rng, U(rng, (0.7, 1.0)), 0.0015) * U(rng, (0.8, 1.4))
            x = transient(x, U(rng, (1, 3)))
        words = [{"Slam": "Table Slam"}.get(recipe, recipe)]
        x = knock_chain(x, rng, st, bright=False)
        x = eq(x, "peak", U(rng, (90, 180)), U(rng, (1, 4)), 1.0)
    x = finish_color(x, rng, st)
    x, mix = add_room(x, rng, st, "kick_room", (0.0, 0.0))
    x = loud(x, rng, st)
    if mix:
        words.append(space_word(mix))
    return x, words, note_name(f0)


# ----------------------------------------------------------------- snares

def acoustic_snare(n, rng, f0, decay, rim=False):
    head = membrane(n, rng, f0, decay, U(rng, (0.05, 0.2)), 0.01, 14, damp=U(rng, (0.6, 1.4)))
    w = wires(n, rng, head, f0, tight=U(rng, (0.06, 0.2)), buzz=U(rng, (0.2, 0.8)))
    x = norm(head) + norm(w) * U(rng, (0.6, 1.3))
    x += contact(n, rng, U(rng, (0.6, 1.0)), U(rng, (0.0008, 0.002))) * U(rng, (0.8, 1.8))
    x += norm(modes(n, rng, 300, 900, 4, (0.03, 0.08))) * 0.25
    if rim:
        x += norm(modes(n, rng, 900, 4500, 6, (0.02, 0.07))) * U(rng, (0.4, 0.8))
    return x


def snare(rng, st, i):
    recipe = pick(rng, st["snare"])
    n = int(1.0 * SR)
    f0 = U(rng, st.get("snare_tune", (170, 250)))
    decay = U(rng, (0.12, 0.3))
    base = acoustic_snare(n, rng, f0, decay, rim=recipe == "Rimshot")
    if recipe in ("Acoustic", "Rimshot"):
        x = base
    elif recipe == "Crack":
        f0 *= 1.25
        x = acoustic_snare(n, rng, f0, decay * 0.7, rim=True)
        st = dict(st, drive1=(3, 6), drive2=(2, 4), presence=(4, 8))
    elif recipe == "Layered":
        x = norm(base) + norm(clap_group(n, rng, int(rng.integers(2, 5)))) * U(rng, (0.4, 0.8))
    elif recipe == "Slam":
        x = norm(table_slam(n, rng, 120)) + norm(wires(n, rng, base, f0)) * 0.6 \
            + norm(clap_group(n, rng, 3)) * U(rng, (0.3, 0.6))
    elif recipe == "Box":
        x = norm(box_hit(n, rng)) + norm(snap(n, rng)) * 0.6 + norm(wires(n, rng, base, f0)) * 0.7
    elif recipe == "Mouth":
        x = norm(mouth(n, rng, "k")) + norm(base) * U(rng, (0.3, 0.6))
    else:  # Clapper: snare, clap and snaps stacked
        x = norm(base) + norm(clap_group(n, rng, int(rng.integers(3, 7)))) * 0.8 + norm(snap(n, rng)) * 0.5
    x = knock_chain(x, rng, st)
    x = finish_color(x, rng, st)
    x, mix = add_room(x, rng, st, "snare_room", (0.1, 0.35))
    x = loud(x, rng, st)
    words = [{"Slam": "Table Slam"}.get(recipe, recipe)]
    if f0 < 195 or f0 > 240:
        words.append("Fat" if f0 < 195 else "High")
    if st.get("crush"):
        words.append("Dusty")
    words.append(space_word(mix))
    return x, words, None


# ------------------------------------------------------------------ claps

def clap(rng, st, i):
    recipe = pick(rng, st["clap"])
    n = int(0.9 * SR)
    if recipe == "Group":
        x = clap_group(n, rng, int(rng.integers(4, 9)), spread=U(rng, (0.015, 0.035)))
    elif recipe == "Tight":
        x = clap_group(n, rng, int(rng.integers(1, 3)), spread=0.006)
    elif recipe == "Snap Stack":
        x = norm(clap_group(n, rng, int(rng.integers(2, 5)))) + norm(snap(n, rng)) * 0.7
        k = int(U(rng, (0.004, 0.012)) * SR)
        x[k:] += norm(snap(n - k, rng)) * 0.5
    elif recipe == "Slap":
        x = norm(clap_group(n, rng, 2)) + norm(modes(n, rng, 150, 500, 8, (0.02, 0.06))) * 0.6
    else:  # Mouth
        x = norm(mouth(n, rng, "pf")) + norm(clap_group(n, rng, 3)) * 0.6
    x = knock_chain(x, rng, st)
    x = finish_color(x, rng, st)
    x, mix = add_room(x, rng, st, "clap_room", (0.1, 0.4))
    x = loud(x, rng, st)
    words = [recipe]
    if st.get("crush"):
        words.append("Dusty")
    words.append(space_word(mix))
    return x, words, None


# ------------------------------------------------------------------- rims

def rim(rng, st, i):
    recipe = pick(rng, st["rim"])
    n = int(0.5 * SR)
    f = None
    if recipe == "Rimshot":
        f = U(rng, (220, 320))
        x = norm(membrane(n, rng, f, 0.08, 0.1, 0.005, 12)) * 0.6 \
            + norm(modes(n, rng, 900, 5000, 8, (0.02, 0.08))) + contact(n, rng, 1.0, 0.0008) * 1.5
    elif recipe == "Cross Stick":
        f = U(rng, (450, 1100))
        x = norm(modes(n, rng, f, f * 4, 6, (0.02, 0.06))) \
            + norm(membrane(n, rng, U(rng, (160, 240)), 0.05, 0.05, 0.01, 6)) * 0.4 + contact(n, rng, 0.8, 0.001)
    elif recipe == "Knuckle":
        x = norm(table_slam(n, rng, 200)) * 0.5 + norm(modes(n, rng, 400, 2000, 12, (0.015, 0.05)))
    elif recipe == "Glass":
        x = glass_tap(n, rng)
    elif recipe == "Can":
        x = metal_tap(n, rng)
    elif recipe == "Stick Click":
        f = U(rng, (1500, 3000))
        x = norm(modes(n, rng, f, f * 2.8, 5, (0.01, 0.035))) + contact(n, rng, 0.9, 0.0008) * 0.8
    else:  # Tongue
        x = mouth(n, rng, "click")
    x = knock_chain(x, rng, dict(st, drive1=(1.2, 2.5), drive2=(1.1, 2.0)))
    x = finish_color(x, rng, st)
    x, mix = add_room(x, rng, st, "rim_room", (0.0, 0.25))
    x = loud(x, rng, st)
    return x, [recipe, space_word(mix)], note_name(f) if f else None


# ------------------------------------------------------------------- hats

def hat(rng, st, i):
    recipe = pick(rng, st["hat"])
    closed_d = U(rng, st.get("hat_closed", (0.03, 0.09)))
    open_d = U(rng, st.get("hat_open", (0.3, 0.7)))
    if recipe == "Closed":
        n = int((closed_d + 0.05) * SR)
        x = cymbal(n, rng, closed_d, True, U(rng, (0.8, 1.3)))
    elif recipe == "Open":
        n = int((open_d * 1.8 + 0.1) * SR)
        x = cymbal(n, rng, open_d, False, U(rng, (0.8, 1.3)))
    elif recipe == "Pedal":
        n = int(0.15 * SR)
        x = cymbal(n, rng, 0.03, True, 0.7)
        x += filt(noise(n, rng), "bp", (500, 2000)) * decay_env(n, 0.025) * 0.8
    elif recipe == "Shaker":
        n = int(0.4 * SR)
        t = np.arange(n) / SR
        a, d = U(rng, (0.01, 0.03)), U(rng, (0.05, 0.14))
        env = np.where(t < a, t / a, np.exp(-6.9 * (t - a) / d))
        x = grains(n, rng, 9000 * env, (0.2, 0.8), (3500, 15000))
    elif recipe == "Mouth":
        n = int(0.35 * SR)
        x = mouth(n, rng, "ts")
    elif recipe == "Keys":
        n = int(0.4 * SR)
        x = keys(n, rng)
    else:  # Sizzle: open plates with a loose rivet rattle
        n = int((open_d * 1.8 + 0.1) * SR)
        t = np.arange(n) / SR
        x = norm(cymbal(n, rng, open_d, False)) + grains(n, rng, 3000 * np.exp(-t / open_d), (0.2, 0.5), (5000, 15000)) * 0.4
    x = filt(x, "hp", U(rng, (1500, 3000)), 2)
    x = knock_chain(x, rng, dict(st, drive1=(1.1, 2.2), drive2=(1.0, 1.8), presence=(0, 3)))
    x = finish_color(x, rng, st)
    mix = 0.0
    if recipe in ("Open", "Sizzle"):
        x, mix = add_room(x, rng, st, "hat_room", (0.0, 0.15))
    x = loud(x, rng, dict(st, clip=(1.0, 1.3)))
    words = [recipe]
    if recipe == "Closed":
        words.append("Tight" if closed_d < 0.05 else "Short")
    if st.get("crush"):
        words.append("Dusty")
    return x, words, None


# ------------------------------------------------------------------ percs

def perc_voice(kind, rng):
    n = int(1.2 * SR)
    t = np.arange(n) / SR
    if kind == "Conga":
        f = U(rng, (170, 330))
        x = membrane(n, rng, f, 0.3, 0.06, 0.02, 10, pos=U(rng, (0.2, 0.7)))
        return x + contact(n, rng, 0.4, 0.002) * 0.5, f
    if kind == "Bongo":
        f = U(rng, (380, 620))
        return membrane(n, rng, f, 0.16, 0.05, 0.01, 10) + contact(n, rng, 0.5, 0.0015) * 0.5, f
    if kind == "Tom":
        f = U(rng, (80, 200))
        return membrane(n, rng, f, U(rng, (0.3, 0.6)), 0.25, 0.03, 10) + contact(n, rng, 0.7, 0.0015) * 0.7, f
    if kind == "Tambourine":
        x = norm(keys(n, rng)) + norm(membrane(n, rng, U(rng, (250, 400)), 0.06, 0.05, 0.01, 6)) * 0.3
        return x + grains(n, rng, 5000 * np.exp(-t / 0.1), (0.2, 0.5), (5000, 15000)) * 0.3, None
    if kind == "Shaker":
        a, d = U(rng, (0.015, 0.04)), U(rng, (0.06, 0.15))
        env = np.where(t < a, t / a, np.exp(-6.9 * (t - a) / d))
        return grains(n, rng, 8000 * env, (0.2, 1.0), (3000, 14000)), None
    if kind == "Cowbell":
        s = U(rng, (0.85, 1.25))
        x = modes(n, rng, 520 * s, 560 * s, 2, (0.2, 0.4)) * 2 + modes(n, rng, 790 * s, 830 * s, 2, (0.15, 0.3)) * 2
        x += modes(n, rng, 1500, 4500, 6, (0.02, 0.08))
        return x + contact(n, rng, 0.9, 0.001) * 0.4, 540 * s
    if kind == "Woodblock":
        f = U(rng, (600, 1400))
        return modes(n, rng, f, f * 3, 4, (0.03, 0.09)) + contact(n, rng, 0.8, 0.001) * 0.4, f
    if kind == "Clave":
        f = U(rng, (2000, 2800))
        return modes(n, rng, f, f * 1.02, 1, (0.05, 0.09)) * 2 + contact(n, rng, 1.0, 0.0006) * 0.4, f
    if kind == "Guiro":
        rate = np.zeros(n)
        k = 0
        for _ in range(int(rng.integers(8, 15))):
            m = int(0.004 * SR)
            rate[k:k + m] = 30000
            k += int(U(rng, (0.008, 0.014)) * SR)
        return grains(n, rng, rate, (0.2, 0.6), (1800, 7000)), None
    if kind == "Triangle":
        f = U(rng, (1100, 2200))
        x = sum(np.sin(2 * np.pi * f * r * t) * np.exp(-6.9 * t / d) * a
                for r, d, a in [(1, 1.1, 1), (2.76, 0.8, .5), (5.4, 0.5, .3), (8.93, 0.3, .2)])
        return x + contact(n, rng, 1.0, 0.0006) * 0.3, f
    if kind == "Can":
        return metal_tap(n, rng), None
    if kind == "Bottle":
        return glass_tap(n, rng), None
    if kind == "Box Tap":
        return box_hit(n, rng), None
    if kind == "Knuckle":
        return table_slam(n, rng, 180), None
    if kind == "Snap":
        return snap(n, rng), None
    if kind == "Tongue Click":
        return mouth(n, rng, "click"), None
    if kind == "Lip Pop":
        return filt(mouth(n, rng, "b"), "hp", 150, 2), None
    if kind == "Paper":
        a = U(rng, (0.02, 0.08))
        return grains(n, rng, 6000 * np.exp(-6.9 * t / a), (0.3, 1.5), (800, 9000)), None
    if kind == "Keys":
        return keys(n, rng), None
    raise ValueError(kind)


def perc(rng, st, i):
    kinds = st["percs"]
    kind = kinds[i % len(kinds)]
    x, f = perc_voice(kind, rng)
    x = knock_chain(x, rng, dict(st, drive1=(1.2, 2.5), drive2=(1.1, 2.0), presence=(1, 4)))
    x = finish_color(x, rng, st)
    x, mix = add_room(x, rng, st, "perc_room", (0.0, 0.3))
    x = loud(x, rng, st)
    return x, [kind, space_word(mix)], note_name(f) if f else None


VOICES = {"Kicks": kick, "Snares": snare, "Claps": clap, "Rims": rim, "Hats": hat, "Percs": perc}
