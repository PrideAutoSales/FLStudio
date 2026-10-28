#!/usr/bin/env python3
"""Generates Source/FactoryPresets.cpp, the Cruz Synth factory bank.

Run from anywhere:  python3 plugins/CruzSynth/tools/make_presets.py

Each preset is built from a few sound-design helpers below, then written out
as a one-line recipe (see FactoryPresets.h for the format). Output levels are
evened out by tools/preset_gains.json, which the render test fills in.
"""
import json
import math
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "Source")
GAINS = os.path.join(HERE, "preset_gains.json")

# ---------------------------------------------------------------- constants
SAW, SQUARE, TRI, SINE = 2 / 3, 1.0, 1 / 3, 0.0       # positions in Basic Shapes
DIV = {"8bar": 0, "4bar": 1, "2bar": 2, "1bar": 3, "1/2": 4, "1/2T": 5, "1/4.": 6, "1/4": 7,
       "1/4T": 8, "1/8.": 9, "1/8": 10, "1/8T": 11, "1/16.": 12, "1/16": 13, "1/16T": 14, "1/32": 15}
LP12, LP24, LADDER, HP12, HP24, BP12, BP24, NOTCH, PEAK, COMBP, COMBM, VOWEL = range(12)
W_OFF, W_SYNC, W_BENDP, W_BENDM, W_ASYM, W_PWM, W_MIRROR, W_FLIP, W_QUANT, W_FM, W_AM, W_RM = range(12)
TUBE, SOFT, HARD, FOLD, CRUSH, DOWNS = range(6)
SINE_L, TRI_L, SAWUP_L, SAWDN_L, SQUARE_L, CUSTOM_L, SH_L, DRIFT_L = range(8)
FREE, RETRIG, ENVMODE = range(3)
TABLES = ["Basic Shapes", "Harmonic Sweep", "Pulse Width", "Vowel Morph", "Sync Sweep", "FM Growl",
          "Fold Sweep", "Digital Steps", "Organ Bars", "Breath", "Hollow Lead", "Glass Bell", "Sine", "Saw",
          "Talk Growl", "Metal FM", "Squash Saw", "Comb Saw", "Bit Grit", "Chord Stack"]
SOURCES = ["Env1", "Env2", "Env3", "Lfo1", "Lfo2", "Lfo3", "Lfo4", "Velocity", "NoteTrack", "ModWheel",
           "Aftertouch", "PitchBend", "Macro1", "Macro2", "Macro3", "Macro4", "NoteRandom"]
DESTS = ["OscACoarse", "OscAFine", "OscAPos", "OscADetune", "OscABlend", "OscAWarp", "OscAPhase", "OscAPan",
         "OscALevel", "OscBCoarse", "OscBFine", "OscBPos", "OscBDetune", "OscBBlend", "OscBWarp", "OscBPhase",
         "OscBPan", "OscBLevel", "SubLevel", "SubPan", "NoiseLevel", "NoisePan", "NoiseColor", "FltCutoff",
         "FltRes", "FltDrive", "FltMix", "Lfo1Rate", "Lfo2Rate", "Lfo3Rate", "Lfo4Rate", "MasterVol"]


def param_ids():
    """Every parameter ID createParameterLayout() makes, to catch typos here."""
    text = open(os.path.join(SRC, "Parameters.cpp")).read()
    ids = set(re.findall(r'add<AP\w>\s*\(L,\s*"(\w+)"', text))
    osc = re.findall(r'id::osc \(o, "(\w+)"\)', text)
    env = re.findall(r'id::env \(e, "(\w+)"\)', text)
    lfo = re.findall(r'id::lfo \(l, "(\w+)"\)', text)
    ids |= {f"osc{o}_{n}" for o in "AB" for n in osc}
    ids |= {f"env{e}_{n}" for e in "123" for n in env}
    ids |= {f"lfo{l}_{n}" for l in "1234" for n in lfo}
    ids |= {f"macro{m}" for m in "1234"}
    return ids


# ---------------------------------------------------------------- preset object
class P:
    def __init__(self, cat, name):
        self.cat, self.name = cat, name
        self.v, self.mods, self.tables, self.lfo_pts, self.fx = {}, [], {}, {}, None

    def set(self, **kw):
        self.v.update(kw)
        return self

    def table(self, osc, name):
        assert name in TABLES, name
        self.tables[osc] = name
        return self

    def mod(self, src, dst, amt, bipolar=False):
        assert src in SOURCES and dst in DESTS, (src, dst)
        self.mods.append((src, dst, amt, bipolar))
        return self

    def lfo_shape(self, lfo, pts):
        self.lfo_pts[lfo] = pts
        self.v[f"lfo{lfo}_shape"] = CUSTOM_L
        return self

    def recipe(self, valid):
        assert len(self.mods) <= 16, self.name
        parts = []
        for osc, t in self.tables.items():
            parts.append(f"{osc}={t.replace(' ', '_')}")
        for k, val in self.v.items():
            assert k in valid, (self.name, k)
            if isinstance(val, bool):
                val = int(val)
            parts.append(f"{k}={round(val, 4) if isinstance(val, float) else val}")
        for src, dst, amt, bip in self.mods:
            parts.append(f"m={src}>{dst}:{round(amt, 3)}" + (":b" if bip else ""))
        for l, pts in self.lfo_pts.items():
            parts.append(f"l{l}=" + ";".join(f"{x},{y},{c}" for x, y, c in pts))
        if self.fx:
            parts.append("fx=" + ",".join(str(i) for i in self.fx))
        return " ".join(parts)


BANK = []


def new(cat, name):
    p = P(cat, name)
    BANK.append(p)
    return p


# ---------------------------------------------------------------- shared pieces
def amp(p, a=0.003, d=1.0, s=1.0, r=0.2, h=0.0):
    return p.set(env1_attack=a, env1_hold=h, env1_decay=d, env1_sustain=s, env1_release=r)


def env2(p, a=0.0, d=0.3, s=0.0, r=0.3):
    return p.set(env2_attack=a, env2_decay=d, env2_sustain=s, env2_release=r)


def env3(p, a=0.0, d=0.1, s=0.0, r=0.1):
    return p.set(env3_attack=a, env3_decay=d, env3_sustain=s, env3_release=r)


def flt(p, kind=LP24, cutoff=2000.0, res=0.2, drive=0.0, key=0.0):
    return p.set(flt_on=True, flt_type=kind, flt_cutoff=cutoff, flt_res=res, flt_drive=drive, flt_keytrack=key)


def lfo(p, n, div=None, rate=None, shape=SINE_L, mode=FREE, rise=0.0, smooth=0.0, phase=0.0):
    d = {f"lfo{n}_shape": shape, f"lfo{n}_mode": mode, f"lfo{n}_rise": rise, f"lfo{n}_smooth": smooth,
         f"lfo{n}_phase": phase}
    if div is not None:
        d.update({f"lfo{n}_sync": True, f"lfo{n}_div": DIV[div]})
    else:
        d.update({f"lfo{n}_sync": False, f"lfo{n}_rate": rate})
    return p.set(**d)


def osc(p, o, table=None, pos=None, uni=1, det=0.25, blend=0.75, octave=0, semi=0, fine=0.0, level=0.75,
        warp=W_OFF, warp_amt=0.0, width=1.0, pan=0.0, rand=1.0):
    pre = f"osc{o}_"
    if table:
        p.table(o, table)
    d = {pre + "on": True, pre + "unison": uni, pre + "detune": det, pre + "blend": blend, pre + "octave": octave,
         pre + "semi": semi, pre + "fine": fine, pre + "level": level, pre + "warp": warp, pre + "warpAmt": warp_amt,
         pre + "width": width, pre + "pan": pan, pre + "rand": rand}
    if pos is not None:
        d[pre + "pos"] = pos
    defaults = {"unison": 1, "detune": 0.25, "blend": 0.75, "octave": 0, "semi": 0, "fine": 0.0, "level": 0.75,
                "warp": W_OFF, "warpAmt": 0.0, "width": 1.0, "pan": 0.0, "rand": 1.0}
    d = {k: v for k, v in d.items() if defaults.get(k[len(pre):], object()) != v}
    return p.set(**d)


def sub(p, octave=-1, level=0.6, shape=0):
    return p.set(sub_on=True, sub_octave=octave, sub_level=level, sub_shape=shape)


def noise(p, kind=0, level=0.2, color=0.5, to_filter=True):
    return p.set(noise_on=True, noise_type=kind, noise_level=level, noise_color=color, noise_toFilter=to_filter)


def mono(p, glide=0.0, legato=False):
    return p.set(mono=True, glide=glide, legato=legato)


def poly(p, n=10):
    return p.set(poly=n)


def dist(p, mode=TUBE, drive=0.3, tone=0.8, mix=1.0):
    return p.set(fx_dist_on=True, fx_dist_mode=mode, fx_dist_drive=drive, fx_dist_tone=tone, fx_dist_mix=mix)


def chorus(p, rate=0.6, depth=0.4, mix=0.5, delay=8.0, fb=0.2):
    return p.set(fx_cho_on=True, fx_cho_rate=rate, fx_cho_depth=depth, fx_cho_mix=mix, fx_cho_delay=delay,
                 fx_cho_feedback=fb)


def phaser(p, rate=0.4, depth=0.6, mix=0.5, freq=800.0, fb=0.5):
    return p.set(fx_pha_on=True, fx_pha_rate=rate, fx_pha_depth=depth, fx_pha_mix=mix, fx_pha_freq=freq,
                 fx_pha_feedback=fb)


def delay(p, div="1/8.", mix=0.2, fb=0.35, hicut=6000.0, pingpong=True):
    return p.set(fx_dly_on=True, fx_dly_sync=True, fx_dly_div=DIV[div], fx_dly_mix=mix, fx_dly_feedback=fb,
                 fx_dly_hicut=hicut, fx_dly_pingpong=pingpong)


def reverb(p, mix=0.25, size=0.6, damp=0.5, width=1.0):
    return p.set(fx_rev_on=True, fx_rev_mix=mix, fx_rev_size=size, fx_rev_damp=damp, fx_rev_width=width)


def comp(p, thresh=-18.0, ratio=4.0, multi=False, upward=0.0, attack=5.0, release=120.0, gain=0.0, mix=1.0):
    return p.set(fx_cmp_on=True, fx_cmp_thresh=thresh, fx_cmp_ratio=ratio, fx_cmp_multi=multi,
                 fx_cmp_upward=upward, fx_cmp_attack=attack, fx_cmp_release=release, fx_cmp_gain=gain,
                 fx_cmp_mix=mix)


def eq(p, low_freq=120.0, low=0.0, high_freq=6000.0, high=0.0):
    return p.set(fx_eq_on=True, fx_eq_lowfreq=low_freq, fx_eq_lowgain=low, fx_eq_highfreq=high_freq,
                 fx_eq_highgain=high)


def pump(p, lfo_n=4, div="1/4", depth=0.55):
    """Sidechain-style ducking on every beat, the classic house and EDM pump."""
    p.lfo_shape(lfo_n, [(0, 0, 0), (0.25, 0.75, 0), (0.45, 1, 0), (1, 1, 0)])
    lfo(p, lfo_n, div=div, shape=CUSTOM_L, mode=FREE)
    p.v[f"lfo{lfo_n}_shape"] = CUSTOM_L
    return p.mod(f"Lfo{lfo_n}", "MasterVol", depth).set(master_vol=0.7 - depth)


def macros(p, tone="FltCutoff", tone_amt=0.3, move=None, move_amt=0.4, space=True):
    """Macro 1 is always tone, Macro 2 movement, Macro 3/4 come from the reverb and delay sends."""
    p.mod("Macro1", tone, tone_amt)
    if move:
        p.mod("Macro2", move, move_amt)
    return p


# ================================================================ BASS
def future_house(name, table="Basic Shapes", pos=SAW, cutoff=500, env_amt=0.35, res=0.35, pitch=0.0,
                 decay=0.25, fold=False, metallic=False, octave=-1):
    p = new("Bass", name)
    osc(p, "A", table, pos, uni=2, det=0.12, octave=octave)
    if metallic:
        osc(p, "B", "Metal FM", 0.35, octave=octave, level=0.45)
        p.mod("Env2", "OscBPos", 0.4)
    sub(p, -1, 0.55)
    amp(p, 0.001, 0.4, 0.75, 0.08)
    flt(p, LADDER, cutoff, res, 0.35)
    env2(p, 0.0, decay, 0.0, 0.1)
    p.mod("Env2", "FltCutoff", env_amt)
    if pitch:
        env3(p, 0.0, 0.05, 0.0, 0.05)
        p.mod("Env3", "OscACoarse", pitch)
    if fold:
        dist(p, FOLD, 0.25, 0.7, 0.5)
    else:
        dist(p, TUBE, 0.35, 0.7)
    comp(p, -20, 6, attack=2, release=80)
    mono(p, 0.03)
    return macros(p, "FltCutoff", 0.3, "FltRes", 0.4)


def reese(name, det=0.42, cutoff=700, drift=0.25, dirty=0.3, uni=2, octave=-1, phase=False):
    p = new("Bass", name)
    osc(p, "A", "Basic Shapes", SAW, uni=uni, det=det, octave=octave)
    osc(p, "B", "Basic Shapes", SAW, uni=uni, det=det + 0.08, octave=octave, fine=9.0, level=0.7)
    sub(p, -1, 0.65)
    amp(p, 0.004, 1.0, 1.0, 0.15)
    flt(p, LADDER, cutoff, 0.25, 0.3)
    lfo(p, 1, rate=drift, shape=SINE_L)
    p.mod("Lfo1", "FltCutoff", 0.12, True)
    p.mod("Lfo1", "OscBFine", 0.05, True)
    dist(p, TUBE, dirty, 0.6)
    if phase:
        phaser(p, 0.15, 0.7, 0.4, 600, 0.6)
    comp(p, -18, 4)
    mono(p, 0.06)
    return macros(p, "FltCutoff", 0.35, "OscADetune", 0.4)


def wobble(name, div="1/8", table="FM Growl", cutoff=220, depth=0.5, pos_depth=0.4, shape=SINE_L, res=0.45,
           mode=FREE, dmode=TUBE, ddrive=0.45):
    p = new("Bass", name)
    osc(p, "A", table, 0.2, uni=3, det=0.15, octave=-1)
    sub(p, -1, 0.6)
    amp(p, 0.003, 1.0, 1.0, 0.1)
    flt(p, LP24, cutoff, res, 0.4)
    lfo(p, 1, div=div, shape=shape, mode=mode)
    p.mod("Lfo1", "FltCutoff", depth)
    p.mod("Lfo1", "OscAPos", pos_depth)
    dist(p, dmode, ddrive, 0.7, 0.7)
    comp(p, -24, 6, multi=True, upward=0.4)
    mono(p, 0.02)
    p.mod("Macro1", "FltCutoff", 0.3)
    p.mod("Macro2", "Lfo1Rate", 0.0)
    p.mod("ModWheel", "OscAPos", 0.4)
    return p


def growl(name, table="Talk Growl", lfo_pts=None, div="1/8", filt=VOWEL, cutoff=900, res=0.55, fold=0.3,
          dmode=FOLD, octave=-1, fm=False, comb=False, pitch_drop=0.0, uni=2):
    """Bass-music talking growl: wavetable and formant filter riding the same LFO, folded and crushed."""
    p = new("Bass", name)
    osc(p, "A", table, 0.1, uni=uni, det=0.1, octave=octave)
    if fm:
        osc(p, "B", "Sine", 0.0, octave=octave, level=0.0)
        p.set(oscA_warp=W_FM, oscA_warpAmt=0.25, oscB_toFilter=False)
        p.mod("Lfo1", "OscAWarp", 0.35)
    sub(p, -1, 0.5)
    amp(p, 0.002, 1.0, 1.0, 0.08)
    flt(p, COMBM if comb else filt, cutoff, res, 0.3)
    pts = lfo_pts or [(0, 0, 0), (0.25, 1, -0.4), (0.5, 0.3, 0), (0.75, 0.9, 0.5), (1, 0, 0)]
    p.lfo_shape(1, pts)
    lfo(p, 1, div=div, shape=CUSTOM_L, mode=RETRIG)
    p.v["lfo1_shape"] = CUSTOM_L
    p.mod("Lfo1", "OscAPos", 0.8)
    p.mod("Lfo1", "FltCutoff", 0.45)
    if pitch_drop:
        env3(p, 0.0, 0.12, 0.0, 0.1)
        p.mod("Env3", "OscACoarse", pitch_drop)
    dist(p, dmode, fold, 0.75, 0.8)
    comp(p, -26, 8, multi=True, upward=0.55, attack=1, release=60)
    eq(p, 90, -2, 6000, 2)
    p.fx = [0, 5, 6, 1, 2, 3, 4]
    mono(p, 0.025)
    p.mod("Macro1", "FltCutoff", 0.25)
    p.mod("Macro2", "OscAPos", 0.4)
    p.mod("ModWheel", "Lfo1Rate", 0.25)
    return p


def sub_bass(name, shape_pos=SINE, glide=0.04, punch=0.0, drive=0.1, decay=0.8, sus=0.8, table="Basic Shapes"):
    p = new("Bass", name)
    osc(p, "A", table, shape_pos, octave=-1, rand=0.0)
    amp(p, 0.001, decay, sus, 0.12)
    if punch:
        env3(p, 0.0, 0.06, 0.0, 0.05)
        p.mod("Env3", "OscACoarse", punch)
    if drive:
        dist(p, SOFT, drive, 0.6)
    mono(p, glide)
    return macros(p, "OscAPos", 0.2)


def acid(name, cutoff=350, res=0.75, env_amt=0.45, decay=0.18, pos=SAW, glide=0.08, drive=0.4, table="Basic Shapes"):
    p = new("Bass", name)
    osc(p, "A", table, pos, octave=-1, rand=0.0)
    amp(p, 0.001, 0.4, 0.8, 0.06)
    flt(p, LADDER, cutoff, res, 0.4, 0.3)
    env2(p, 0.0, decay, 0.0, 0.1)
    p.mod("Env2", "FltCutoff", env_amt)
    p.mod("Velocity", "FltCutoff", 0.12)
    dist(p, TUBE, drive, 0.7)
    delay(p, "1/8.", 0.12, 0.3, 4000)
    mono(p, glide, legato=True)
    return macros(p, "FltCutoff", 0.3, "FltRes", 0.25)


def build_bass():
    future_house("Bounce Metal", metallic=True, pitch=0.06)
    future_house("Bounce Square", pos=SQUARE, cutoff=420, env_amt=0.4)
    future_house("Flip Phone", table="Pulse Width", pos=0.55, cutoff=600, res=0.45, pitch=0.12)
    future_house("Groove Ladder", cutoff=350, env_amt=0.45, decay=0.18)
    future_house("Rubber Pluck", table="Hollow Lead", pos=0.3, cutoff=300, env_amt=0.5, decay=0.14, pitch=0.08)
    future_house("Clang House", table="Metal FM", pos=0.45, cutoff=900, env_amt=0.3, fold=True)
    reese("Reese Classic")
    reese("Reese Wide", det=0.55, uni=4, cutoff=900, phase=True)
    reese("Reese Tunnel", det=0.35, cutoff=450, dirty=0.5, drift=0.12)
    reese("Neuro Reese", det=0.48, cutoff=1200, dirty=0.6, phase=True).table("A", "Comb Saw").set(oscA_pos=0.4)
    wobble("Wobble Eighth", "1/8")
    wobble("Wobble Triplet", "1/4T", table="Fold Sweep", depth=0.55)
    wobble("Wobble Sixteenth", "1/16", depth=0.4, pos_depth=0.5, table="Squash Saw")
    wobble("Wub Square", "1/8", shape=SQUARE_L, table="Basic Shapes", pos_depth=0.3, depth=0.5)
    growl("Yoi Growl", "Talk Growl", div="1/8")
    growl("Talk Back", "Talk Growl", div="1/4", lfo_pts=[(0, 0, 0), (0.5, 1, 0.5), (1, 0, -0.5)], filt=VOWEL)
    growl("Scream Machine", "Metal FM", div="1/16", fm=True, dmode=FOLD, fold=0.4)
    growl("Grind Unit", "Squash Saw", div="1/8", comb=True, res=0.6, dmode=HARD, fold=0.35)
    growl("Bit Chewer", "Bit Grit", div="1/16T", dmode=CRUSH, fold=0.35)
    growl("Wah Monster", "FM Growl", div="1/4", lfo_pts=[(0, 0, 0), (0.15, 1, -0.6), (1, 0, 0.3)], filt=BP24, res=0.5)
    growl("Drop Growl", "Talk Growl", div="1/8", pitch_drop=0.12, fold=0.35)
    sub_bass("Clean Sub", glide=0.03)
    sub_bass("Punch Sub", punch=0.15, drive=0.15)
    sub_bass("Glide 808", glide=0.18, punch=0.12, drive=0.35, decay=1.6, sus=0.0)
    sub_bass("Warm Triangle Sub", shape_pos=TRI, drive=0.2)
    acid("Acid Line")
    acid("Acid Squelch", res=0.85, env_amt=0.55, decay=0.12, drive=0.55)
    acid("Organ Bass", cutoff=1200, res=0.15, env_amt=0.2, decay=0.25, table="Organ Bars", pos=0.4, glide=0.0)

    p = new("Bass", "Hoover Rave")
    osc(p, "A", "Pulse Width", 0.3, uni=7, det=0.6, octave=-1, warp=W_PWM, warp_amt=0.3)
    osc(p, "B", "Basic Shapes", SAW, uni=5, det=0.5, octave=-1, level=0.5)
    sub(p, -1, 0.4)
    amp(p, 0.01, 1.0, 1.0, 0.2)
    env3(p, 0.0, 0.25, 0.0, 0.2)
    p.mod("Env3", "OscACoarse", -0.06)
    p.mod("Env3", "OscBCoarse", -0.06)
    lfo(p, 1, rate=4.5)
    p.mod("Lfo1", "OscAWarp", 0.2)
    flt(p, LP24, 3000, 0.15)
    chorus(p, 0.3, 0.5, 0.4)
    mono(p, 0.12)
    macros(p, "FltCutoff", 0.3, "OscADetune", 0.3)

    p = new("Bass", "Rolling Tech")
    osc(p, "A", "Basic Shapes", SAW, octave=-1, rand=0.0)
    sub(p, -1, 0.6)
    amp(p, 0.001, 0.22, 0.0, 0.08)
    flt(p, LADDER, 250, 0.35, 0.25)
    env2(p, 0.0, 0.12)
    p.mod("Env2", "FltCutoff", 0.35)
    p.mod("NoteRandom", "FltCutoff", 0.06)
    comp(p, -16, 4)
    mono(p, 0.0)
    macros(p, "FltCutoff", 0.3, "FltRes", 0.3)

    p = new("Bass", "Donk Drop")
    osc(p, "A", "Basic Shapes", TRI, octave=-1, rand=0.0)
    sub(p, -1, 0.5)
    amp(p, 0.001, 0.3, 0.0, 0.08)
    env3(p, 0.0, 0.07, 0.0, 0.05)
    p.mod("Env3", "OscACoarse", 0.25)
    flt(p, LP24, 700, 0.45)
    env2(p, 0.0, 0.1)
    p.mod("Env2", "FltCutoff", 0.3)
    dist(p, SOFT, 0.3)
    mono(p, 0.0)
    macros(p, "FltCutoff", 0.3)


# ================================================================ LEADS
def supersaw_lead(name, uni=9, det=0.5, cutoff=7000, octave_b=1, b_table="Basic Shapes", b_pos=SAW, b_level=0.45,
                  glide=0.0, vib=True, dly="1/8.", rev=0.2, drive=0.0, pitch_up=0.0):
    p = new("Leads", name)
    osc(p, "A", "Basic Shapes", SAW, uni=uni, det=det, blend=0.8)
    osc(p, "B", b_table, b_pos, uni=max(1, uni // 2), det=det * 0.7, octave=octave_b, level=b_level)
    amp(p, 0.004, 0.6, 0.85, 0.3)
    flt(p, LP24, cutoff, 0.15, 0.0, 0.3)
    if vib:
        lfo(p, 2, rate=5.5, rise=0.5)
        p.mod("Lfo2", "OscAFine", 0.04, True)
        p.mod("ModWheel", "OscAFine", 0.0)
    if pitch_up:
        env3(p, 0.0, 0.08, 0.0, 0.05)
        p.mod("Env3", "OscACoarse", -pitch_up)
    if drive:
        dist(p, SOFT, drive, 0.8, 0.6)
    delay(p, dly, 0.18, 0.35, 5000)
    reverb(p, rev, 0.7)
    eq(p, 200, -3, 8000, 2)
    if glide:
        mono(p, glide, legato=True)
    else:
        poly(p, 8)
    return macros(p, "FltCutoff", 0.3, "OscADetune", 0.35)


def screech(name, table="Metal FM", pos=0.5, warp=W_SYNC, warp_amt=0.4, lfo_div="1/16", drive=0.45, octave=0,
            fm_osc=False, pitch_env=0.0, filt=PEAK):
    """Bass-music screech: bright metallic table, sync or FM warp swept fast, folded and resonant."""
    p = new("Leads", name)
    osc(p, "A", table, pos, uni=3, det=0.2, octave=octave, warp=warp, warp_amt=warp_amt)
    if fm_osc:
        osc(p, "B", "Sine", 0.0, octave=octave + 1, level=0.0)
        p.set(oscB_toFilter=False)
    amp(p, 0.002, 1.0, 1.0, 0.12)
    flt(p, filt, 2500, 0.6, 0.3)
    lfo(p, 1, div=lfo_div, shape=TRI_L, mode=RETRIG)
    p.mod("Lfo1", "OscAWarp", 0.35)
    p.mod("Lfo1", "OscAPos", 0.3)
    p.mod("Lfo1", "FltCutoff", 0.15)
    if pitch_env:
        env3(p, 0.0, 0.15, 0.0, 0.1)
        p.mod("Env3", "OscACoarse", pitch_env)
    dist(p, FOLD, drive, 0.85, 0.7)
    comp(p, -24, 8, multi=True, upward=0.5, attack=1)
    eq(p, 300, -6, 7000, -2)
    reverb(p, 0.12, 0.5)
    mono(p, 0.05, legato=True)
    p.mod("Macro1", "FltCutoff", 0.3)
    p.mod("Macro2", "OscAWarp", 0.4)
    p.mod("ModWheel", "Lfo1Rate", 0.3)
    p.mod("PitchBend", "OscAPos", 0.2)
    return p


def mono_lead(name, table="Basic Shapes", pos=SAW, cutoff=3000, res=0.3, glide=0.08, env_amt=0.3, uni=1, det=0.2,
              warp=W_OFF, warp_amt=0.0, vib=True, drive=0.0, octave=0, b=None, noise_lvl=0.0, rev=0.15, dly=0.15):
    p = new("Leads", name)
    osc(p, "A", table, pos, uni=uni, det=det, octave=octave, warp=warp, warp_amt=warp_amt)
    if b:
        osc(p, "B", **b)
    if noise_lvl:
        noise(p, 1, noise_lvl, 0.7)
    amp(p, 0.003, 0.8, 0.85, 0.2)
    flt(p, LP24, cutoff, res, 0.1, 0.4)
    env2(p, 0.0, 0.4, 0.2, 0.3)
    p.mod("Env2", "FltCutoff", env_amt)
    if vib:
        lfo(p, 2, rate=5.8, rise=0.4)
        p.mod("Lfo2", "OscAFine", 0.035, True)
    p.mod("ModWheel", "OscAFine", 0.0)
    if drive:
        dist(p, TUBE, drive, 0.8)
    delay(p, "1/8.", dly, 0.35, 5000)
    reverb(p, rev, 0.6)
    mono(p, glide, legato=True)
    return macros(p, "FltCutoff", 0.3, "OscAPos", 0.4)


def build_leads():
    supersaw_lead("Festival Saw", uni=11, det=0.55, rev=0.3)
    supersaw_lead("Anthem Stack", uni=9, det=0.5, b_table="Hollow Lead", b_pos=0.8)
    supersaw_lead("Main Stage", uni=13, det=0.6, drive=0.25, pitch_up=0.02)
    supersaw_lead("Trance Gate", uni=7, det=0.45, dly="1/16", rev=0.25)
    supersaw_lead("Hyper Saw", uni=16, det=0.7, cutoff=10000, drive=0.3)
    supersaw_lead("Glide Saw", uni=7, det=0.4, glide=0.12)
    screech("Metal Scream")
    screech("Sync Screech", "Saw", 0.0, W_SYNC, 0.5, "1/8")
    screech("Formant Shriek", "Talk Growl", 0.6, W_BENDP, 0.5, "1/16T", filt=VOWEL)
    screech("Laser Scream", "Squash Saw", 0.4, W_SYNC, 0.6, "1/32", pitch_env=0.12)
    screech("FM Banshee", "Sine", 0.0, W_FM, 0.6, "1/16", fm_osc=True, drive=0.5)
    screech("Grit Siren", "Bit Grit", 0.3, W_MIRROR, 0.0, "1/8T", drive=0.4)
    mono_lead("Sync Rider", "Basic Shapes", SAW, warp=W_SYNC, warp_amt=0.3, uni=3)
    mono_lead("Square Chip", "Basic Shapes", SQUARE, cutoff=9000, res=0.1, glide=0.02, env_amt=0.0, rev=0.1)
    mono_lead("Hollow Pipe", "Hollow Lead", 0.2, cutoff=4000, glide=0.06)
    mono_lead("Whistle", "Sine", 0.0, cutoff=12000, res=0.0, env_amt=0.0, noise_lvl=0.08, glide=0.1, rev=0.3)
    mono_lead("Acid Lead", "Basic Shapes", SAW, cutoff=800, res=0.75, env_amt=0.5, drive=0.4, glide=0.06)
    mono_lead("Fat Mono", "Basic Shapes", SAW, uni=3, det=0.3, cutoff=2500, glide=0.05,
              b=dict(table="Basic Shapes", pos=SQUARE, octave=-1, level=0.5))
    mono_lead("PWM Lead", "Pulse Width", 0.2, warp=W_PWM, warp_amt=0.3, cutoff=5000)
    mono_lead("Fold Lead", "Fold Sweep", 0.4, cutoff=6000, drive=0.3, uni=2)
    mono_lead("Vocal Lead", "Vowel Morph", 0.3, cutoff=8000, res=0.1, env_amt=0.1, uni=3, rev=0.3)
    mono_lead("Octave Lead", "Basic Shapes", SAW, uni=5, det=0.35, cutoff=6000,
              b=dict(table="Basic Shapes", pos=SAW, octave=1, uni=3, det=0.3, level=0.5))
    mono_lead("Brass Lead", "Basic Shapes", SAW, uni=3, det=0.25, cutoff=900, res=0.15, env_amt=0.45)
    mono_lead("Dirty Saw", "Squash Saw", 0.35, cutoff=7000, drive=0.45, uni=2)
    mono_lead("Bell Lead", "Glass Bell", 0.4, cutoff=10000, env_amt=0.0, glide=0.0,
              b=dict(table="Sine", pos=0.0, octave=2, level=0.25), rev=0.3, dly=0.25)

    p = mono_lead("Future Wobble Lead", "Basic Shapes", SAW, uni=7, det=0.45, cutoff=4500, glide=0.04)
    lfo(p, 3, div="1/8T", shape=SINE_L, mode=RETRIG)
    p.mod("Lfo3", "FltCutoff", 0.25)
    p.mod("Lfo3", "OscACoarse", 0.006, True)

    p = mono_lead("Laser Zap", "Basic Shapes", SQUARE, cutoff=9000, env_amt=0.0, glide=0.0)
    env3(p, 0.0, 0.18, 0.0, 0.1)
    p.mod("Env3", "OscACoarse", 0.3)

    p = mono_lead("Siren Lead", "Hollow Lead", 0.6, cutoff=6000, env_amt=0.1, glide=0.15)
    lfo(p, 3, div="1/4", shape=TRI_L)
    p.mod("Lfo3", "OscACoarse", 0.04, True)

    p = mono_lead("Chord Lead", "Chord Stack", 0.8, cutoff=7000, uni=5, det=0.35, glide=0.0)
    p.set(mono=False, poly=8)

    p = mono_lead("Talkbox Lead", "Talk Growl", 0.3, cutoff=8000, res=0.1, env_amt=0.0, uni=2, det=0.15)
    lfo(p, 3, div="1/4", shape=TRI_L)
    p.mod("Lfo3", "OscAPos", 0.5)


# ================================================================ KEYS
def keys(name, table, pos, decay=1.2, sus=0.3, rel=0.4, cutoff=6000, env_amt=0.25, b=None, uni=1, det=0.15,
         chorus_on=False, phaser_on=False, tremolo=0.0, rev=0.2, dly=0.0, vel_bright=0.25, crush=0.0, noise_lvl=0.0,
         noise_type=1, warp=W_OFF, warp_amt=0.0, pumped=False):
    p = new("Keys", name)
    osc(p, "A", table, pos, uni=uni, det=det, warp=warp, warp_amt=warp_amt)
    if b:
        osc(p, "B", **b)
    if noise_lvl:
        noise(p, noise_type, noise_lvl, 0.6, False)
    amp(p, 0.002, decay, sus, rel)
    flt(p, LP12, cutoff, 0.1, 0.0, 0.5)
    env2(p, 0.0, decay * 0.6, 0.2, rel)
    p.mod("Env2", "FltCutoff", env_amt)
    p.mod("Velocity", "FltCutoff", vel_bright)
    if tremolo:
        lfo(p, 3, rate=5.0)
        p.mod("Lfo3", "OscALevel", -tremolo)
    if chorus_on:
        chorus(p, 0.5, 0.35, 0.4)
    if phaser_on:
        phaser(p, 0.3, 0.5, 0.35)
    if crush:
        dist(p, DOWNS, crush, 0.5, 0.5)
    if dly:
        delay(p, "1/8.", dly, 0.3, 4500)
    reverb(p, rev, 0.6)
    if pumped:
        pump(p)
    poly(p, 12)
    return macros(p, "FltCutoff", 0.3, "OscAPos", 0.3)


def build_keys():
    keys("House Piano", "Glass Bell", 0.15, decay=1.4, sus=0.15,
         b=dict(table="Basic Shapes", pos=TRI, level=0.6), env_amt=0.3)
    keys("Deep House Organ", "Organ Bars", 0.55, decay=0.3, sus=0.7, rel=0.12, cutoff=5000, env_amt=0.1,
         chorus_on=True)
    keys("Garage Organ", "Organ Bars", 0.8, decay=0.25, sus=0.6, rel=0.1, cutoff=7000, phaser_on=True)
    keys("Velvet EP", "Sine", 0.0, decay=2.0, sus=0.2, rel=0.6, warp=W_FM, warp_amt=0.15,
         b=dict(table="Sine", pos=0.0, octave=1, level=0.0), tremolo=0.15, chorus_on=True)
    keys("Glass EP", "Glass Bell", 0.3, decay=1.8, sus=0.1, rel=0.5, chorus_on=True, tremolo=0.1)
    keys("Digital EP", "Metal FM", 0.15, decay=1.5, sus=0.15, rel=0.5, cutoff=8000)
    keys("Toy Box", "Glass Bell", 0.7, decay=0.6, sus=0.0, rel=0.3, cutoff=10000, dly=0.15)
    keys("Mallet", "Sine", 0.0, decay=0.35, sus=0.0, rel=0.3, b=dict(table="Sine", pos=0.0, octave=2, level=0.3))
    keys("Clav Funk", "Pulse Width", 0.6, decay=0.4, sus=0.1, rel=0.1, cutoff=1500, env_amt=0.4)
    keys("Lofi Keys", "Organ Bars", 0.3, decay=1.2, sus=0.3, rel=0.4, crush=0.4, noise_lvl=0.04, noise_type=3,
         cutoff=3000)
    keys("Tape Keys", "Glass Bell", 0.2, decay=1.6, sus=0.2, rel=0.5, cutoff=4000, chorus_on=True, noise_lvl=0.03)
    keys("Stab Chord", "Chord Stack", 1.0, decay=0.35, sus=0.0, rel=0.2, cutoff=3500, env_amt=0.35, uni=3,
         det=0.25, dly=0.12)
    keys("Big Room Stab", "Basic Shapes", SAW, decay=0.3, sus=0.0, rel=0.25, cutoff=2500, env_amt=0.45, uni=9,
         det=0.5, rev=0.35)
    keys("Future Keys", "Basic Shapes", SAW, decay=0.6, sus=0.5, rel=0.3, cutoff=3000, env_amt=0.3, uni=7,
         det=0.45, pumped=True)
    keys("Pumping Chords", "Hollow Lead", 0.6, decay=1.0, sus=0.8, rel=0.3, cutoff=4000, uni=5, det=0.35,
         pumped=True)
    keys("Soft Rhodes", "Sine", 0.0, decay=1.8, sus=0.25, rel=0.5, b=dict(table="Glass Bell", pos=0.1, level=0.3),
         tremolo=0.2, cutoff=4000)
    keys("Bright Upright", "Harmonic Sweep", 0.6, decay=1.5, sus=0.1, rel=0.4, env_amt=0.35)
    keys("Harpsi", "Pulse Width", 0.85, decay=0.9, sus=0.0, rel=0.3, cutoff=9000,
         b=dict(table="Basic Shapes", pos=SAW, octave=1, level=0.35))
    keys("Square Keys", "Basic Shapes", SQUARE, decay=0.8, sus=0.4, rel=0.3, cutoff=2500, chorus_on=True)
    keys("Hollow Keys", "Hollow Lead", 0.25, decay=1.0, sus=0.4, rel=0.4, cutoff=3500, phaser_on=True)
    keys("Kalimba", "Glass Bell", 0.5, decay=0.5, sus=0.0, rel=0.4, b=dict(table="Sine", pos=0.0, level=0.5),
         dly=0.1)
    keys("Steel Pan", "Metal FM", 0.3, decay=0.7, sus=0.0, rel=0.4, cutoff=7000, dly=0.12)
    keys("Music Box", "Glass Bell", 0.9, decay=0.9, sus=0.0, rel=0.6, cutoff=12000, rev=0.35)
    keys("Retro Organ", "Organ Bars", 1.0, decay=0.5, sus=0.9, rel=0.1, cutoff=8000, tremolo=0.25)
    keys("Bit Piano", "Bit Grit", 0.2, decay=1.0, sus=0.1, rel=0.3, cutoff=6000)
    keys("Vowel Keys", "Vowel Morph", 0.1, decay=1.0, sus=0.3, rel=0.4, cutoff=7000, chorus_on=True)
    keys("Fold Keys", "Fold Sweep", 0.15, decay=0.9, sus=0.2, rel=0.3, env_amt=0.2).mod("Velocity", "OscAPos", 0.4)
    keys("Detuned Upright", "Basic Shapes", TRI, decay=1.6, sus=0.15, rel=0.5, uni=3, det=0.2,
         b=dict(table="Glass Bell", pos=0.2, level=0.35))
    keys("Night Keys", "Sine", 0.0, decay=2.5, sus=0.3, rel=1.0, b=dict(table="Chord Stack", pos=0.3, level=0.25),
         rev=0.4, dly=0.15, cutoff=3000)
    keys("Pluck Piano", "Harmonic Sweep", 0.8, decay=0.45, sus=0.0, rel=0.3, cutoff=2000, env_amt=0.45, dly=0.15)


# ================================================================ PADS
def pad(name, table, pos, uni=7, det=0.35, atk=0.8, rel=2.5, cutoff=3500, b=None, lfo_pos=0.0, lfo_rate=0.1,
        lfo_cut=0.0, sweep_div=None, chorus_on=True, rev=0.45, size=0.85, dly=0.0, pumped=False, noise_lvl=0.0,
        phaser_on=False, filt=LP24, res=0.15):
    p = new("Pads", name)
    osc(p, "A", table, pos, uni=uni, det=det, blend=0.8)
    if b:
        osc(p, "B", **b)
    if noise_lvl:
        noise(p, 1, noise_lvl, 0.6)
    amp(p, atk, 1.5, 0.85, rel)
    flt(p, filt, cutoff, res, 0.0, 0.2)
    if lfo_pos:
        lfo(p, 1, rate=lfo_rate, shape=DRIFT_L)
        p.mod("Lfo1", "OscAPos", lfo_pos)
    if lfo_cut:
        if sweep_div:
            lfo(p, 2, div=sweep_div, shape=TRI_L)
        else:
            lfo(p, 2, rate=0.07, shape=SINE_L)
        p.mod("Lfo2", "FltCutoff", lfo_cut, True)
    if chorus_on:
        chorus(p, 0.3, 0.45, 0.4)
    if phaser_on:
        phaser(p, 0.12, 0.6, 0.35)
    if dly:
        delay(p, "1/4.", dly, 0.45, 3500)
    reverb(p, rev, size, 0.4)
    if pumped:
        pump(p, 4, "1/4", 0.5)
    poly(p, 10)
    p.mod("Macro1", "FltCutoff", 0.3)
    p.mod("Macro2", "OscAPos", 0.4)
    p.mod("ModWheel", "FltCutoff", 0.2)
    return p


def build_pads():
    pad("Supersaw Pad", "Basic Shapes", SAW, uni=11, det=0.5, b=dict(table="Basic Shapes", pos=SAW, octave=1, uni=5,
        det=0.45, level=0.35))
    pad("Warm Analog", "Basic Shapes", SAW, uni=5, det=0.3, cutoff=1600, b=dict(table="Basic Shapes", pos=SQUARE,
        octave=-1, level=0.35), lfo_cut=0.06)
    pad("Vowel Choir", "Vowel Morph", 0.2, lfo_pos=0.5)
    pad("Breath Pad", "Breath", 0.4, lfo_pos=0.4, noise_lvl=0.05, cutoff=5000)
    pad("Glass Pad", "Glass Bell", 0.3, lfo_pos=0.6, lfo_rate=0.15, cutoff=8000, dly=0.2)
    pad("Evolving Waves", "Harmonic Sweep", 0.2, lfo_pos=0.6, lfo_rate=0.05, cutoff=6000)
    pad("Pump Pad", "Basic Shapes", SAW, uni=9, det=0.45, cutoff=4000, pumped=True)
    pad("Shimmer Pad", "Hollow Lead", 0.5, b=dict(table="Sine", pos=0.0, octave=2, level=0.25), rev=0.6, size=0.95,
        dly=0.25, cutoff=9000)
    pad("Dark Drone", "Fold Sweep", 0.2, uni=5, det=0.2, cutoff=700, res=0.3, lfo_pos=0.3, lfo_rate=0.04,
        lfo_cut=0.08, chorus_on=False, phaser_on=True)
    pad("Future Garage", "Vowel Morph", 0.6, uni=3, det=0.2, cutoff=2200, noise_lvl=0.06, dly=0.2)
    pad("Sweep Pad", "Basic Shapes", SAW, uni=7, det=0.4, cutoff=900, res=0.35, lfo_cut=0.25, sweep_div="4bar")
    pad("Talking Pad", "Talk Growl", 0.0, uni=5, det=0.25, lfo_pos=0.7, lfo_rate=0.2, cutoff=6000)
    pad("Metal Air", "Metal FM", 0.2, uni=5, det=0.3, lfo_pos=0.3, cutoff=5000, phaser_on=True)
    pad("Comb Motion", "Comb Saw", 0.0, uni=7, det=0.35, lfo_pos=0.8, lfo_rate=0.12)
    pad("Organ Haze", "Organ Bars", 0.5, uni=5, det=0.3, cutoff=3000, phaser_on=True, chorus_on=True)
    pad("Chord Cloud", "Chord Stack", 0.6, uni=5, det=0.3, cutoff=4000, lfo_pos=0.3, dly=0.2)
    pad("Square Dream", "Basic Shapes", SQUARE, uni=5, det=0.3, cutoff=2000, lfo_cut=0.1)
    pad("PWM Pad", "Pulse Width", 0.3, uni=5, det=0.3, lfo_pos=0.5, lfo_rate=0.3, cutoff=3000)
    pad("Sunrise", "Basic Shapes", SAW, uni=9, det=0.4, atk=3.0, rel=4.0, cutoff=500, res=0.2,
        b=dict(table="Glass Bell", pos=0.4, octave=1, level=0.3)).set(env2_attack=4.0, env2_decay=1.0,
        env2_sustain=1.0, env2_release=3.0).mod("Env2", "FltCutoff", 0.4)
    pad("Hollow Space", "Hollow Lead", 0.1, uni=5, det=0.3, cutoff=2500, rev=0.6, size=0.95)
    pad("Grit Pad", "Bit Grit", 0.2, uni=5, det=0.3, cutoff=3000, lfo_pos=0.4)
    pad("Lush Strings Pad", "Basic Shapes", SAW, uni=9, det=0.45, cutoff=2800, b=dict(table="Pulse Width", pos=0.4,
        uni=5, det=0.4, level=0.4))
    pad("Night Drive", "Squash Saw", 0.15, uni=7, det=0.35, cutoff=1200, res=0.3, lfo_cut=0.15, sweep_div="2bar",
        dly=0.15)
    pad("Ambient Wash", "Breath", 0.8, uni=3, det=0.2, atk=2.5, rel=5.0, rev=0.65, size=0.98, dly=0.25)
    pad("Crystal Bed", "Glass Bell", 0.8, uni=3, det=0.15, cutoff=10000, lfo_pos=0.4, rev=0.55)
    pad("Sub Pad", "Basic Shapes", TRI, uni=3, det=0.2, cutoff=1000, b=dict(table="Sine", pos=0.0, octave=-1,
        level=0.5))
    pad("Wide Vox", "Vowel Morph", 0.8, uni=9, det=0.4, lfo_pos=0.4, lfo_rate=0.08)
    pad("Synthwave Pad", "Basic Shapes", SAW, uni=5, det=0.35, cutoff=1800, phaser_on=True, chorus_on=True)
    pad("Blade Pad", "Basic Shapes", SAW, uni=7, det=0.5, cutoff=1400, res=0.25, lfo_cut=0.08, rev=0.6,
        size=0.95, b=dict(table="Basic Shapes", pos=SAW, octave=-1, uni=3, det=0.3, level=0.35))
    pad("Slow Bloom", "Harmonic Sweep", 0.0, uni=5, det=0.3, atk=2.0, rel=3.0).set(env2_attack=3.0, env2_decay=1.0,
        env2_sustain=1.0, env2_release=2.0).mod("Env2", "OscAPos", 0.7)


# ================================================================ STRINGS
def strings(name, table="Basic Shapes", pos=SAW, uni=7, det=0.35, atk=0.35, rel=1.2, cutoff=3500, res=0.1,
            b=None, vib=0.02, chorus_on=True, phaser_on=False, rev=0.35, decay=1.0, sus=0.9, short=False,
            swell=False, dly=0.0):
    p = new("Strings", name)
    osc(p, "A", table, pos, uni=uni, det=det, blend=0.85)
    if b:
        osc(p, "B", **b)
    if short:
        amp(p, 0.004, decay, 0.0, rel)
    else:
        amp(p, atk, decay, sus, rel)
    flt(p, LP12, cutoff, res, 0.0, 0.4)
    if swell:
        env2(p, atk * 1.5, 1.0, 1.0, rel)
        p.mod("Env2", "FltCutoff", 0.3)
    if short:
        env2(p, 0.0, decay * 0.5, 0.0, rel)
        p.mod("Env2", "FltCutoff", 0.3)
    if vib:
        lfo(p, 2, rate=5.0, rise=0.8)
        p.mod("Lfo2", "OscAFine", vib, True)
    if chorus_on:
        chorus(p, 0.5, 0.5, 0.5)
    if phaser_on:
        phaser(p, 0.2, 0.6, 0.4)
    if dly:
        delay(p, "1/4", dly, 0.3, 4000)
    reverb(p, rev, 0.75)
    eq(p, 180, -3, 7000, 1)
    poly(p, 12)
    p.mod("Macro1", "FltCutoff", 0.3)
    p.mod("Macro2", "OscADetune", 0.3)
    p.mod("ModWheel", "FltCutoff", 0.25)
    p.mod("Velocity", "FltCutoff", 0.15)
    return p


def build_strings():
    strings("Synth Strings")
    strings("String Machine", "Pulse Width", 0.15, uni=5, det=0.3, phaser_on=True,
            b=dict(table="Basic Shapes", pos=SAW, octave=1, uni=3, det=0.3, level=0.35))
    strings("Pulse Strings", "Pulse Width", 0.3, uni=5, det=0.25, cutoff=4000)
    strings("Trance Strings", uni=11, det=0.5, cutoff=5000, atk=0.15, dly=0.12)
    strings("Cinematic Swell", uni=9, det=0.4, atk=1.5, rel=2.5, cutoff=1500, swell=True, rev=0.5)
    strings("Staccato Saws", uni=7, det=0.35, short=True, decay=0.25, rel=0.15, cutoff=3000)
    strings("Pizzicato", "Harmonic Sweep", 0.5, uni=3, det=0.15, short=True, decay=0.2, rel=0.2, cutoff=2500,
            chorus_on=False)
    strings("Octave Strings", uni=7, det=0.35, b=dict(table="Basic Shapes", pos=SAW, octave=1, uni=5, det=0.35,
            level=0.45))
    strings("Ensemble 80s", uni=5, det=0.3, cutoff=2500, phaser_on=True)
    strings("Dark Cellos", uni=5, det=0.25, cutoff=1200, res=0.15, atk=0.25,
            b=dict(table="Basic Shapes", pos=SAW, octave=-1, uni=3, det=0.25, level=0.5))
    strings("Bright Violins", "Harmonic Sweep", 0.9, uni=7, det=0.3, cutoff=7000, vib=0.03)
    strings("Hollow Strings", "Hollow Lead", 0.4, uni=7, det=0.35, cutoff=4500)
    strings("Glass Strings", "Glass Bell", 0.6, uni=5, det=0.3, cutoff=8000,
            b=dict(table="Basic Shapes", pos=SAW, uni=5, det=0.35, level=0.4))
    strings("Vox Strings", "Vowel Morph", 0.4, uni=7, det=0.35, cutoff=6000)
    strings("Lofi Strings", "Basic Shapes", SAW, uni=5, det=0.4, cutoff=1800, rev=0.3).set(
        fx_dist_on=True, fx_dist_mode=DOWNS, fx_dist_drive=0.35, fx_dist_mix=0.5)
    strings("Epic Unison", uni=16, det=0.55, cutoff=4000, atk=0.5, rev=0.5, b=dict(table="Basic Shapes", pos=SAW,
            octave=-1, uni=9, det=0.5, level=0.4))
    strings("Soft Section", "Basic Shapes", TRI, uni=9, det=0.35, cutoff=2500, atk=0.6)
    strings("Tremolo Strings", uni=7, det=0.35, cutoff=3500).set(lfo3_sync=True, lfo3_div=DIV["1/16"],
            lfo3_shape=TRI_L, lfo3_mode=FREE).mod("Lfo3", "OscALevel", -0.4)
    strings("Spiccato", uni=5, det=0.3, short=True, decay=0.12, rel=0.08, cutoff=3500, chorus_on=False)
    strings("Wide Ensemble", uni=13, det=0.45, cutoff=3000).set(oscA_width=1.0)
    strings("Solo Violin", "Basic Shapes", SAW, uni=1, det=0.0, cutoff=4500, res=0.2, vib=0.05, atk=0.12,
            chorus_on=False).set(mono=True, legato=True, glide=0.08)
    strings("Rising Strings", uni=9, det=0.4, atk=2.5, rel=2.0, swell=True, cutoff=1000)
    strings("Arctic Strings", "Glass Bell", 0.2, uni=7, det=0.35, cutoff=9000, rev=0.55)
    strings("Saw Orchestra", uni=9, det=0.4, cutoff=2800, b=dict(table="Pulse Width", pos=0.3, octave=-1, uni=5,
            det=0.35, level=0.4))
    strings("Disco Strings", "Basic Shapes", SAW, uni=7, det=0.35, atk=0.08, cutoff=5000, dly=0.15, phaser_on=True)
    strings("Choir Strings", "Vowel Morph", 0.2, uni=9, det=0.4, cutoff=5000,
            b=dict(table="Basic Shapes", pos=SAW, uni=5, det=0.35, level=0.4))
    strings("Brass Section", "Basic Shapes", SAW, uni=5, det=0.25, cutoff=700, atk=0.06, swell=True,
            chorus_on=False, vib=0.02)
    strings("Synthwave Strings", "Basic Shapes", SAW, uni=7, det=0.4, cutoff=2200, phaser_on=True, dly=0.15)
    strings("Fold Strings", "Fold Sweep", 0.25, uni=7, det=0.35, cutoff=3500)
    strings("Distant Strings", uni=7, det=0.35, cutoff=1600, rev=0.6, atk=0.8, rel=2.5)


# ================================================================ SYNTHS (plucks, arps, stabs, chords, FX)
def pluck(name, table="Basic Shapes", pos=SAW, uni=5, det=0.35, decay=0.3, cutoff=900, env_amt=0.45, res=0.2,
          dly="1/8.", dly_mix=0.2, rev=0.25, b=None, pumped=False, filt=LP24):
    p = new("Synths", name)
    osc(p, "A", table, pos, uni=uni, det=det)
    if b:
        osc(p, "B", **b)
    amp(p, 0.001, decay * 1.5, 0.0, 0.25)
    flt(p, filt, cutoff, res, 0.0, 0.4)
    env2(p, 0.0, decay, 0.0, 0.2)
    p.mod("Env2", "FltCutoff", env_amt)
    p.mod("Velocity", "FltCutoff", 0.15)
    delay(p, dly, dly_mix, 0.4, 5000)
    reverb(p, rev, 0.65)
    if pumped:
        pump(p, 4, "1/4", 0.45)
    poly(p, 12)
    p.mod("Macro1", "FltCutoff", 0.3)
    return p


def arp_synth(name, table, pos, div="1/16", target="OscAPos", amount=0.6, shape=SH_L, decay=0.25, cutoff=3000,
              dly_div="1/8.", gate=False):
    p = new("Synths", name)
    osc(p, "A", table, pos, uni=3, det=0.2)
    amp(p, 0.001, decay, 0.0 if not gate else 1.0, 0.15)
    flt(p, LP24, cutoff, 0.3, 0.1, 0.4)
    lfo(p, 1, div=div, shape=shape, mode=FREE)
    p.mod("Lfo1", target, amount)
    if gate:
        lfo(p, 4, div=div, shape=SQUARE_L, mode=FREE)
        p.mod("Lfo4", "MasterVol", -0.6)
    delay(p, dly_div, 0.25, 0.4, 5000)
    reverb(p, 0.2, 0.6)
    poly(p, 10)
    p.mod("Macro1", "FltCutoff", 0.3)
    p.mod("Macro2", "Lfo1Rate", 0.2)
    return p


def build_synths():
    pluck("Big Room Pluck", uni=9, det=0.5, decay=0.25, cutoff=1200, rev=0.35)
    pluck("Future Bass Pluck", uni=7, det=0.45, decay=0.35, cutoff=700, env_amt=0.5, pumped=True)
    pluck("Trance Pluck", uni=5, det=0.35, decay=0.18, cutoff=600, env_amt=0.55, dly="1/16", dly_mix=0.25)
    pluck("Tropical Pluck", "Sine", 0.0, uni=1, decay=0.2, cutoff=5000, env_amt=0.0,
          b=dict(table="Glass Bell", pos=0.3, level=0.4))
    pluck("Marimba Pluck", "Basic Shapes", TRI, uni=1, decay=0.2, cutoff=2500, env_amt=0.3)
    pluck("Glass Pluck", "Glass Bell", 0.3, uni=1, decay=0.3, cutoff=1500, env_amt=0.45,
          b=dict(table="Sine", pos=0.0, octave=1, level=0.3))
    pluck("Square Pluck", "Basic Shapes", SQUARE, uni=3, det=0.2, decay=0.2, cutoff=800, res=0.3)
    pluck("Hollow Pluck", "Hollow Lead", 0.3, uni=3, det=0.25, decay=0.25, cutoff=1000)
    pluck("Metal Pluck", "Metal FM", 0.4, uni=3, det=0.2, decay=0.2, cutoff=2000, env_amt=0.4)
    pluck("Vowel Pluck", "Vowel Morph", 0.5, uni=3, det=0.2, decay=0.25, cutoff=1200, filt=BP12, res=0.2)
    pluck("Chord Pluck", "Chord Stack", 0.9, uni=3, det=0.25, decay=0.3, cutoff=1200, env_amt=0.45)
    pluck("Bit Pluck", "Bit Grit", 0.3, uni=1, decay=0.15, cutoff=3000, env_amt=0.3)

    p = new("Synths", "House Stab")
    osc(p, "A", "Organ Bars", 0.7, uni=3, det=0.2)
    osc(p, "B", "Chord Stack", 0.8, level=0.4)
    amp(p, 0.001, 0.3, 0.0, 0.15)
    flt(p, LP12, 2500, 0.2)
    env2(p, 0.0, 0.15)
    p.mod("Env2", "FltCutoff", 0.3)
    reverb(p, 0.3, 0.5)
    delay(p, "1/8.", 0.15, 0.3)
    poly(p, 12)
    macros(p, "FltCutoff", 0.3)

    p = new("Synths", "Rave Stab")
    osc(p, "A", "Basic Shapes", SAW, uni=7, det=0.5)
    osc(p, "B", "Basic Shapes", SQUARE, uni=3, det=0.3, octave=1, level=0.4)
    amp(p, 0.001, 0.25, 0.0, 0.2)
    flt(p, LP24, 1800, 0.35, 0.3)
    env2(p, 0.0, 0.12)
    p.mod("Env2", "FltCutoff", 0.35)
    dist(p, TUBE, 0.35)
    reverb(p, 0.3, 0.6)
    poly(p, 12)
    macros(p, "FltCutoff", 0.3)

    p = new("Synths", "Sidechain Chords")
    osc(p, "A", "Basic Shapes", SAW, uni=9, det=0.45)
    osc(p, "B", "Hollow Lead", 0.6, uni=5, det=0.35, octave=1, level=0.35)
    amp(p, 0.01, 1.0, 1.0, 0.3)
    flt(p, LP24, 4000, 0.15)
    pump(p, 4, "1/4", 0.6)
    reverb(p, 0.3, 0.7)
    poly(p, 10)
    macros(p, "FltCutoff", 0.3, "OscADetune", 0.3)

    p = new("Synths", "Gated Trance")
    osc(p, "A", "Basic Shapes", SAW, uni=9, det=0.45)
    amp(p, 0.005, 1.0, 1.0, 0.2)
    flt(p, LP24, 5000, 0.15)
    p.lfo_shape(4, [(0, 1, 0), (0.45, 1, 0), (0.5, 0, 0), (1, 0, 0)])
    lfo(p, 4, div="1/16", shape=CUSTOM_L, mode=RETRIG)
    p.v["lfo4_shape"] = CUSTOM_L
    p.mod("Lfo4", "MasterVol", 0.55)
    p.set(master_vol=0.15)
    delay(p, "1/8.", 0.2, 0.35)
    reverb(p, 0.3, 0.7)
    poly(p, 10)
    macros(p, "FltCutoff", 0.3)

    arp_synth("S&H Arp", "Digital Steps", 0.3)
    arp_synth("Random Bleeps", "Basic Shapes", SQUARE, target="FltCutoff", amount=0.4, cutoff=1500)
    arp_synth("Wavetable Arp", "Harmonic Sweep", 0.2, div="1/8", shape=SAWUP_L, amount=0.7, decay=0.4)
    arp_synth("Stutter Gate", "Basic Shapes", SAW, div="1/16", gate=True, shape=SINE_L, amount=0.0, cutoff=5000)
    arp_synth("Chiptune Arp", "Pulse Width", 0.1, div="1/32", shape=SQUARE_L, target="OscACoarse", amount=0.125,
              decay=0.3, cutoff=9000)

    p = new("Synths", "Laser Riser")
    osc(p, "A", "Basic Shapes", SAW, uni=7, det=0.4)
    noise(p, 0, 0.25, 0.7)
    amp(p, 0.01, 1.0, 1.0, 0.5)
    flt(p, LP24, 300, 0.4)
    lfo(p, 1, div="4bar", shape=SAWUP_L, mode=ENVMODE)
    p.mod("Lfo1", "FltCutoff", 0.65)
    p.mod("Lfo1", "OscACoarse", 0.12)
    reverb(p, 0.4, 0.85)
    poly(p, 6)
    macros(p, "FltCutoff", 0.3)

    p = new("Synths", "Noise Sweep")
    osc(p, "A", "Basic Shapes", SAW, level=0.0)
    noise(p, 0, 0.8, 0.5)
    amp(p, 0.01, 1.0, 1.0, 0.8)
    flt(p, BP24, 300, 0.5)
    lfo(p, 1, div="2bar", shape=TRI_L, mode=FREE)
    p.mod("Lfo1", "FltCutoff", 0.6)
    phaser(p, 0.2, 0.7, 0.4)
    reverb(p, 0.4, 0.8)
    poly(p, 4)
    macros(p, "FltCutoff", 0.3)

    p = new("Synths", "Zap Drop")
    osc(p, "A", "Squash Saw", 0.3, uni=3, det=0.3)
    amp(p, 0.001, 0.4, 0.0, 0.2)
    env3(p, 0.0, 0.3, 0.0, 0.2)
    p.mod("Env3", "OscACoarse", 0.35)
    p.mod("Env3", "OscAPos", 0.5)
    dist(p, FOLD, 0.3, 0.7)
    reverb(p, 0.25, 0.6)
    poly(p, 6)
    macros(p, "OscAPos", 0.3)

    p = new("Synths", "Supersaw Chords")
    osc(p, "A", "Basic Shapes", SAW, uni=11, det=0.55)
    osc(p, "B", "Basic Shapes", SAW, uni=7, det=0.5, octave=1, level=0.35)
    amp(p, 0.005, 1.0, 1.0, 0.35)
    flt(p, LP24, 6000, 0.1, 0.0, 0.3)
    reverb(p, 0.3, 0.75)
    delay(p, "1/8.", 0.12, 0.3)
    eq(p, 200, -3, 8000, 2)
    poly(p, 10)
    macros(p, "FltCutoff", 0.3, "OscADetune", 0.3)

    p = new("Synths", "Detroit Chords")
    osc(p, "A", "Basic Shapes", SAW, uni=3, det=0.25)
    osc(p, "B", "Basic Shapes", SQUARE, uni=3, det=0.2, level=0.5)
    amp(p, 0.002, 0.5, 0.3, 0.3)
    flt(p, LADDER, 900, 0.35, 0.2)
    env2(p, 0.0, 0.3, 0.1)
    p.mod("Env2", "FltCutoff", 0.3)
    delay(p, "1/4.", 0.25, 0.45, 3000)
    reverb(p, 0.35, 0.7)
    poly(p, 10)
    macros(p, "FltCutoff", 0.3, "FltRes", 0.3)

    p = new("Synths", "Growl Stab")
    osc(p, "A", "Talk Growl", 0.5, uni=3, det=0.2)
    amp(p, 0.001, 0.3, 0.0, 0.15)
    env2(p, 0.0, 0.25)
    p.mod("Env2", "OscAPos", -0.5)
    flt(p, VOWEL, 1500, 0.5)
    dist(p, FOLD, 0.25, 0.8, 0.6)
    comp(p, -20, 6)
    poly(p, 8)
    macros(p, "FltCutoff", 0.3)

    p = new("Synths", "Wonky Synth")
    osc(p, "A", "Basic Shapes", SQUARE, uni=3, det=0.25)
    amp(p, 0.003, 0.6, 0.7, 0.2)
    flt(p, LP24, 2000, 0.3)
    lfo(p, 1, rate=0.6, shape=DRIFT_L)
    p.mod("Lfo1", "OscAFine", 0.12, True)
    p.mod("Lfo1", "FltCutoff", 0.1, True)
    delay(p, "1/8T", 0.2, 0.35)
    poly(p, 8)
    macros(p, "FltCutoff", 0.3)

    arp_synth("Bell Arp", "Glass Bell", 0.2, div="1/16", shape=SAWUP_L, amount=0.6, decay=0.5, cutoff=8000)
    arp_synth("Formant Chop", "Talk Growl", 0.3, div="1/16", amount=0.8, decay=0.2, cutoff=6000)


def main():
    build_bass()
    build_leads()
    build_keys()
    build_pads()
    build_strings()
    build_synths()

    valid = param_ids()
    gains = json.load(open(GAINS)) if os.path.exists(GAINS) else {}
    names = set()
    for p in BANK:
        key = f"{p.cat}/{p.name}"
        assert p.name not in names, p.name
        names.add(p.name)
        p.v.setdefault("master_vol", 0.5)
        # The render test's level match scales the volume and any volume
        # modulation together, so pumps and gates keep their shape. Whatever
        # the volume knob can't reach goes to the compressor's output gain,
        # with ratio 1 (no compression) if the preset didn't use it already.
        k = gains.get(key, 1.0)
        base = p.v["master_vol"]
        peak = base + sum(max(0.0, m[2]) for m in p.mods if m[1] == "MasterVol")
        kv = min(k, 1.0 / max(peak, 1e-3))
        p.v["master_vol"] = round(base * kv, 4)
        p.mods = [(s_, d, round(a * kv, 4) if d == "MasterVol" else a, b) for s_, d, a, b in p.mods]
        extra_db = 20.0 * math.log10(k / kv)
        if extra_db > 0.05:
            if not p.v.get("fx_cmp_on"):
                comp(p, thresh=0.0, ratio=1.0)
            p.v["fx_cmp_gain"] = round(min(24.0, p.v.get("fx_cmp_gain", 0.0) + extra_db), 2)

    lines = []
    for p in BANK:
        r = p.recipe(valid)
        lines.append(f'    {{ "{p.cat}", "{p.name}",\n      "{r}" }},')
    out = ["// Generated by tools/make_presets.py. Edit that script, not this file.",
           '#include "FactoryPresets.h"', "#include <iterator>", "", "namespace cs", "{", "namespace",
           "{", "const FactoryPreset bank[] =", "{", *lines, "};", "}", "",
           "const FactoryPreset* factoryPresets() { return bank; }",
           "int numFactoryPresets() { return (int) std::size (bank); }", "} // namespace cs", ""]
    open(os.path.join(SRC, "FactoryPresets.cpp"), "w").write("\n".join(out))
    counts = {}
    for p in BANK:
        counts[p.cat] = counts.get(p.cat, 0) + 1
    print(len(BANK), "presets", counts)


if __name__ == "__main__":
    main()
