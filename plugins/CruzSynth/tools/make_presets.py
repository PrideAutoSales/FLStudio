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
TUBE, SOFT, HARD, FOLD, CRUSH, DOWNS, DIODE, LFOLD, ASYM, RECT, SQUASH, FUZZ, TAPE = range(13)
F_LP12, F_LP24, F_HP12, F_HP24, F_BP12, F_BP24, F_NOTCH, F_PEAK = range(8)        # FX filter types
HALL, PLATE, ROOM = range(3)
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
         "FltRes", "FltDrive", "FltMix", "Lfo1Rate", "Lfo2Rate", "Lfo3Rate", "Lfo4Rate", "MasterVol",
         "FxHypDetune", "FxHypMix", "FxHypWide", "FxDistDrive", "FxDistFreq", "FxDistMix",
         "FxFlaFeedback", "FxFlaMix", "FxPhaFreq", "FxPhaFeedback", "FxPhaMix",
         "FxChoDepth", "FxChoMix", "FxDlyFeedback", "FxDlyMix", "FxMbDepth",
         "FxEqLowGain", "FxEqHighGain", "FxFilCutoff", "FxFilRes", "FxFilDrive", "FxFilMix",
         "FxRevSize", "FxRevDecay", "FxRevMix", "PnoLevel", "PnoTone"]


def param_ids():
    """Every parameter ID createParameterLayout() makes, to catch typos here."""
    text = open(os.path.join(SRC, "Parameters.cpp")).read()
    ids = set(re.findall(r'add[34]?<AP\w>\s*\(L,\s*"(\w+)"', text))
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


def delay(p, div="1/8.", mix=0.2, fb=0.35, hicut=6000.0, pingpong=True, lowcut=None, offset=None):
    p.set(fx_dly_on=True, fx_dly_sync=True, fx_dly_div=DIV[div], fx_dly_mix=mix, fx_dly_feedback=fb,
          fx_dly_hicut=hicut, fx_dly_pingpong=pingpong)
    if lowcut is not None:
        p.set(fx_dly_lowcut=lowcut)
    if offset is not None:
        p.set(fx_dly_offset=offset)
    return p


def reverb(p, mix=0.25, size=0.6, damp=0.5, width=1.0, decay=None, mode=HALL, pre=0.0, lowcut=120.0,
           hicut=9000.0):
    """Decay follows the size unless given, so bigger rooms ring longer."""
    if decay is None:
        decay = round(0.5 + 4.0 * size * size, 2)
    p.set(fx_rev_on=True, fx_rev_mix=mix, fx_rev_size=size, fx_rev_damp=damp, fx_rev_width=width,
          fx_rev_decay=decay, fx_rev_lowcut=lowcut, fx_rev_hicut=hicut)
    if mode != HALL:
        p.set(fx_rev_mode=mode)
    if pre:
        p.set(fx_rev_predelay=pre)
    return p


def hyper(p, rate=0.8, detune=0.3, voices=4, mix=0.5, wide=0.0, size=0.5):
    return p.set(fx_hyp_on=True, fx_hyp_rate=rate, fx_hyp_detune=detune, fx_hyp_voices=voices, fx_hyp_mix=mix,
                 fx_hyp_wide=wide, fx_hyp_size=size)


def flanger(p, rate=0.3, depth=0.6, fb=0.5, mix=0.5, spread=0.25, div=None):
    p.set(fx_fla_on=True, fx_fla_rate=rate, fx_fla_depth=depth, fx_fla_feedback=fb, fx_fla_mix=mix,
          fx_fla_spread=spread)
    if div:
        p.set(fx_fla_sync=True, fx_fla_div=DIV[div])
    return p


def multiband(p, depth=0.6, time=0.5, up=0.4, down=1.0, low=0.0, mid=0.0, high=0.0, gain=0.0):
    return p.set(fx_mb_on=True, fx_mb_depth=depth, fx_mb_time=time, fx_mb_up=up, fx_mb_down=down, fx_mb_low=low,
                 fx_mb_mid=mid, fx_mb_high=high, fx_mb_gain=gain)


def fxfilter(p, kind=F_LP12, cutoff=2000.0, res=0.2, drive=0.0, stereo=0.0, mix=1.0):
    return p.set(fx_fil_on=True, fx_fil_type=kind, fx_fil_cutoff=cutoff, fx_fil_res=res, fx_fil_drive=drive,
                 fx_fil_stereo=stereo, fx_fil_mix=mix)


def space_macros(p):
    """Macro 3 opens the reverb and Macro 4 the delay, on the presets that use them."""
    if p.v.get("fx_rev_on"):
        p.mod("Macro3", "FxRevMix", 0.3)
    if p.v.get("fx_dly_on"):
        p.mod("Macro4", "FxDlyMix", 0.3)
    return p


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
    hyper(p, 0.7, 0.25, 4, 0.3, 0.35)
    delay(p, dly, 0.18, 0.35, 5000)
    reverb(p, rev, 0.7)
    eq(p, 200, -3, 8000, 2)
    if glide:
        mono(p, glide, legato=True)
    else:
        poly(p, 8)
    p.mod("Macro3", "FxHypMix", 0.4)
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


# ================================================================ DEEP HOUSE and MINIMAL
def bassline(cat, name, table, pos, cutoff=600, res=0.2, env_amt=0.3, decay=0.25, sus=0.8, glide=0.04,
             sub_lvl=0.5, drive=0.15, filt=LADDER, pitch=0.0, rel=0.08, octave=-1, uni=1, det=0.1, legato=False,
             b=None, dly=None, lfo_cut=0.0, lfo_div="1/4"):
    """A mono bassline voice: one oscillator, sub, enveloped filter, a touch of tape."""
    p = new(cat, name)
    osc(p, "A", table, pos, uni=uni, det=det, octave=octave, rand=0.0)
    if b:
        osc(p, "B", **b)
    if sub_lvl:
        sub(p, -1, sub_lvl)
    amp(p, 0.002, max(decay * 2.0, 0.3), sus, rel)
    flt(p, filt, cutoff, res, 0.2, 0.3)
    env2(p, 0.0, decay, 0.0, 0.1)
    p.mod("Env2", "FltCutoff", env_amt)
    p.mod("Velocity", "FltCutoff", 0.1)
    if pitch:
        env3(p, 0.0, 0.06, 0.0, 0.05)
        p.mod("Env3", "OscACoarse", pitch)
    if lfo_cut:
        lfo(p, 1, div=lfo_div, shape=SINE_L, mode=RETRIG)
        p.mod("Lfo1", "FltCutoff", lfo_cut, True)
    if drive:
        dist(p, TAPE, drive, 0.6)
    if dly:
        delay(p, dly, 0.12, 0.3, 3000, lowcut=300)
    comp(p, -20, 4, attack=3, release=80)
    mono(p, glide, legato)
    macros(p, "FltCutoff", 0.3, "FltRes", 0.3)
    return space_macros(p)


def part(cat, name, table, pos, uni=1, det=0.2, b=None, atk=0.002, dec=0.5, sus=0.0, rel=0.3, cutoff=3000,
         res=0.15, env_amt=0.3, fdec=None, filt=LP24, key=0.4, pitch=0.0, pdec=0.05, noise_lvl=0.0, noise_kind=0,
         chorus_on=False, phaser_on=False, flang=False, hyp=0.0, wide=0.0, tape=0.0, dly=None, dly_mix=0.2,
         dly_fb=0.4, rev=0.2, size=0.6, rev_mode=HALL, lfo_cut=0.0, lfo_div=None, lfo_rate=0.08, lfo_pos=0.0,
         tremolo=0.0, fxf=None, gate=None, mono_glide=None, vibrato=0.0, mb=0.0, warp=W_OFF, warp_amt=0.0,
         vel_pos=0.0, polyn=12):
    """The general voice behind the deep house and minimal banks: chords, stabs, keys, plucks, pads, leads
    and percussive blips, depending on the envelope times."""
    p = new(cat, name)
    osc(p, "A", table, pos, uni=uni, det=det, warp=warp, warp_amt=warp_amt)
    if b:
        osc(p, "B", **b)
    if noise_lvl:
        noise(p, noise_kind, noise_lvl, 0.6, True)
    amp(p, atk, dec, sus, rel)
    flt(p, filt, cutoff, res, 0.0, key)
    env2(p, 0.0, fdec if fdec is not None else dec * 0.7, 0.0, rel)
    p.mod("Env2", "FltCutoff", env_amt)
    p.mod("Velocity", "FltCutoff", 0.15)
    if pitch:
        env3(p, 0.0, pdec, 0.0, 0.05)
        p.mod("Env3", "OscACoarse", pitch)
        if b:
            p.mod("Env3", "OscBCoarse", pitch)
    if lfo_cut:
        if lfo_div:
            lfo(p, 2, div=lfo_div, shape=TRI_L)
        else:
            lfo(p, 2, rate=lfo_rate, shape=SINE_L)
        p.mod("Lfo2", "FltCutoff", lfo_cut, True)
    if lfo_pos:
        lfo(p, 1, rate=0.12, shape=DRIFT_L)
        p.mod("Lfo1", "OscAPos", lfo_pos)
    if tremolo:
        lfo(p, 3, rate=4.5)
        p.mod("Lfo3", "OscAPan", tremolo, True)
    if vibrato:
        lfo(p, 3, rate=5.2, rise=0.4)
        p.mod("Lfo3", "OscAFine", vibrato, True)
    if gate:
        lfo(p, 4, div=gate, shape=SQUARE_L, mode=RETRIG)
        p.mod("Lfo4", "MasterVol", -0.55)
    if vel_pos:
        p.mod("Velocity", "OscAPos", vel_pos)
    if tape:
        dist(p, TAPE, tape, 0.6, 0.7)
    if hyp or wide:
        hyper(p, 0.6, 0.25, 4, hyp, wide)
    if flang:
        flanger(p, 0.15, 0.5, 0.35, 0.35)
    if chorus_on:
        chorus(p, 0.45, 0.4, 0.4)
    if phaser_on:
        phaser(p, 0.2, 0.55, 0.35)
    if dly:
        delay(p, dly, dly_mix, dly_fb, 4000, lowcut=250)
    if rev:
        reverb(p, rev, size, mode=rev_mode)
    if mb:
        multiband(p, mb, 0.5, 0.4, 1.0)
    if fxf:
        fxfilter(p, **fxf)
    if mono_glide is not None:
        mono(p, mono_glide, True)
    else:
        poly(p, polyn)
    macros(p, "FltCutoff", 0.3, "OscAPos" if table not in ("Sine",) else "FltRes", 0.3)
    return space_macros(p)


def build_deep_house():
    c = "Deep House"
    bassline(c, "Nineties Organ Bass", "Organ Bars", 0.35, cutoff=1400, env_amt=0.2, decay=0.18, sus=0.5, sub_lvl=0.45,
             filt=LP12)
    bassline(c, "Warm Sub Bass", "Basic Shapes", TRI * 0.5, cutoff=900, env_amt=0.1, sus=0.9, glide=0.06,
             sub_lvl=0.3, drive=0.2)
    bassline(c, "Round Bass", "Basic Shapes", SAW, cutoff=380, res=0.3, env_amt=0.35, decay=0.2, sus=0.6)
    bassline(c, "Lazy Glide Bass", "Basic Shapes", SQUARE, cutoff=520, env_amt=0.25, glide=0.12, legato=True,
             sus=0.85)
    bassline(c, "Deep 808 Bass", "Sine", 0.0, cutoff=4000, env_amt=0.0, sus=0.6, decay=0.9, pitch=0.06,
             sub_lvl=0.0, drive=0.3, glide=0.05)
    bassline(c, "Velvet Pluck Bass", "Harmonic Sweep", 0.4, cutoff=300, env_amt=0.45, decay=0.14, sus=0.2,
             dly="1/8.")
    bassline(c, "Moody Bass", "Hollow Lead", 0.3, cutoff=650, res=0.25, env_amt=0.2, lfo_cut=0.08,
             lfo_div="1/2", sus=0.9)

    part(c, "Dub Stab Chord", "Chord Stack", 0.25, uni=2, det=0.15, dec=0.35, rel=0.25, cutoff=800, res=0.3,
         env_amt=0.35, dly="1/4.", dly_mix=0.3, dly_fb=0.55, rev=0.3, size=0.75, lfo_cut=0.12, lfo_div="4bar")
    part(c, "Minor Nine Stab", "Chord Stack", 0.6, uni=3, det=0.2, dec=0.3, rel=0.25, cutoff=1400, env_amt=0.4,
         chorus_on=True, dly="1/8.", dly_mix=0.2, rev=0.25)
    part(c, "Velvet Chords", "Basic Shapes", SAW, uni=5, det=0.3, atk=0.04, dec=1.5, sus=0.7, rel=0.6,
         cutoff=1800, env_amt=0.15, chorus_on=True, rev=0.35, size=0.75, hyp=0.25)
    part(c, "Late Night Rhodes", "Sine", 0.0, warp=W_FM, warp_amt=0.18, dec=2.2, sus=0.2, rel=0.6,
         b=dict(table="Sine", pos=0.0, octave=1, level=0.0), cutoff=4500, env_amt=0.2, tremolo=0.35,
         chorus_on=True, rev=0.25, tape=0.15)
    part(c, "Sunset Keys", "Glass Bell", 0.2, dec=1.6, sus=0.15, rel=0.5,
         b=dict(table="Basic Shapes", pos=TRI, level=0.5), cutoff=3500, env_amt=0.25, chorus_on=True,
         dly="1/8.", dly_mix=0.15, rev=0.3)
    part(c, "Soft House Pluck", "Basic Shapes", TRI, uni=3, det=0.2, dec=0.4, rel=0.3, cutoff=1200,
         env_amt=0.4, dly="1/8.", dly_mix=0.25, rev=0.25)
    part(c, "Marimba House", "Sine", 0.0, dec=0.3, rel=0.25, b=dict(table="Sine", pos=0.0, octave=2, level=0.25),
         cutoff=6000, env_amt=0.1, warp=W_FM, warp_amt=0.12, dly="1/8.", dly_mix=0.15, rev=0.2)
    part(c, "Filtered Strings", "Basic Shapes", SAW, uni=7, det=0.35, atk=0.5, dec=1.5, sus=0.9, rel=1.2,
         cutoff=700, env_amt=0.0, lfo_cut=0.15, lfo_div="8bar", chorus_on=True, rev=0.4, size=0.8)
    part(c, "Warm Deep Pad", "Hollow Lead", 0.4, uni=5, det=0.3, atk=0.9, dec=2.0, sus=0.85, rel=2.0,
         cutoff=1600, env_amt=0.0, lfo_pos=0.3, rev=0.45, size=0.85, hyp=0.3, wide=0.4)
    part(c, "Deep Pad Wash", "Breath", 0.5, uni=4, det=0.3, atk=1.5, dec=3.0, sus=0.9, rel=3.0, cutoff=2200,
         env_amt=0.0, lfo_pos=0.4, rev=0.6, size=0.95, rev_mode=HALL, wide=0.5, noise_lvl=0.03, noise_kind=1)
    part(c, "Vocal Chop Lead", "Vowel Morph", 0.3, dec=0.6, sus=0.4, rel=0.2, cutoff=4500, filt=LP12, res=0.25,
         env_amt=0.15, lfo_pos=0.35, dly="1/8.", dly_mix=0.25, rev=0.25, mono_glide=0.05, vel_pos=0.3)
    part(c, "Jazz Organ", "Organ Bars", 0.7, dec=0.3, sus=0.85, rel=0.12, cutoff=6000, env_amt=0.05,
         chorus_on=True, phaser_on=True, rev=0.2, size=0.4, rev_mode=ROOM)
    part(c, "Shuffle Stab", "Basic Shapes", SQUARE, uni=3, det=0.2, dec=0.18, rel=0.15, cutoff=1500,
         env_amt=0.45, dly="1/8T", dly_mix=0.25, rev=0.2)
    part(c, "House Brass Stab", "Basic Shapes", SAW, uni=5, det=0.25, atk=0.01, dec=0.45, sus=0.3, rel=0.2,
         cutoff=900, res=0.2, env_amt=0.45, fdec=0.25, rev=0.2, mb=0.4)
    part(c, "Bell Tone", "Glass Bell", 0.6, dec=1.2, rel=0.8, cutoff=9000, env_amt=0.1, dly="1/4.",
         dly_mix=0.2, rev=0.35)
    part(c, "Dusk Arp", "Pulse Width", 0.5, dec=0.25, rel=0.2, cutoff=1600, env_amt=0.4, lfo_cut=0.18,
         lfo_div="2bar", dly="1/8.", dly_mix=0.3, rev=0.25)
    part(c, "Air Chords", "Chord Stack", 0.8, uni=5, det=0.3, atk=0.3, dec=2.0, sus=0.8, rel=1.2, filt=HP12,
         cutoff=350, env_amt=0.0, chorus_on=True, rev=0.45, size=0.85, wide=0.4)
    part(c, "Tape Chords", "Chord Stack", 0.45, uni=2, det=0.15, atk=0.01, dec=1.0, sus=0.6, rel=0.5,
         cutoff=2000, env_amt=0.2, tape=0.35, flang=True, rev=0.3, lfo_pos=0.15)
    part(c, "Smooth Lead", "Basic Shapes", TRI, dec=1.0, sus=0.8, rel=0.3, cutoff=3500, env_amt=0.1,
         vibrato=0.04, dly="1/4.", dly_mix=0.25, rev=0.25, mono_glide=0.08)
    part(c, "Detroit Strings", "Basic Shapes", SAW, uni=7, det=0.4, atk=0.25, dec=1.5, sus=0.85, rel=0.9,
         cutoff=2500, env_amt=0.0, phaser_on=True, rev=0.35, size=0.75)


def build_minimal():
    c = "Minimal"
    part(c, "Minimal Blip", "Sine", 0.0, dec=0.08, rel=0.05, cutoff=8000, env_amt=0.0, pitch=0.1, pdec=0.02,
         dly="1/16", dly_mix=0.2, rev=0.0)
    bassline(c, "Rubber Band Bass", "Basic Shapes", TRI * 0.5, cutoff=1500, env_amt=0.1, decay=0.15, sus=0.4,
             pitch=0.05, sub_lvl=0.3, glide=0.1, legato=True)
    bassline(c, "Click Bass", "Basic Shapes", SQUARE, cutoff=260, res=0.35, env_amt=0.5, decay=0.06, sus=0.0,
             sub_lvl=0.5, drive=0.25)
    part(c, "Tick Perc", "Sine", 0.0, b=dict(table="Sine", pos=0.0, level=0.0), dec=0.04, rel=0.03, filt=HP12,
         cutoff=4000, env_amt=0.0, noise_lvl=0.6, noise_kind=0, rev=0.1, size=0.3, rev_mode=ROOM)
    part(c, "Wood Block", "Sine", 0.0, dec=0.07, rel=0.05, cutoff=9000, env_amt=0.0,
         warp=W_FM, warp_amt=0.3, b=dict(table="Sine", pos=0.0, octave=1, level=0.0), pitch=0.04, pdec=0.01,
         rev=0.12, size=0.3, rev_mode=ROOM)
    part(c, "Glass Ping", "Glass Bell", 0.5, dec=0.25, rel=0.2, cutoff=10000, env_amt=0.0, dly="1/16",
         dly_mix=0.3, dly_fb=0.45, rev=0.2)
    part(c, "Dub Tone", "Sine", 0.0, b=dict(table="Basic Shapes", pos=TRI, octave=-1, level=0.3), dec=0.5,
         rel=0.4, cutoff=1200, env_amt=0.2, dly="1/4.", dly_mix=0.35, dly_fb=0.6, rev=0.3, size=0.8)
    part(c, "Micro Pluck", "Basic Shapes", SAW, dec=0.09, rel=0.08, cutoff=700, env_amt=0.5, fdec=0.05,
         filt=LP12, dly="1/16", dly_mix=0.2, rev=0.15)
    p = part(c, "Step Bleeps", "Basic Shapes", SQUARE, dec=0.12, rel=0.08, cutoff=3000, env_amt=0.2,
             dly="1/8.", dly_mix=0.25, rev=0.2)
    lfo(p, 1, div="1/16", shape=SH_L, mode=FREE)
    p.mod("Lfo1", "OscACoarse", 0.125, True)
    part(c, "Metal Tick", "Metal FM", 0.4, dec=0.06, rel=0.05, cutoff=1500, env_amt=0.0, filt=HP12, key=0.0, rev=0.1,
         size=0.3, rev_mode=ROOM)
    part(c, "Resonant Zap", "Basic Shapes", SAW, dec=0.2, rel=0.1, cutoff=200, res=0.7, env_amt=0.65, fdec=0.08,
         filt=LADDER, dly="1/8", dly_mix=0.2)
    part(c, "Minimal Stab", "Chord Stack", 0.15, dec=0.12, rel=0.1, cutoff=1300, env_amt=0.4, fdec=0.08,
         dly="1/8.", dly_mix=0.3, dly_fb=0.5, rev=0.2)
    part(c, "Hollow Perc", "Hollow Lead", 0.5, dec=0.12, rel=0.1, cutoff=600, res=0.5, env_amt=0.3,
         filt=COMBP, rev=0.15, size=0.4, rev_mode=ROOM)
    part(c, "Comb Tom", "Basic Shapes", TRI, dec=0.25, rel=0.2, cutoff=180, res=0.6, env_amt=0.2,
         filt=COMBP, pitch=0.06, pdec=0.08, rev=0.15, size=0.4, rev_mode=ROOM)
    part(c, "Rim Synth", "Sine", 0.0, b=dict(table="Sine", pos=0.0, octave=2, level=0.5), dec=0.05, rel=0.04,
         cutoff=6000, env_amt=0.0, noise_lvl=0.25, rev=0.12, size=0.35, rev_mode=PLATE)
    bassline(c, "Deep Pulse Bass", "Pulse Width", 0.4, cutoff=700, env_amt=0.2, decay=0.2, sus=0.6,
             lfo_cut=0.06, lfo_div="1bar")
    bassline(c, "Minimal Acid Bass", "Basic Shapes", SAW, cutoff=300, res=0.55, env_amt=0.4, decay=0.12,
             sus=0.5, filt=LADDER, glide=0.06, dly="1/8.", drive=0.3)
    part(c, "Bit Blip", "Bit Grit", 0.3, dec=0.09, rel=0.06, cutoff=9000, env_amt=0.0, dly="1/16T",
         dly_mix=0.2, rev=0.1)
    part(c, "Vowel Pop", "Vowel Morph", 0.6, dec=0.15, rel=0.1, filt=VOWEL, cutoff=1200, res=0.5, env_amt=0.3,
         vel_pos=0.4, dly="1/8", dly_mix=0.2, rev=0.15)
    part(c, "Gated Chord", "Chord Stack", 0.35, uni=2, det=0.15, dec=1.0, sus=1.0, rel=0.1, cutoff=1800,
         env_amt=0.0, gate="1/16", rev=0.2, dly="1/8.", dly_mix=0.15)
    part(c, "Shaker Noise", "Sine", 0.0, b=dict(table="Sine", pos=0.0, level=0.0), dec=0.06, rel=0.05,
         filt=HP24, cutoff=6000, env_amt=0.0, noise_lvl=0.7, noise_kind=0, rev=0.12, size=0.3, rev_mode=ROOM)
    bassline(c, "Thump Bass", "Sine", 0.0, cutoff=3000, env_amt=0.0, decay=0.35, sus=0.3, pitch=0.12,
             sub_lvl=0.0, drive=0.35)
    part(c, "Ping Pong Drop", "Sine", 0.0, dec=0.15, rel=0.1, cutoff=6000, env_amt=0.0, pitch=-0.06, pdec=0.1,
         dly="1/8.", dly_mix=0.4, dly_fb=0.55, rev=0.2)
    part(c, "Phase Clicks", "Digital Steps", 0.6, dec=0.07, rel=0.05, cutoff=5000, env_amt=0.1,
         phaser_on=True, dly="1/16", dly_mix=0.2)
    p = part(c, "Tiny Arp", "Harmonic Sweep", 0.3, dec=0.1, rel=0.08, cutoff=2500, env_amt=0.3,
             dly="1/8.", dly_mix=0.25, rev=0.15)
    lfo(p, 1, div="1/16", shape=SH_L, mode=FREE)
    p.mod("Lfo1", "OscAPos", 0.6)
    p = part(c, "Echo Chord", "Chord Stack", 0.5, uni=2, det=0.15, dec=0.2, rel=0.2, cutoff=2500, env_amt=0.2,
             dly="1/4.", dly_mix=0.4, dly_fb=0.65, rev=0.3, size=0.8,
             fxf=dict(kind=F_BP12, cutoff=900, res=0.35, mix=0.8))
    lfo(p, 2, div="4bar", shape=SINE_L)
    p.mod("Lfo2", "FxFilCutoff", 0.25, True)


# ================================================================ PIANOS
GRAND, BRIGHT, UPRIGHT, FELT, TINE, REED = range(6)


def piano(cat, name, model, tone=0.0, dyn=0.5, sus=0.5, damper=0.5, strings=0.3, body=0.5, hammer=0.5, width=0.7,
          level=0.8, rev=0.0, size=0.6, mode=HALL, pre=0.0):
    """A modelled piano on its own: the oscillators are off and Env 1 just lets the strings ring.
    The grand gets 32 voices, so held pedal passages don't cut notes short."""
    p = new(cat, name)
    p.set(oscA_on=False, pno_on=True, pno_model=model, pno_tone=tone, pno_dynamics=dyn, pno_sustain=sus,
          pno_damper=damper, pno_strings=strings, pno_body=body, pno_hammer=hammer, pno_width=width,
          pno_level=level, vel_sens=0.0, poly=32 if model == GRAND else 16)
    amp(p, a=0.0, d=1.0, s=1.0, r=6.0)
    if rev:
        reverb(p, mix=rev, size=size, mode=mode, pre=pre, lowcut=150.0, hicut=8000.0)
    p.mod("Macro1", "PnoTone", 0.4)
    return p


def tremolo(p, div="1/8", depth=0.25):
    lfo(p, 4, div=div, shape=SINE_L, mode=FREE)
    return p.mod("Lfo4", "MasterVol", depth)


def build_pianos():
    c = "Pianos"
    space_macros(piano(c, "Concert Grand", GRAND, rev=0.16, size=0.7))
    space_macros(piano(c, "Grand Close Mic", GRAND, width=0.9, body=0.6, hammer=0.6, rev=0.07, size=0.35,
                       mode=ROOM))
    space_macros(eq(piano(c, "Ballad Grand", GRAND, tone=-0.2, sus=0.65, damper=0.6, rev=0.26, size=0.8,
                          pre=20.0), low_freq=150.0, low=1.5))
    space_macros(piano(c, "Cinematic Grand", GRAND, tone=-0.35, sus=0.75, damper=0.75, width=0.85, rev=0.4,
                       size=0.95, pre=35.0))
    p = piano(c, "Grand Felt Pedal", GRAND, tone=-0.6, dyn=0.35, sus=0.7, damper=0.8, hammer=0.3, rev=0.3, size=0.85)
    space_macros(delay(p, div="1/4.", mix=0.12, fb=0.3, hicut=3500.0))
    p = piano(c, "Pop Grand", BRIGHT, tone=0.1, dyn=0.4, rev=0.14, size=0.5, mode=PLATE)
    space_macros(eq(comp(p, thresh=-20.0, ratio=3.0, attack=10.0, release=150.0), high_freq=7000.0, high=2.0))
    space_macros(piano(c, "Bright Stage Grand", BRIGHT, tone=0.35, hammer=0.65, width=0.8, rev=0.12, size=0.45,
                       mode=ROOM))
    p = piano(c, "Dance Floor Piano", BRIGHT, tone=0.55, dyn=0.15, sus=0.35, damper=0.3, strings=0.4, body=0.35,
              rev=0.18, size=0.5, mode=PLATE)
    eq(comp(p, thresh=-24.0, ratio=4.0, attack=3.0, release=100.0), low_freq=220.0, low=-6.0, high_freq=5000.0,
       high=3.0)
    space_macros(delay(p, div="1/8.", mix=0.12, fb=0.25))
    space_macros(piano(c, "Studio Upright", UPRIGHT, rev=0.1, size=0.4, mode=ROOM))
    space_macros(piano(c, "Honky Tonk Upright", UPRIGHT, tone=0.25, strings=0.85, damper=0.4, rev=0.1, size=0.35,
                       mode=ROOM))
    p = piano(c, "Bedroom Upright", UPRIGHT, tone=-0.3, dyn=0.35, hammer=0.7, rev=0.12, size=0.35, mode=ROOM)
    dist(p, mode=TAPE, drive=0.25, tone=0.5, mix=0.6)
    space_macros(fxfilter(p, kind=F_LP12, cutoff=4500.0, res=0.1))
    space_macros(piano(c, "Felt Lullaby", FELT, sus=0.6, damper=0.6, rev=0.22, size=0.7))
    p = piano(c, "Felt Diary", FELT, tone=-0.2, hammer=0.8, width=0.5, rev=0.14, size=0.4, mode=ROOM)
    dist(p, mode=TAPE, drive=0.2, tone=0.45, mix=0.5)
    lfo(p, 3, rate=0.35, shape=DRIFT_L, mode=FREE)
    p.mod("Lfo3", "FxFilCutoff", 0.05, True)
    space_macros(fxfilter(p, kind=F_LP12, cutoff=3800.0, res=0.05))
    p = piano(c, "Ambient Felt Wash", FELT, tone=-0.1, sus=0.8, damper=0.85, width=0.9, rev=0.45, size=0.95,
              pre=40.0)
    space_macros(delay(p, div="1/4.", mix=0.18, fb=0.45, hicut=3000.0))
    p = piano(c, "Tine EP Classic", TINE, dyn=0.4, rev=0.12, size=0.5, mode=ROOM)
    space_macros(chorus(tremolo(p, div="1/8", depth=0.18), rate=0.5, depth=0.3, mix=0.3))
    space_macros(phaser(piano(c, "Tine EP Phaser", TINE, tone=0.2, rev=0.14, size=0.5, mode=PLATE),
                        rate=0.25, depth=0.6, mix=0.45, freq=700.0, fb=0.4))
    p = piano(c, "Soft Tine Keys", TINE, tone=-0.4, dyn=0.35, sus=0.6, rev=0.2, size=0.6)
    space_macros(chorus(p, rate=0.35, depth=0.35, mix=0.4))
    p = piano(c, "Reed EP Bark", REED, tone=0.35, dyn=0.45, rev=0.1, size=0.4, mode=ROOM)
    space_macros(dist(p, mode=TUBE, drive=0.2, tone=0.6, mix=0.5))
    p = piano(c, "Reed EP Tremolo", REED, rev=0.12, size=0.45, mode=ROOM)
    space_macros(tremolo(p, div="1/8T", depth=0.3))
    # A grand with a slow pad blooming underneath (Env 2 brings the pad in).
    p = piano(c, "Grand Pad Layer", GRAND, tone=-0.1, sus=0.6, rev=0.3, size=0.85)
    osc(p, "A", table="Basic Shapes", pos=SAW, uni=7, det=0.3, level=0.0, octave=0)
    flt(p, kind=LP24, cutoff=1400.0, res=0.1)
    env2(p, a=1.4, d=1.0, s=1.0, r=3.0)
    p.mod("Env2", "OscALevel", 0.35)
    space_macros(chorus(p, rate=0.3, depth=0.4, mix=0.35))
    p.set(pno_toFilter=False)


# ================================================================ MORE (0.6): modern sounds and classic machines
# Each bank below adds to its category. Presets named after a style ("Seventies Mono Lead", "Silver Box Acid")
# are this synth's own take on the sound of a classic instrument, made from scratch with these oscillators,
# filters and effects. No factory patch data from any other instrument is used.
ST = 1 / 96          # one semitone as a pitch modulation amount (Coarse spans 96 semitones)


def fm_pair(p, carrier="A", ratio_oct=0, ratio_semi=0, ratio_fine=0.0, index=0.2, mod_level=0.0):
    """Two-operator FM: Osc B is a sine modulator (silent unless mod_level), Osc A a sine carrier."""
    osc(p, carrier, "Sine", 0.0, warp=W_FM, warp_amt=index, rand=0.0)
    osc(p, "B", "Sine", 0.0, octave=ratio_oct, semi=ratio_semi, fine=ratio_fine, level=mod_level, rand=0.0)
    p.set(oscB_toFilter=bool(mod_level))
    return p


def build_bass_more():
    c = "Bass"
    # Three-oscillator ladder mono (saw, saw a few cents up, square an octave down), the classic seventies bass.
    p = new(c, "Fat Ladder Bass")
    osc(p, "A", "Basic Shapes", SAW, octave=-1, rand=0.0)
    osc(p, "B", "Basic Shapes", SAW, octave=-1, fine=7.0, level=0.7, rand=0.0)
    sub(p, -1, 0.5, shape=3)
    amp(p, 0.002, 0.6, 0.75, 0.08)
    flt(p, LADDER, 420, 0.3, 0.45, 0.35)
    env2(p, 0.0, 0.28, 0.25, 0.12)
    p.mod("Env2", "FltCutoff", 0.32)
    p.mod("Velocity", "FltCutoff", 0.1)
    dist(p, TUBE, 0.25, 0.65)
    mono(p, 0.04, legato=True)
    macros(p, "FltCutoff", 0.3, "FltRes", 0.35)

    # Small single-oscillator mono: saw plus a square sub, snappy envelope.
    p = new(c, "Little Mono Bass")
    osc(p, "A", "Basic Shapes", SAW, octave=-1, rand=0.0)
    sub(p, -1, 0.65, shape=3)
    amp(p, 0.001, 0.3, 0.55, 0.06)
    flt(p, LP24, 300, 0.35, 0.2, 0.4)
    env2(p, 0.0, 0.16, 0.0, 0.08)
    p.mod("Env2", "FltCutoff", 0.38)
    p.mod("Velocity", "FltCutoff", 0.12)
    comp(p, -18, 3, attack=4, release=90)
    mono(p, 0.0)
    macros(p, "FltCutoff", 0.3, "FltRes", 0.35)

    # The silver-box acid line: one saw, a squelchy 18 dB-ish resonance, accent on hard notes, slides on overlaps.
    p = new(c, "Silver Box Acid")
    osc(p, "A", "Basic Shapes", SAW, octave=-1, rand=0.0)
    amp(p, 0.001, 0.5, 0.85, 0.04)
    flt(p, LADDER, 260, 0.78, 0.3, 0.25)
    env2(p, 0.0, 0.22, 0.0, 0.08)
    p.mod("Env2", "FltCutoff", 0.4)
    p.mod("Velocity", "FltCutoff", 0.18)      # accent opens it
    p.mod("Velocity", "FltRes", 0.08)
    dist(p, TUBE, 0.5, 0.65)
    delay(p, "1/8.", 0.1, 0.3, 3500, lowcut=300)
    mono(p, 0.07, legato=True)
    macros(p, "FltCutoff", 0.35, "FltRes", 0.2)
    p.mod("ModWheel", "FxDistDrive", 0.35)

    p = new(c, "Silver Box Square")
    osc(p, "A", "Basic Shapes", SQUARE, octave=-1, rand=0.0)
    amp(p, 0.001, 0.5, 0.85, 0.04)
    flt(p, LADDER, 300, 0.72, 0.3, 0.25)
    env2(p, 0.0, 0.18, 0.0, 0.08)
    p.mod("Env2", "FltCutoff", 0.38)
    p.mod("Velocity", "FltCutoff", 0.16)
    dist(p, DIODE, 0.4, 0.6)
    delay(p, "1/16", 0.12, 0.35, 3000, lowcut=300)
    mono(p, 0.06, legato=True)
    macros(p, "FltCutoff", 0.35, "FltRes", 0.2)

    # Eighties FM slap bass: sine carrier, 1:1 modulator whose index falls fast; harder notes snap more.
    p = new(c, "FM Slap Bass")
    fm_pair(p, ratio_oct=0, index=0.12)
    p.set(oscA_octave=-1, oscB_octave=-1)
    sub(p, -1, 0.25)
    amp(p, 0.001, 0.9, 0.35, 0.1)
    env2(p, 0.0, 0.12, 0.1, 0.1)
    p.mod("Env2", "OscAWarp", 0.32)
    p.mod("Velocity", "OscAWarp", 0.12)
    env3(p, 0.0, 0.025, 0.0, 0.02)
    p.mod("Env3", "OscACoarse", 0.2 * ST * 12)  # a tiny thwack at the start
    comp(p, -18, 4, attack=2, release=80)
    eq(p, 90, 2, 3500, 2)
    mono(p, 0.0)
    macros(p, "OscAWarp", 0.25, "OscBFine", 0.1)

    # Bass pedals: a deep, sustained two-oscillator drone with a soft filter bloom.
    p = new(c, "Pedal Synth Bass")
    osc(p, "A", "Basic Shapes", SAW, octave=-1, rand=0.0)
    osc(p, "B", "Basic Shapes", SQUARE, octave=-1, fine=-6.0, level=0.6, rand=0.0)
    sub(p, -1, 0.7)
    amp(p, 0.01, 1.0, 1.0, 0.35)
    flt(p, LP24, 260, 0.2, 0.25, 0.2)
    env2(p, 0.02, 0.6, 0.3, 0.3)
    p.mod("Env2", "FltCutoff", 0.2)
    chorus(p, 0.25, 0.3, 0.25)
    mono(p, 0.05, legato=True)
    macros(p, "FltCutoff", 0.3, "OscBFine", 0.15)

    # Log drum: a pitched, woody bass hit (sine with a fast pitch drop, a little FM bark and a resonant knock).
    p = new(c, "Log Drum")
    osc(p, "A", "Sine", 0.0, warp=W_FM, warp_amt=0.06, rand=0.0)
    osc(p, "B", "Sine", 0.0, octave=1, semi=7, level=0.0, rand=0.0)
    p.set(oscB_toFilter=False)
    amp(p, 0.001, 0.45, 0.0, 0.12)
    env3(p, 0.0, 0.045, 0.0, 0.03)
    p.mod("Env3", "OscACoarse", 9 * ST)
    p.mod("Env3", "OscAWarp", 0.22)
    p.mod("Velocity", "OscAWarp", 0.08)
    flt(p, PEAK, 900, 0.35, 0.0, 0.6)
    dist(p, TUBE, 0.35, 0.6)
    reverb(p, 0.08, 0.35, mode=ROOM)
    mono(p, 0.05)
    macros(p, "OscAWarp", 0.2, "FltCutoff", 0.25)

    # Drill 808: long sine with punch, soft clipped, glides on overlapping notes.
    p = new(c, "Drill Slide 808")
    osc(p, "A", "Basic Shapes", SINE, octave=-1, rand=0.0)
    amp(p, 0.001, 2.2, 0.0, 0.25)
    p.set(env1_curve=0.35)
    env3(p, 0.0, 0.05, 0.0, 0.05)
    p.mod("Env3", "OscACoarse", 0.1)
    dist(p, SOFT, 0.45, 0.55)
    eq(p, 60, 1.5, 3000, 1)
    mono(p, 0.14, legato=True)
    macros(p, "FxDistDrive", 0.35, "OscAPos", 0.12)

    # Riddim: a squared-off, comb-hollowed square bass that chops on the beat.
    p = new(c, "Riddim Square")
    osc(p, "A", "Basic Shapes", SQUARE, uni=2, det=0.08, octave=-1, width=0.3, rand=0.0)
    sub(p, -1, 0.5)
    amp(p, 0.001, 1.0, 1.0, 0.06)
    flt(p, NOTCH, 600, 0.45, 0.2)
    lfo(p, 1, div="1/8T", shape=SQUARE_L, mode=RETRIG)
    p.mod("Lfo1", "FltCutoff", 0.25)
    dist(p, HARD, 0.45, 0.7)
    comp(p, -24, 8, multi=True, upward=0.45, attack=1, release=60)
    mono(p, 0.0)
    macros(p, "FltCutoff", 0.25, "Lfo1Rate", 0.0)
    p.mod("ModWheel", "FltRes", 0.3)

    # Psy rolling bass: very short, tight saw pluck, made for off-beat sixteenths.
    p = new(c, "Psy Rolling Bass")
    osc(p, "A", "Basic Shapes", SAW, octave=-1, rand=0.0)
    sub(p, -1, 0.35)
    amp(p, 0.001, 0.12, 0.0, 0.03)
    flt(p, LP24, 220, 0.3, 0.2, 0.5)
    env2(p, 0.0, 0.07, 0.0, 0.03)
    p.mod("Env2", "FltCutoff", 0.45)
    p.mod("Velocity", "FltCutoff", 0.1)
    comp(p, -18, 4, attack=1, release=50)
    mono(p, 0.0)
    macros(p, "FltCutoff", 0.3, "FltRes", 0.3)

    # Hard techno rumble: a distorted, reverb-soaked low thump, low-passed and ducked on the beat.
    p = new(c, "Techno Rumble")
    osc(p, "A", "Sine", 0.0, octave=-1, rand=0.0)
    amp(p, 0.001, 0.7, 0.0, 0.5)
    env3(p, 0.0, 0.06, 0.0, 0.05)
    p.mod("Env3", "OscACoarse", 0.14)
    dist(p, TUBE, 0.6, 0.45)
    reverb(p, 0.55, 0.55, damp=0.7, mode=ROOM, lowcut=30, hicut=2500)
    fxfilter(p, F_LP24, 220, 0.25, 0.2)
    p.fx = [7, 0, 4, 10, 8, 1, 2, 3, 5, 9, 6]
    mono(p, 0.0)
    macros(p, "FxFilCutoff", 0.25, "FxRevMix", 0.3)

    # Funk mono with a vocal "bwow": square-ish, envelope swept band of resonance, quick glide.
    p = new(c, "Seventies Funk Bass")
    osc(p, "A", "Pulse Width", 0.45, octave=-1, rand=0.0)
    osc(p, "B", "Basic Shapes", SAW, octave=-1, fine=-5.0, level=0.5, rand=0.0)
    amp(p, 0.002, 0.5, 0.6, 0.08)
    flt(p, LADDER, 240, 0.55, 0.35, 0.3)
    env2(p, 0.01, 0.32, 0.1, 0.1)
    p.mod("Env2", "FltCutoff", 0.42)
    p.mod("Velocity", "FltCutoff", 0.12)
    mono(p, 0.05, legato=True)
    macros(p, "FltCutoff", 0.3, "FltRes", 0.3)

    # West coast: a sine run through a wavefolder whose depth follows an envelope, like a low pass gate.
    p = new(c, "Wavefold Bass")
    osc(p, "A", "Sine", 0.0, octave=-1, rand=0.0)
    osc(p, "B", "Basic Shapes", TRI, octave=-1, level=0.4, rand=0.0)
    amp(p, 0.001, 0.5, 0.55, 0.12)
    dist(p, FOLD, 0.15, 0.75, 1.0)
    env2(p, 0.0, 0.25, 0.2, 0.1)
    p.mod("Env2", "FxDistDrive", 0.35)
    p.mod("Velocity", "FxDistDrive", 0.15)
    flt(p, LP12, 1800, 0.1, 0.0, 0.5)
    comp(p, -18, 3)
    mono(p, 0.03)
    macros(p, "FxDistDrive", 0.3, "OscBLevel", 0.3)

    # Speed garage: a detuned saw bass that wobbles a quarter note and drops into each note.
    p = new(c, "Speed Garage Bass")
    osc(p, "A", "Basic Shapes", SAW, uni=2, det=0.3, octave=-1, width=0.3, rand=0.0)
    osc(p, "B", "Basic Shapes", SAW, octave=-1, fine=12.0, level=0.6, rand=0.0)
    sub(p, -1, 0.6)
    amp(p, 0.002, 1.0, 1.0, 0.1)
    flt(p, LP24, 380, 0.4, 0.3)
    lfo(p, 1, div="1/4", shape=SINE_L, mode=RETRIG, phase=0.25)
    p.mod("Lfo1", "FltCutoff", 0.18, True)
    env3(p, 0.0, 0.08, 0.0, 0.05)
    p.mod("Env3", "OscACoarse", 2 * ST)
    p.mod("Env3", "OscBCoarse", 2 * ST)
    dist(p, TUBE, 0.3, 0.6)
    mono(p, 0.06, legato=True)
    macros(p, "FltCutoff", 0.3, "Lfo1Rate", 0.0)

    # Bass house: a mid-range talking bass, the vowel filter riding a quarter-note envelope.
    p = new(c, "Bass House Wah")
    osc(p, "A", "Squash Saw", 0.25, uni=2, det=0.1, octave=-1, width=0.3, rand=0.0)
    sub(p, -1, 0.55)
    amp(p, 0.001, 0.6, 0.7, 0.06)
    flt(p, VOWEL, 300, 0.45, 0.3)
    env2(p, 0.0, 0.22, 0.15, 0.08)
    p.mod("Env2", "FltCutoff", 0.4)
    p.mod("Env2", "OscAPos", 0.3)
    dist(p, FOLD, 0.25, 0.75, 0.6)
    comp(p, -22, 6, multi=True, upward=0.35, attack=2, release=70)
    mono(p, 0.02)
    macros(p, "FltCutoff", 0.3, "OscAPos", 0.35)

    # Synth upright: triangle and sine with a woody band-pass knock, finger-plucked decay.
    p = new(c, "Synth Upright Bass")
    osc(p, "A", "Basic Shapes", TRI, octave=-1, rand=0.0)
    osc(p, "B", "Sine", 0.0, octave=-1, level=0.5, rand=0.0)
    noise(p, 1, 0.05, 0.4)
    amp(p, 0.004, 1.2, 0.0, 0.15)
    p.set(env1_curve=0.3)
    flt(p, LP12, 900, 0.15, 0.0, 0.5)
    env2(p, 0.0, 0.08, 0.0, 0.05)
    p.mod("Env2", "FltCutoff", 0.25)
    p.mod("Env2", "NoiseLevel", 0.25)
    p.mod("Velocity", "FltCutoff", 0.15)
    reverb(p, 0.08, 0.3, mode=ROOM)
    mono(p, 0.02)
    p.set(vel_sens=0.6)
    macros(p, "FltCutoff", 0.3)

    # Dark synthwave: octave-stacked saw bass with a quick filter snap for running sixteenths.
    p = new(c, "Night Drive Bass")
    osc(p, "A", "Basic Shapes", SAW, uni=3, det=0.2, octave=-1, width=0.3, rand=0.0)
    osc(p, "B", "Basic Shapes", SAW, octave=0, level=0.3, rand=0.0)
    sub(p, -1, 0.5)
    amp(p, 0.001, 0.3, 0.4, 0.06)
    flt(p, LP24, 250, 0.3, 0.25, 0.3)
    env2(p, 0.0, 0.14, 0.05, 0.06)
    p.mod("Env2", "FltCutoff", 0.4)
    chorus(p, 0.4, 0.25, 0.2)
    comp(p, -18, 4, attack=3, release=80)
    mono(p, 0.0)
    macros(p, "FltCutoff", 0.3, "OscADetune", 0.3)

    # Kick and bass in one: a short punchy sub thump with a click on top, for kick-bass grooves.
    p = new(c, "Kick Bass Thump")
    osc(p, "A", "Sine", 0.0, octave=-1, rand=0.0)
    noise(p, 0, 0.12, 0.8, False)
    amp(p, 0.0, 0.28, 0.25, 0.1)
    env3(p, 0.0, 0.035, 0.0, 0.03)
    p.mod("Env3", "OscACoarse", 0.25)
    p.mod("Env3", "NoiseLevel", 0.2)
    p.set(noise_level=0.0)
    dist(p, SOFT, 0.3, 0.6)
    comp(p, -16, 4, attack=1, release=60)
    mono(p, 0.0)
    macros(p, "FxDistDrive", 0.3)


def build_leads_more():
    c = "Leads"
    p = new(c, "Seventies Mono Lead")
    osc(p, "A", "Basic Shapes", SAW, rand=0.0)
    osc(p, "B", "Basic Shapes", SAW, fine=8.0, level=0.7, rand=0.0)
    sub(p, -1, 0.3, shape=3)
    amp(p, 0.004, 0.8, 0.9, 0.18)
    flt(p, LADDER, 1100, 0.3, 0.45, 0.45)
    env2(p, 0.02, 0.45, 0.35, 0.2)
    p.mod("Env2", "FltCutoff", 0.25)
    lfo(p, 2, rate=5.6, rise=0.3)
    p.mod("ModWheel", "OscAFine", 0.0)
    p.mod("Lfo2", "OscAFine", 0.03, True)
    p.mod("Lfo2", "OscBFine", 0.03, True)
    dist(p, TUBE, 0.2, 0.7)
    delay(p, "1/8.", 0.12, 0.3, 4500)
    reverb(p, 0.12, 0.55)
    mono(p, 0.09, legato=True)
    macros(p, "FltCutoff", 0.3, "FltRes", 0.3)

    # Hard-sync sweep: the slave saw's pitch sweeps down after each attack.
    p = new(c, "Sync Sweep Lead")
    osc(p, "A", "Saw", 0.0, warp=W_SYNC, warp_amt=0.15, rand=0.0)
    osc(p, "B", "Basic Shapes", SQUARE, octave=-1, level=0.35, rand=0.0)
    amp(p, 0.002, 0.8, 0.85, 0.2)
    flt(p, LP24, 6000, 0.15, 0.1, 0.4)
    env2(p, 0.0, 0.5, 0.15, 0.3)
    p.mod("Env2", "OscAWarp", 0.4)
    p.mod("ModWheel", "OscAWarp", 0.3)
    lfo(p, 2, rate=5.5, rise=0.5)
    p.mod("Lfo2", "OscAFine", 0.025, True)
    chorus(p, 0.4, 0.25, 0.25)
    delay(p, "1/8.", 0.15, 0.35, 5000)
    reverb(p, 0.15, 0.6)
    mono(p, 0.05, legato=True)
    macros(p, "FltCutoff", 0.3, "OscAWarp", 0.4)

    # Two screaming filters: a resonant high pass into a driven low pass.
    p = new(c, "Dual Filter Scream")
    osc(p, "A", "Basic Shapes", SAW, rand=0.0)
    osc(p, "B", "Pulse Width", 0.3, fine=-6.0, level=0.6, rand=0.0)
    amp(p, 0.003, 1.0, 1.0, 0.15)
    fxfilter(p, F_HP12, 400, 0.65, 0.3)
    flt(p, LP24, 1600, 0.7, 0.55, 0.4)
    env2(p, 0.0, 0.4, 0.3, 0.2)
    p.mod("Env2", "FltCutoff", 0.25)
    p.mod("Aftertouch", "FltCutoff", 0.2)
    dist(p, TUBE, 0.35, 0.6)
    reverb(p, 0.1, 0.5)
    p.fx = [10, 7, 0, 8, 1, 2, 3, 4, 5, 9, 6]
    mono(p, 0.04, legato=True)
    macros(p, "FltCutoff", 0.3, "FxFilCutoff", 0.3)

    p = new(c, "Virtual Analog Lead")
    osc(p, "A", "Basic Shapes", SAW, uni=2, det=0.12, warp=W_SYNC, warp_amt=0.2, rand=0.0)
    osc(p, "B", "Basic Shapes", SAW, fine=-9.0, level=0.6, rand=0.0)
    amp(p, 0.002, 0.7, 0.8, 0.2)
    flt(p, LP12, 3500, 0.25, 0.1, 0.5)
    env2(p, 0.0, 0.3, 0.2, 0.2)
    p.mod("Env2", "OscAWarp", 0.25)
    p.mod("Env2", "FltCutoff", 0.15)
    lfo(p, 2, rate=5.8, rise=0.4)
    p.mod("Lfo2", "OscAFine", 0.03, True)
    delay(p, "1/4", 0.12, 0.3, 5000)
    reverb(p, 0.15, 0.6)
    mono(p, 0.03, legato=True)
    macros(p, "FltCutoff", 0.3, "OscAWarp", 0.35)

    # Fuzzed, gliding saw lead of current rap production.
    p = new(c, "Rage Fuzz Lead")
    osc(p, "A", "Basic Shapes", SAW, uni=5, det=0.3, rand=1.0)
    osc(p, "B", "Basic Shapes", SQUARE, uni=3, det=0.25, octave=1, level=0.3)
    amp(p, 0.002, 1.0, 1.0, 0.25)
    flt(p, LP24, 5000, 0.15, 0.1, 0.3)
    dist(p, FUZZ, 0.55, 0.6, 0.85)
    chorus(p, 0.5, 0.3, 0.3)
    reverb(p, 0.18, 0.6)
    eq(p, 250, -4, 6000, -2)
    mono(p, 0.12, legato=True)
    p.mod("PitchBend", "FxDistDrive", 0.2)
    macros(p, "FxDistDrive", 0.3, "OscADetune", 0.3)

    # Hyperpop: bright, pitched-up stack with a hard-clipped edge and a quick upward scoop.
    p = new(c, "Hyperpop Lead")
    osc(p, "A", "Basic Shapes", SAW, uni=7, det=0.35)
    osc(p, "B", "Basic Shapes", SQUARE, uni=3, det=0.2, octave=1, level=0.45)
    amp(p, 0.001, 0.6, 0.85, 0.18)
    env3(p, 0.0, 0.06, 0.0, 0.05)
    p.mod("Env3", "OscACoarse", -1 * ST)
    p.mod("Env3", "OscBCoarse", -1 * ST)
    flt(p, LP12, 9000, 0.1)
    dist(p, HARD, 0.35, 0.85, 0.6)
    multiband(p, 0.5, 0.4, 0.5, 1.0)
    delay(p, "1/16", 0.12, 0.3, 7000)
    reverb(p, 0.15, 0.5, mode=PLATE)
    poly(p, 8)
    macros(p, "FltCutoff", 0.3, "OscADetune", 0.3)

    # Melodic techno: a hollow sync lead, filter breathing over four bars, big space.
    p = new(c, "Afterhours Lead")
    osc(p, "A", "Basic Shapes", SAW, uni=3, det=0.18, warp=W_SYNC, warp_amt=0.12)
    osc(p, "B", "Basic Shapes", SQUARE, octave=-1, level=0.4, rand=0.0)
    amp(p, 0.004, 1.0, 1.0, 0.35)
    flt(p, LP24, 1300, 0.35, 0.2, 0.4)
    lfo(p, 1, div="4bar", shape=TRI_L)
    p.mod("Lfo1", "FltCutoff", 0.12, True)
    p.mod("Lfo1", "OscAWarp", 0.15)
    delay(p, "1/4.", 0.25, 0.5, 3000, lowcut=300)
    reverb(p, 0.3, 0.85, pre=20.0)
    mono(p, 0.08, legato=True)
    macros(p, "FltCutoff", 0.3, "OscAWarp", 0.3)

    p = new(c, "Night Drive Lead")
    osc(p, "A", "Basic Shapes", SAW, uni=3, det=0.22)
    osc(p, "B", "Pulse Width", 0.35, warp=W_PWM, warp_amt=0.2, level=0.55)
    amp(p, 0.004, 0.8, 0.85, 0.3)
    flt(p, LP24, 3200, 0.2, 0.1, 0.4)
    lfo(p, 1, rate=0.4, shape=TRI_L)
    p.mod("Lfo1", "OscBWarp", 0.2)
    lfo(p, 2, rate=5.4, rise=0.6)
    p.mod("Lfo2", "OscAFine", 0.03, True)
    chorus(p, 0.5, 0.45, 0.4)
    delay(p, "1/4.", 0.22, 0.4, 4000)
    reverb(p, 0.25, 0.75)
    mono(p, 0.07, legato=True)
    macros(p, "FltCutoff", 0.3, "OscBWarp", 0.3)

    # G-funk whistle: a sine-ish portamento lead with a slow vibrato.
    p = new(c, "West Coast Whistle")
    osc(p, "A", "Basic Shapes", TRI * 0.45, rand=0.0)
    amp(p, 0.01, 1.0, 1.0, 0.25)
    flt(p, LP12, 4500, 0.1, 0.0, 0.5)
    lfo(p, 2, rate=5.0, rise=0.35)
    p.mod("Lfo2", "OscAFine", 0.05, True)
    p.mod("ModWheel", "OscAPos", 0.2)
    chorus(p, 0.3, 0.2, 0.2)
    delay(p, "1/4", 0.15, 0.3, 4000)
    reverb(p, 0.2, 0.6)
    mono(p, 0.16, legato=True)
    macros(p, "OscAPos", 0.25, "Lfo2Rate", 0.15)

    p = new(c, "Classic Trance Lead")
    osc(p, "A", "Basic Shapes", SAW, uni=8, det=0.42)
    osc(p, "B", "Basic Shapes", SQUARE, uni=4, det=0.3, octave=1, level=0.3)
    amp(p, 0.002, 0.5, 0.8, 0.35)
    flt(p, LP24, 6500, 0.15, 0.0, 0.3)
    lfo(p, 2, rate=5.6, rise=0.6)
    p.mod("Lfo2", "OscAFine", 0.03, True)
    delay(p, "1/8.", 0.22, 0.4, 5500)
    reverb(p, 0.28, 0.8)
    eq(p, 250, -3, 9000, 2)
    poly(p, 8)
    macros(p, "FltCutoff", 0.3, "OscADetune", 0.3)

    p = new(c, "Fuzz Square Lead")
    osc(p, "A", "Basic Shapes", SQUARE, uni=2, det=0.1)
    amp(p, 0.002, 1.0, 1.0, 0.15)
    flt(p, LP24, 3200, 0.25, 0.2, 0.4)
    dist(p, FUZZ, 0.5, 0.55, 0.9)
    lfo(p, 2, rate=5.5, rise=0.4)
    p.mod("ModWheel", "OscAFine", 0.0)
    p.mod("Lfo2", "OscAFine", 0.035, True)
    delay(p, "1/8.", 0.12, 0.3, 4500)
    reverb(p, 0.12, 0.5)
    mono(p, 0.04, legato=True)
    macros(p, "FltCutoff", 0.3, "FxDistDrive", 0.3)

    # Breath-controlled style lead: the mod wheel or aftertouch blows it open.
    p = new(c, "Breath Lead")
    osc(p, "A", "Breath", 0.35, uni=2, det=0.08)
    noise(p, 1, 0.06, 0.6)
    amp(p, 0.04, 1.0, 1.0, 0.2)
    flt(p, LP12, 1500, 0.2, 0.0, 0.6)
    p.mod("ModWheel", "FltCutoff", 0.25)
    p.mod("Aftertouch", "FltCutoff", 0.2)
    p.mod("Velocity", "FltCutoff", 0.15)
    lfo(p, 2, rate=5.2, rise=0.5)
    p.mod("Lfo2", "OscAFine", 0.03, True)
    reverb(p, 0.2, 0.6, mode=PLATE)
    mono(p, 0.06, legato=True)
    macros(p, "FltCutoff", 0.3, "OscAPos", 0.3)

    # Modular sine lead: FM bell attack that softens into a pure tone, with a slow random drift.
    p = new(c, "Patch Cord Lead")
    fm_pair(p, ratio_oct=1, index=0.05)
    amp(p, 0.005, 1.0, 1.0, 0.3)
    env2(p, 0.0, 0.35, 0.05, 0.2)
    p.mod("Env2", "OscAWarp", 0.3)
    lfo(p, 1, rate=0.3, shape=DRIFT_L)
    p.mod("Lfo1", "OscAFine", 0.04, True)
    p.mod("ModWheel", "OscAWarp", 0.3)
    delay(p, "1/4.", 0.22, 0.45, 3500)
    reverb(p, 0.22, 0.7)
    mono(p, 0.05, legato=True)
    macros(p, "OscAWarp", 0.3, "OscBFine", 0.2)

    p = new(c, "Pluck Lead Dutch")
    osc(p, "A", "Basic Shapes", SAW, uni=5, det=0.3)
    osc(p, "B", "Basic Shapes", SQUARE, octave=1, level=0.35)
    amp(p, 0.001, 0.35, 0.3, 0.2)
    flt(p, LP24, 1800, 0.35, 0.2, 0.4)
    env2(p, 0.0, 0.12, 0.0, 0.1)
    p.mod("Env2", "FltCutoff", 0.35)
    env3(p, 0.0, 0.05, 0.0, 0.03)
    p.mod("Env3", "OscACoarse", 12 * ST)
    dist(p, HARD, 0.3, 0.8, 0.5)
    reverb(p, 0.2, 0.55)
    mono(p, 0.0)
    macros(p, "FltCutoff", 0.3, "FltRes", 0.3)

    p = new(c, "Digital Wave Lead")
    osc(p, "A", "Digital Steps", 0.2, uni=2, det=0.12)
    amp(p, 0.003, 1.0, 0.9, 0.25)
    env2(p, 0.0, 0.6, 0.3, 0.3)
    p.mod("Env2", "OscAPos", 0.45)
    lfo(p, 2, rate=5.5, rise=0.5)
    p.mod("Lfo2", "OscAFine", 0.03, True)
    chorus(p, 0.4, 0.3, 0.3)
    delay(p, "1/8.", 0.18, 0.35, 5000)
    reverb(p, 0.2, 0.6)
    mono(p, 0.04, legato=True)
    macros(p, "OscAPos", 0.3, "OscADetune", 0.3)


def build_keys_more():
    c = "Keys"
    # Eighties FM electric piano: a sine body with a short, high, inharmonic "tine" ping on top.
    p = new(c, "FM Tine Piano")
    osc(p, "A", "Sine", 0.0, warp=W_FM, warp_amt=0.03, rand=0.0)
    osc(p, "B", "Sine", 0.0, octave=3, semi=10, level=0.0, rand=0.0)  # about 14:1, the tine
    amp(p, 0.001, 2.6, 0.0, 0.5)
    p.set(env1_curve=0.3)
    env2(p, 0.0, 0.22, 0.0, 0.2)
    p.mod("Env2", "OscAWarp", 0.1)
    p.mod("Velocity", "OscAWarp", 0.08)
    env3(p, 0.0, 0.06, 0.0, 0.06)
    p.mod("Env3", "OscBLevel", 0.1)
    lfo(p, 3, rate=4.2)
    p.mod("Lfo3", "OscAPan", 0.3, True)
    chorus(p, 0.5, 0.3, 0.35)
    reverb(p, 0.18, 0.55, mode=PLATE)
    poly(p, 12)
    p.set(vel_sens=0.7)
    macros(p, "OscAWarp", 0.15, "Lfo3Rate", 0.2)

    p = new(c, "FM Crystal Keys")
    fm_pair(p, ratio_oct=1, ratio_semi=10, ratio_fine=-31.0, index=0.04)
    amp(p, 0.001, 1.6, 0.1, 0.6)
    env2(p, 0.0, 0.4, 0.05, 0.3)
    p.mod("Env2", "OscAWarp", 0.22)
    p.mod("Velocity", "OscAWarp", 0.1)
    chorus(p, 0.4, 0.25, 0.3)
    delay(p, "1/8.", 0.12, 0.3, 6000)
    reverb(p, 0.22, 0.65)
    poly(p, 12)
    p.set(vel_sens=0.6)
    macros(p, "OscAWarp", 0.2)

    p = new(c, "Auto Wah Clav")
    osc(p, "A", "Pulse Width", 0.75, rand=0.0)
    osc(p, "B", "Basic Shapes", SAW, octave=1, level=0.25, rand=0.0)
    amp(p, 0.001, 0.6, 0.15, 0.08)
    flt(p, BP12, 700, 0.55, 0.2, 0.5)
    env2(p, 0.0, 0.18, 0.0, 0.08)
    p.mod("Env2", "FltCutoff", 0.35)
    p.mod("Velocity", "FltCutoff", 0.15)
    phaser(p, 0.6, 0.4, 0.3, 1200, 0.3)
    reverb(p, 0.08, 0.3, mode=ROOM)
    poly(p, 10)
    p.set(vel_sens=0.6)
    macros(p, "FltCutoff", 0.3, "FltRes", 0.3)

    # Vibraphone: sine with a faint 4th partial, motor tremolo.
    p = new(c, "Soft Vibes")
    osc(p, "A", "Sine", 0.0, rand=0.0)
    osc(p, "B", "Sine", 0.0, octave=2, level=0.2, rand=0.0)
    amp(p, 0.001, 3.0, 0.0, 0.8)
    p.set(env1_curve=0.3)
    env3(p, 0.0, 0.3, 0.0, 0.3)
    p.mod("Env3", "OscBLevel", 0.15)
    lfo(p, 3, rate=4.8)
    p.mod("Lfo3", "OscALevel", -0.2)
    reverb(p, 0.22, 0.6)
    poly(p, 12)
    p.set(vel_sens=0.7)
    macros(p, "OscBLevel", 0.25, "Lfo3Rate", 0.2)

    p = new(c, "Celeste Box")
    osc(p, "A", "Sine", 0.0, rand=0.0)
    osc(p, "B", "Glass Bell", 0.55, octave=1, level=0.35, rand=0.0)
    amp(p, 0.001, 1.4, 0.0, 0.6)
    env3(p, 0.0, 0.15, 0.0, 0.1)
    p.mod("Env3", "OscBLevel", 0.2)
    flt(p, LP12, 7000, 0.1, 0.0, 0.5)
    reverb(p, 0.28, 0.7)
    poly(p, 12)
    p.set(vel_sens=0.6)
    macros(p, "FltCutoff", 0.3)

    p = new(c, "Lofi Rhodes")
    osc(p, "A", "Sine", 0.0, warp=W_FM, warp_amt=0.12, rand=0.0)
    osc(p, "B", "Sine", 0.0, octave=1, level=0.0, rand=0.0)
    p.set(oscB_toFilter=False)
    amp(p, 0.002, 2.2, 0.15, 0.5)
    flt(p, LP12, 2600, 0.1, 0.0, 0.5)
    env2(p, 0.0, 0.3, 0.0, 0.2)
    p.mod("Env2", "OscAWarp", 0.08)
    lfo(p, 1, rate=0.6, shape=DRIFT_L)
    p.mod("Lfo1", "OscAFine", 0.05, True)
    noise(p, 3, 0.04, 0.5, False)
    dist(p, TAPE, 0.35, 0.45, 0.7)
    chorus(p, 0.35, 0.3, 0.3)
    reverb(p, 0.2, 0.5)
    poly(p, 10)
    p.set(vel_sens=0.6)
    macros(p, "FltCutoff", 0.3, "OscAFine", 0.0)

    p = new(c, "Plugg Bells")
    osc(p, "A", "Glass Bell", 0.35, rand=0.0)
    osc(p, "B", "Sine", 0.0, octave=1, level=0.35, rand=0.0)
    amp(p, 0.001, 1.2, 0.0, 0.5)
    flt(p, LP12, 6000, 0.1)
    chorus(p, 0.6, 0.4, 0.45)
    delay(p, "1/8", 0.2, 0.35, 5000)
    reverb(p, 0.3, 0.7)
    poly(p, 10)
    p.set(vel_sens=0.5)
    macros(p, "FltCutoff", 0.3, "OscAPos", 0.3)

    p = new(c, "Dark Trap Bell")
    fm_pair(p, ratio_oct=1, ratio_semi=5, index=0.06)
    p.set(oscA_octave=0)
    amp(p, 0.001, 1.8, 0.0, 0.6)
    env2(p, 0.0, 0.5, 0.05, 0.4)
    p.mod("Env2", "OscAWarp", 0.18)
    flt(p, LP12, 3500, 0.1, 0.0, 0.4)
    delay(p, "1/4", 0.15, 0.35, 3500)
    reverb(p, 0.3, 0.75)
    poly(p, 10)
    p.set(vel_sens=0.4)
    macros(p, "OscAWarp", 0.2, "FltCutoff", 0.25)

    # Drawbar organ through a spinning speaker (chorus and phaser together, the wheel speeds it up).
    p = new(c, "Gospel Drawbars")
    osc(p, "A", "Organ Bars", 0.85, rand=0.0)
    osc(p, "B", "Organ Bars", 0.4, octave=-1, level=0.4, rand=0.0)
    amp(p, 0.002, 0.2, 1.0, 0.06)
    env3(p, 0.0, 0.03, 0.0, 0.02)
    p.mod("Env3", "OscBLevel", 0.2)          # key click
    chorus(p, 0.8, 0.45, 0.45)
    phaser(p, 0.9, 0.35, 0.25, 900, 0.2)
    p.mod("ModWheel", "FxChoDepth", 0.3)
    dist(p, TUBE, 0.2, 0.7, 0.5)
    reverb(p, 0.15, 0.45, mode=ROOM)
    poly(p, 12)
    macros(p, "OscAPos", 0.25, "FxChoMix", 0.3)

    # Paired detuned reeds, the French accordion sound.
    p = new(c, "Musette Reeds")
    osc(p, "A", "Pulse Width", 0.6, rand=0.0)
    osc(p, "B", "Pulse Width", 0.6, fine=14.0, level=0.7, rand=0.0)
    amp(p, 0.03, 0.5, 0.95, 0.12)
    flt(p, LP12, 3500, 0.15, 0.0, 0.5)
    p.mod("Velocity", "FltCutoff", 0.15)
    reverb(p, 0.12, 0.4, mode=ROOM)
    poly(p, 10)
    macros(p, "FltCutoff", 0.3, "OscBFine", 0.15)

    p = new(c, "Cassette Keys")
    osc(p, "A", "Basic Shapes", TRI, uni=2, det=0.1, rand=0.0)
    osc(p, "B", "Glass Bell", 0.15, level=0.3, rand=0.0)
    amp(p, 0.003, 1.6, 0.25, 0.5)
    flt(p, LP12, 2200, 0.1, 0.0, 0.5)
    lfo(p, 1, rate=0.45, shape=DRIFT_L)
    p.mod("Lfo1", "OscAFine", 0.06, True)
    p.mod("Lfo1", "OscBFine", 0.06, True)
    noise(p, 3, 0.05, 0.5, False)
    dist(p, TAPE, 0.4, 0.4, 0.7)
    reverb(p, 0.2, 0.5)
    poly(p, 10)
    p.set(vel_sens=0.5)
    macros(p, "FltCutoff", 0.3, "FxDistDrive", 0.3)

    p = new(c, "Glass Marimba")
    osc(p, "A", "Sine", 0.0, rand=0.0)
    osc(p, "B", "Sine", 0.0, octave=2, semi=-2, level=0.25, rand=0.0)   # the tuned bar's high mode
    amp(p, 0.001, 0.5, 0.0, 0.25)
    env3(p, 0.0, 0.06, 0.0, 0.05)
    p.mod("Env3", "OscBLevel", 0.2)
    reverb(p, 0.2, 0.5)
    delay(p, "1/8.", 0.1, 0.25, 5000)
    poly(p, 12)
    p.set(vel_sens=0.6)
    macros(p, "OscBLevel", 0.25)


def build_pads_more():
    c = "Pads"
    # Eighties poly pad: one pulse with slowly moving width and a deep stereo chorus.
    p = pad("Eighties Poly Pad", "Pulse Width", 0.35, uni=1, det=0.0, atk=0.6, rel=2.0, cutoff=2400,
            b=dict(table="Basic Shapes", pos=SAW, level=0.45, rand=0.0), chorus_on=False)
    lfo(p, 3, rate=0.45, shape=TRI_L)
    p.mod("Lfo3", "OscAPos", 0.25)
    chorus(p, 0.5, 0.65, 0.55, delay=6.0, fb=0.1)
    # Fantasy pad: a bell attack fading into a soft, chorused pad.
    p = pad("Fantasy Bell Pad", "Basic Shapes", SAW, uni=5, det=0.3, atk=0.5, rel=2.5, cutoff=2600,
            b=dict(table="Glass Bell", pos=0.6, octave=1, level=0.0, rand=0.0))
    env3(p, 0.0, 1.2, 0.0, 1.0)
    p.mod("Env3", "OscBLevel", 0.5)
    p.set(env1_attack=0.004, oscB_toFilter=False, oscA_level=0.0)
    env2(p, 0.6, 1.0, 1.0, 2.0)
    p.mod("Env2", "OscALevel", 0.6)
    # Vector pad: two tables crossfade on a slow cycle while their positions drift.
    p = pad("Vector Pad", "Vowel Morph", 0.2, uni=3, det=0.25, cutoff=4000,
            b=dict(table="Glass Bell", pos=0.3, uni=3, det=0.25, level=0.6))
    lfo(p, 3, rate=0.08, shape=SINE_L)
    p.mod("Lfo3", "OscALevel", 0.3, True)
    p.mod("Lfo3", "OscBLevel", -0.3, True)
    p.mod("Lfo3", "OscBPos", 0.4)
    # The great brass-pad of seventies film synths: slow, vibrato on pressure, a huge room.
    p = pad("Neon Rain Pad", "Basic Shapes", SAW, uni=3, det=0.22, atk=1.2, rel=3.0, cutoff=900, res=0.2,
            b=dict(table="Basic Shapes", pos=SAW, fine=7.0, uni=2, det=0.2, level=0.6), rev=0.55, size=0.95,
            chorus_on=False)
    env2(p, 1.0, 2.0, 0.6, 2.5)
    p.mod("Env2", "FltCutoff", 0.15)
    lfo(p, 3, rate=5.5, rise=1.2)
    p.mod("Lfo3", "OscAFine", 0.025, True)
    p.mod("Aftertouch", "FltCutoff", 0.15)
    p.set(fx_rev_predelay=45.0)
    pad("Lofi Dust Pad", "Basic Shapes", TRI, uni=5, det=0.3, cutoff=1500, noise_lvl=0.05, lfo_cut=0.05).set(
        noise_type=3, fx_dist_on=True, fx_dist_mode=TAPE, fx_dist_drive=0.35, fx_dist_tone=0.4, fx_dist_mix=0.7)
    pad("Ambient Glass Swell", "Glass Bell", 0.45, uni=5, det=0.3, atk=2.0, rel=4.0, cutoff=7000, lfo_pos=0.4,
        lfo_rate=0.06, dly=0.25, rev=0.6, size=0.97)
    p = pad("Cinematic Drone", "Comb Saw", 0.2, uni=5, det=0.25, atk=2.5, rel=4.0, cutoff=900, res=0.25,
            b=dict(table="Basic Shapes", pos=SAW, octave=-1, uni=3, det=0.2, level=0.45), lfo_pos=0.5,
            lfo_rate=0.03, lfo_cut=0.1, chorus_on=False, rev=0.55, size=0.95)
    p.set(fx_rev_decay=7.0)
    p = pad("Granular Air", "Breath", 0.6, uni=7, det=0.4, atk=1.5, rel=3.5, cutoff=6000, filt=BP12, res=0.3,
            noise_lvl=0.15, lfo_pos=0.6, lfo_rate=0.2, lfo_cut=0.15, rev=0.6, size=0.95, dly=0.2)
    pad("Future Swell Pad", "Basic Shapes", SAW, uni=9, det=0.45, atk=0.3, rel=1.5, cutoff=5000,
        b=dict(table="Basic Shapes", pos=SAW, octave=1, uni=5, det=0.4, level=0.3), pumped=True)
    p = pad("Warm Tape Pad", "Basic Shapes", SAW, uni=5, det=0.28, cutoff=1300,
            b=dict(table="Basic Shapes", pos=TRI, octave=-1, level=0.4), lfo_cut=0.05)
    lfo(p, 3, rate=0.5, shape=DRIFT_L)
    p.mod("Lfo3", "OscAFine", 0.05, True)
    p.set(fx_dist_on=True, fx_dist_mode=TAPE, fx_dist_drive=0.3, fx_dist_tone=0.45, fx_dist_mix=0.6)
    pad("Shimmer Choir", "Vowel Morph", 0.45, uni=7, det=0.3, atk=1.0, cutoff=8000, lfo_pos=0.4,
        b=dict(table="Vowel Morph", pos=0.3, octave=1, uni=3, det=0.3, level=0.3), rev=0.65, size=0.98, dly=0.2)
    p = pad("Space Organ Pad", "Organ Bars", 0.45, uni=3, det=0.15, atk=1.8, rel=4.0, cutoff=2800,
            b=dict(table="Organ Bars", pos=0.9, octave=1, level=0.3), chorus_on=False, rev=0.6, size=0.98)
    p.set(fx_rev_decay=8.0, fx_rev_predelay=30.0)
    p = pad("Dark Matter", "Sine", 0.0, uni=1, det=0.0, atk=2.0, rel=4.0, cutoff=1800, chorus_on=False,
            b=dict(table="Sine", pos=0.0, semi=7, octave=-1, level=0.0), rev=0.5, size=0.9, phaser_on=True)
    p.set(oscA_warp=W_FM, oscA_warpAmt=0.15, oscB_toFilter=False)
    lfo(p, 3, rate=0.05, shape=SINE_L)
    p.mod("Lfo3", "OscAWarp", 0.15)
    pad("Phase Drift Pad", "Basic Shapes", SAW, uni=7, det=0.35, cutoff=2600, phaser_on=True, lfo_cut=0.06)
    pad("Sunrise Pad", "Harmonic Sweep", 0.3, uni=7, det=0.35, atk=2.0, rel=3.0, cutoff=5000, lfo_pos=0.5,
        lfo_rate=0.04, b=dict(table="Basic Shapes", pos=SAW, octave=1, uni=3, det=0.3, level=0.25))
    p = pad("Frozen Lake", "Glass Bell", 0.8, uni=5, det=0.25, atk=1.5, rel=4.0, cutoff=9000, filt=HP12,
            res=0.1, lfo_pos=0.3, rev=0.65, size=0.98, dly=0.2)
    p.set(flt_cutoff=500.0)
    p = pad("Modular Drone", "Fold Sweep", 0.3, uni=3, det=0.2, atk=2.0, rel=3.5, cutoff=1200, res=0.4,
            chorus_on=False, phaser_on=True, rev=0.5, size=0.9)
    lfo(p, 3, rate=0.25, shape=SH_L, smooth=0.8)
    p.mod("Lfo3", "FltCutoff", 0.15, True)
    p.mod("Lfo3", "OscAPos", 0.3)
    pad("Analog Brass Pad", "Basic Shapes", SAW, uni=5, det=0.25, atk=0.4, rel=1.5, cutoff=1400, res=0.15,
        b=dict(table="Basic Shapes", pos=SAW, fine=-8.0, uni=3, det=0.2, level=0.6), lfo_cut=0.04)


def build_strings_more():
    c = "Strings"
    strings("Vintage Ensemble", "Basic Shapes", SAW, uni=3, det=0.25, cutoff=3800, phaser_on=True,
            b=dict(table="Pulse Width", pos=0.25, octave=1, uni=3, det=0.25, level=0.45), rev=0.35).set(
        fx_cho_rate=0.9, fx_cho_depth=0.6, fx_cho_mix=0.55)
    # Pressure-sensitive film brass: slow swell, vibrato under aftertouch.
    p = strings("Blade Brass", "Basic Shapes", SAW, uni=3, det=0.2, atk=0.35, rel=1.8, cutoff=700, res=0.2,
                swell=True, vib=0.0, chorus_on=False, rev=0.45,
                b=dict(table="Basic Shapes", pos=SAW, fine=6.0, uni=2, det=0.2, level=0.6))
    lfo(p, 3, rate=5.6, rise=0.8)
    p.mod("Lfo3", "OscAFine", 0.02, True)
    p.mod("Aftertouch", "FltCutoff", 0.15)
    p.set(fx_rev_predelay=40.0, fx_rev_size=0.9)
    # Arena synth brass: punchy stacked saws for big rock-style chord stabs.
    p = strings("Arena Synth Brass", "Basic Shapes", SAW, uni=5, det=0.3, atk=0.02, rel=0.4, cutoff=1200,
                res=0.15, vib=0.0, rev=0.25, decay=0.8, sus=0.75,
                b=dict(table="Basic Shapes", pos=SAW, fine=-7.0, uni=3, det=0.25, level=0.6))
    env2(p, 0.0, 0.35, 0.3, 0.3)
    p.mod("Env2", "FltCutoff", 0.25)
    p = strings("Velvet Poly Brass", "Basic Shapes", SAW, uni=3, det=0.2, atk=0.08, rel=0.6, cutoff=900,
                res=0.2, swell=True, vib=0.0, rev=0.3,
                b=dict(table="Pulse Width", pos=0.35, uni=2, det=0.15, level=0.5))
    p = strings("Minneapolis Horns", "Basic Shapes", SAW, uni=3, det=0.2, atk=0.01, rel=0.2, cutoff=1500,
                res=0.2, vib=0.0, rev=0.2, short=True, decay=0.35,
                b=dict(table="Basic Shapes", pos=SQUARE, octave=1, level=0.35))
    p.set(env1_sustain=0.35)
    strings("Chamber Strings", "Basic Shapes", SAW, uni=7, det=0.3, atk=0.25, rel=1.0, cutoff=2600, vib=0.03,
            rev=0.4)
    strings("Warm Cello Section", "Basic Shapes", SAW, uni=5, det=0.25, atk=0.3, cutoff=1100, res=0.12,
            b=dict(table="Hollow Lead", pos=0.3, octave=-1, uni=3, det=0.2, level=0.5), vib=0.025, rev=0.4)
    strings("High Octave Violins", "Harmonic Sweep", 0.85, uni=7, det=0.3, atk=0.3, cutoff=8000, vib=0.03,
            b=dict(table="Basic Shapes", pos=SAW, octave=1, uni=5, det=0.3, level=0.4), rev=0.45)
    strings("Orchestral Swell", "Basic Shapes", SAW, uni=11, det=0.4, atk=2.2, rel=2.5, cutoff=900, swell=True,
            b=dict(table="Basic Shapes", pos=SAW, octave=-1, uni=5, det=0.35, level=0.45), rev=0.55)
    strings("Sharp Pizzicato", "Basic Shapes", SAW, uni=3, det=0.15, short=True, decay=0.12, rel=0.12,
            cutoff=3500, chorus_on=False, rev=0.3)


def build_synths_more():
    c = "Synths"
    # Berlin school sequence voice: a resonant saw blip that loves long delays.
    p = pluck("Berlin Sequence", uni=1, det=0.0, decay=0.22, cutoff=500, env_amt=0.45, res=0.45, dly="1/8.",
              dly_mix=0.3, rev=0.3, filt=LADDER)
    phaser(p, 0.1, 0.5, 0.3)
    lfo(p, 2, div="8bar", shape=TRI_L)
    p.mod("Lfo2", "FltCutoff", 0.15, True)
    # Low pass gate pluck: the level and the brightness fall together, the classic bongo of west coast synths.
    p = new(c, "Low Pass Gate Pluck")
    fm_pair(p, ratio_oct=1, ratio_semi=7, index=0.05)
    amp(p, 0.001, 0.45, 0.0, 0.2)
    p.set(env1_curve=0.25)
    flt(p, LP12, 400, 0.1, 0.0, 0.5)
    env2(p, 0.0, 0.3, 0.0, 0.2)
    p.set(env2_curve=0.25)
    p.mod("Env2", "FltCutoff", 0.5)
    p.mod("Env2", "OscAWarp", 0.25)
    p.mod("Velocity", "OscAWarp", 0.1)
    delay(p, "1/8.", 0.2, 0.4, 4000)
    reverb(p, 0.2, 0.6)
    poly(p, 10)
    p.set(vel_sens=0.6)
    macros(p, "OscAWarp", 0.25, "FltCutoff", 0.25)
    # Generative modular patch: random voltages move pitch (in fifths and octaves) and timbre.
    p = new(c, "Random Voltage")
    osc(p, "A", "Basic Shapes", TRI, warp=W_ASYM, warp_amt=0.2)
    amp(p, 0.001, 0.3, 0.0, 0.25)
    flt(p, LP24, 1200, 0.45, 0.1, 0.4)
    env2(p, 0.0, 0.15, 0.0, 0.1)
    p.mod("Env2", "FltCutoff", 0.3)
    lfo(p, 1, div="1/8", shape=SH_L, mode=FREE)
    p.mod("Lfo1", "OscAWarp", 0.4)
    p.mod("Lfo1", "FltCutoff", 0.15, True)
    lfo(p, 3, div="1/4", shape=SH_L, mode=FREE)
    p.mod("Lfo3", "OscACoarse", 12 * ST, True)
    delay(p, "1/8.", 0.3, 0.5, 3500)
    reverb(p, 0.3, 0.8)
    poly(p, 6)
    macros(p, "FltCutoff", 0.3, "Lfo1Rate", 0.2)
    # Complex oscillator pluck: FM and a wavefolder in one stroke.
    p = new(c, "Complex Osc Pluck")
    fm_pair(p, ratio_oct=0, ratio_semi=7, index=0.1)
    amp(p, 0.001, 0.6, 0.0, 0.3)
    env2(p, 0.0, 0.2, 0.0, 0.15)
    p.mod("Env2", "OscAWarp", 0.3)
    dist(p, FOLD, 0.25, 0.7, 0.6)
    flt(p, LP12, 5000, 0.1, 0.0, 0.4)
    delay(p, "1/16.", 0.15, 0.35, 4500)
    reverb(p, 0.2, 0.55)
    poly(p, 8)
    p.set(vel_sens=0.5)
    macros(p, "OscAWarp", 0.3, "FxDistDrive", 0.3)
    # Poly-mod bell: the second oscillator frequency-modulates the first as the filter falls.
    p = new(c, "Poly Mod Bell")
    osc(p, "A", "Basic Shapes", TRI, warp=W_FM, warp_amt=0.15, rand=0.0)
    osc(p, "B", "Basic Shapes", SAW, octave=1, semi=7, level=0.2, rand=0.0)
    amp(p, 0.001, 1.0, 0.0, 0.5)
    flt(p, LP24, 900, 0.3, 0.0, 0.5)
    env2(p, 0.0, 0.5, 0.0, 0.4)
    p.mod("Env2", "FltCutoff", 0.35)
    p.mod("Env2", "OscAWarp", 0.25)
    chorus(p, 0.4, 0.3, 0.3)
    delay(p, "1/4", 0.15, 0.35, 4000)
    reverb(p, 0.22, 0.6)
    poly(p, 10)
    macros(p, "FltCutoff", 0.3, "OscAWarp", 0.3)
    # Future bass chords: supersaws that scoop up into the note, ducked on the beat.
    p = new(c, "Future Bass Chords")
    osc(p, "A", "Basic Shapes", SAW, uni=9, det=0.45)
    osc(p, "B", "Basic Shapes", SQUARE, uni=5, det=0.35, octave=1, level=0.3)
    amp(p, 0.003, 1.0, 1.0, 0.3)
    flt(p, LP24, 4500, 0.15, 0.0, 0.3)
    env3(p, 0.0, 0.08, 0.0, 0.05)
    p.mod("Env3", "OscACoarse", -2 * ST)
    p.mod("Env3", "OscBCoarse", -2 * ST)
    lfo(p, 2, rate=5.0, rise=0.3)
    p.mod("Lfo2", "OscAFine", 0.02, True)
    multiband(p, 0.45, 0.4, 0.4, 1.0)
    reverb(p, 0.25, 0.7)
    pump(p, 4, "1/4", 0.5)
    poly(p, 10)
    macros(p, "FltCutoff", 0.3, "OscADetune", 0.3)
    p = new(c, "Kawaii Chords")
    osc(p, "A", "Basic Shapes", SQUARE, uni=3, det=0.2)
    osc(p, "B", "Glass Bell", 0.3, octave=1, level=0.35)
    amp(p, 0.002, 0.8, 0.6, 0.25)
    flt(p, LP12, 5000, 0.15)
    lfo(p, 2, rate=6.0, rise=0.25)
    p.mod("Lfo2", "OscAFine", 0.04, True)
    chorus(p, 0.6, 0.4, 0.4)
    delay(p, "1/8", 0.15, 0.3, 6000)
    reverb(p, 0.2, 0.6)
    pump(p, 4, "1/4", 0.4)
    poly(p, 10)
    macros(p, "FltCutoff", 0.3, "Lfo2Rate", 0.2)
    p = pluck("Afterhours Arp", "Basic Shapes", SAW, uni=3, det=0.2, decay=0.25, cutoff=500, env_amt=0.45,
              res=0.35, dly="1/4.", dly_mix=0.3, rev=0.35,
              b=dict(table="Basic Shapes", pos=SQUARE, octave=-1, level=0.35))
    lfo(p, 2, div="4bar", shape=TRI_L)
    p.mod("Lfo2", "FltCutoff", 0.15, True)
    p.set(fx_dly_feedback=0.5, fx_rev_size=0.85)
    # Pitched cowbell of drift phonk: two squares a little over a fifth apart, band-passed and driven.
    p = new(c, "Phonk Cowbell")
    osc(p, "A", "Basic Shapes", SQUARE, rand=0.0)
    osc(p, "B", "Basic Shapes", SQUARE, semi=7, fine=-20.0, level=0.75, rand=0.0)
    amp(p, 0.0, 0.35, 0.0, 0.2)
    p.set(env1_curve=0.25)
    flt(p, BP12, 1400, 0.35, 0.1, 0.7)
    dist(p, HARD, 0.3, 0.6, 0.6)
    reverb(p, 0.15, 0.4, mode=ROOM)
    poly(p, 8)
    macros(p, "FltCutoff", 0.3, "FxDistDrive", 0.3)
    p = pluck("Hyper Pluck", uni=9, det=0.45, decay=0.22, cutoff=1500, env_amt=0.45, rev=0.3,
              b=dict(table="Basic Shapes", pos=SAW, octave=1, uni=5, det=0.4, level=0.3))
    multiband(p, 0.55, 0.4, 0.5, 1.0)
    pluck("Cassette Pluck", "Basic Shapes", TRI, uni=2, det=0.12, decay=0.3, cutoff=900, env_amt=0.4,
          rev=0.25).set(fx_dist_on=True, fx_dist_mode=TAPE, fx_dist_drive=0.35, fx_dist_tone=0.4, fx_dist_mix=0.7)
    p = pluck("Melodic Dub Pluck", uni=9, det=0.45, decay=0.4, cutoff=900, env_amt=0.45, rev=0.4,
              b=dict(table="Hollow Lead", pos=0.5, octave=1, uni=3, det=0.3, level=0.3), dly="1/4", dly_mix=0.2)
    p.set(fx_rev_size=0.9)
    p = pluck("Koto Pluck", "Harmonic Sweep", 0.7, uni=1, det=0.0, decay=0.25, cutoff=2500, env_amt=0.35,
              rev=0.25, dly_mix=0.12)
    env3(p, 0.0, 0.03, 0.0, 0.02)
    p.mod("Env3", "OscACoarse", 0.6 * ST)
    p = new(c, "Electro Zap")
    osc(p, "A", "Basic Shapes", SQUARE, rand=0.0)
    amp(p, 0.001, 0.25, 0.0, 0.15)
    env3(p, 0.0, 0.18, 0.0, 0.1)
    p.mod("Env3", "OscACoarse", 0.35)
    flt(p, LP24, 3000, 0.5, 0.2)
    env2(p, 0.0, 0.15)
    p.mod("Env2", "FltCutoff", 0.3)
    delay(p, "1/16", 0.2, 0.35, 5000)
    poly(p, 6)
    macros(p, "FltCutoff", 0.3)
    p = new(c, "White Noise Riser")
    osc(p, "A", "Basic Shapes", SAW, uni=7, det=0.5, level=0.35)
    noise(p, 0, 0.7, 0.6)
    amp(p, 0.05, 1.0, 1.0, 1.0)
    flt(p, BP12, 250, 0.35)
    lfo(p, 1, div="8bar", shape=SAWUP_L, mode=ENVMODE)
    p.mod("Lfo1", "FltCutoff", 0.7)
    p.mod("Lfo1", "OscACoarse", 0.2)
    phaser(p, 0.3, 0.5, 0.3)
    reverb(p, 0.45, 0.85)
    poly(p, 4)
    macros(p, "FltCutoff", 0.3)
    p = new(c, "Downlifter")
    osc(p, "A", "Basic Shapes", SAW, uni=5, det=0.4, level=0.4)
    noise(p, 0, 0.6, 0.6)
    amp(p, 0.001, 2.0, 0.0, 1.0)
    flt(p, BP12, 6000, 0.3)
    env2(p, 0.0, 1.8, 0.0, 1.0)
    p.mod("Env2", "FltCutoff", 0.25)
    env3(p, 0.0, 2.0, 0.0, 1.0)
    p.mod("Env3", "OscACoarse", 0.25)
    reverb(p, 0.5, 0.9)
    poly(p, 4)
    macros(p, "FltCutoff", 0.3)
    p = new(c, "Impact Boom")
    osc(p, "A", "Sine", 0.0, octave=-2, rand=0.0)
    noise(p, 2, 0.4, 0.4, False)
    amp(p, 0.0, 2.5, 0.0, 2.0)
    p.set(env1_curve=0.3)
    env3(p, 0.0, 0.5, 0.0, 0.3)
    p.mod("Env3", "OscACoarse", 0.15)
    env2(p, 0.0, 0.25, 0.0, 0.2)
    p.mod("Env2", "NoiseLevel", 0.4)
    p.set(noise_level=0.0)
    dist(p, SOFT, 0.35, 0.5)
    reverb(p, 0.4, 0.95, lowcut=40, hicut=4000)
    poly(p, 4)
    macros(p, "FxRevMix", 0.3)
    arp_synth("Glass Sequence", "Glass Bell", 0.3, div="1/16", target="OscAPos", amount=0.5, shape=SAWUP_L,
              decay=0.35, cutoff=7000)
    p = new(c, "Wonk Chords")
    osc(p, "A", "Basic Shapes", SAW, uni=5, det=0.35)
    osc(p, "B", "Basic Shapes", SQUARE, uni=3, det=0.25, octave=-1, level=0.4)
    amp(p, 0.003, 0.9, 0.7, 0.3)
    flt(p, LP24, 2200, 0.3, 0.2)
    lfo(p, 1, div="1/2T", shape=DRIFT_L)
    p.mod("Lfo1", "OscAFine", 0.12, True)
    p.mod("Lfo1", "OscBFine", 0.1, True)
    p.mod("Lfo1", "FltCutoff", 0.08, True)
    dist(p, TUBE, 0.25, 0.6, 0.5)
    reverb(p, 0.2, 0.6)
    poly(p, 10)
    macros(p, "FltCutoff", 0.3, "OscAFine", 0.0)
    p = pluck("Afro House Marimba", "Sine", 0.0, uni=1, det=0.0, decay=0.22, cutoff=6000, env_amt=0.0,
              b=dict(table="Sine", pos=0.0, octave=2, level=0.2), dly="1/8.", dly_mix=0.18, rev=0.25)
    p.set(oscA_warp=W_FM, oscA_warpAmt=0.08)
    p.mod("Velocity", "OscAWarp", 0.08)


def build_deep_house_more():
    c = "Deep House"
    # The nineties house organ: bright drawbars with a short, punchy decay.
    p = part(c, "Classic House Organ", "Organ Bars", 0.95, dec=0.35, sus=0.35, rel=0.12, cutoff=7000,
             env_amt=0.1, b=dict(table="Organ Bars", pos=0.55, octave=1, level=0.3), chorus_on=True, rev=0.15,
             size=0.4, rev_mode=ROOM)
    part(c, "Memory Chord", "Chord Stack", 0.3, uni=2, det=0.12, dec=0.28, rel=0.2, cutoff=1100, res=0.25,
         env_amt=0.4, fdec=0.15, filt=LADDER, dly="1/8.", dly_mix=0.22, rev=0.2, tape=0.2)
    part(c, "Soulful Rhodes", "Sine", 0.0, warp=W_FM, warp_amt=0.14, dec=2.4, sus=0.2, rel=0.6,
         b=dict(table="Sine", pos=0.0, octave=1, level=0.0), cutoff=3800, env_amt=0.25, tremolo=0.25,
         phaser_on=True, rev=0.2, tape=0.2)
    p = part(c, "French Filter Chords", "Basic Shapes", SAW, uni=5, det=0.3, atk=0.005, dec=1.0, sus=0.9,
             rel=0.25, cutoff=5000, env_amt=0.0, phaser_on=True, rev=0.15,
             fxf=dict(kind=F_LP24, cutoff=900, res=0.35, drive=0.2))
    lfo(p, 2, div="4bar", shape=SINE_L)
    p.mod("Lfo2", "FxFilCutoff", 0.2, True)
    pump(p, 4, "1/4", 0.45)
    bassline(c, "Disco Octave Bass", "Basic Shapes", SAW, cutoff=700, res=0.25, env_amt=0.35, decay=0.14,
             sus=0.3, sub_lvl=0.4, glide=0.0, b=dict(table="Basic Shapes", pos=SAW, octave=0, level=0.3,
                                                     rand=0.0))
    part(c, "Lofi House Pad", "Basic Shapes", SAW, uni=5, det=0.3, atk=0.6, dec=2.0, sus=0.85, rel=1.5,
         cutoff=1200, env_amt=0.0, chorus_on=True, tape=0.35, rev=0.35, size=0.8, lfo_cut=0.06, lfo_div="2bar",
         noise_lvl=0.03, noise_kind=3)
    bassline(c, "Afro Log Bass", "Sine", 0.0, cutoff=4000, env_amt=0.0, decay=0.3, sus=0.0, pitch=0.08,
             sub_lvl=0.0, drive=0.3, glide=0.05).set(oscA_warp=W_FM, oscA_warpAmt=0.08)
    part(c, "Garage Organ Stab", "Organ Bars", 0.6, dec=0.18, rel=0.1, cutoff=2500, env_amt=0.3, fdec=0.1,
         dly="1/8T", dly_mix=0.18, rev=0.15, mb=0.3)


def build_minimal_more():
    c = "Minimal"
    p = part(c, "Dub Techno Chord", "Chord Stack", 0.3, uni=2, det=0.15, dec=0.3, rel=0.25, cutoff=700, res=0.3,
             env_amt=0.3, dly="1/4.", dly_mix=0.4, dly_fb=0.7, rev=0.45, size=0.9, lfo_cut=0.12, lfo_div="8bar")
    p.set(fx_dly_hicut=2200.0, fx_dly_lowcut=400.0)
    bassline(c, "Micro Click Bass", "Basic Shapes", SAW, cutoff=220, res=0.3, env_amt=0.5, decay=0.04, sus=0.0,
             sub_lvl=0.6, drive=0.2)
    p = part(c, "Glitch Blip", "Digital Steps", 0.3, dec=0.05, rel=0.04, cutoff=9000, env_amt=0.0, dly="1/32",
             dly_mix=0.25, dly_fb=0.3, rev=0.0)
    lfo(p, 1, div="1/16", shape=SH_L, mode=FREE)
    p.mod("Lfo1", "OscAPos", 0.7)
    part(c, "Rubber Perc", "Sine", 0.0, dec=0.12, rel=0.08, cutoff=3000, env_amt=0.0, pitch=0.12, pdec=0.04,
         rev=0.12, size=0.3, rev_mode=ROOM)
    part(c, "Shuffle Blip", "Basic Shapes", TRI, dec=0.08, rel=0.06, cutoff=2500, env_amt=0.2,
         dly="1/8T", dly_mix=0.3, dly_fb=0.45, rev=0.12)
    part(c, "Tape Rim", "Sine", 0.0, b=dict(table="Sine", pos=0.0, octave=2, semi=5, level=0.4), dec=0.05,
         rel=0.04, cutoff=5000, env_amt=0.0, noise_lvl=0.2, tape=0.35, dly="1/8.", dly_mix=0.25, rev=0.1)
    part(c, "Hypnotic Sine", "Sine", 0.0, dec=0.35, rel=0.2, cutoff=8000, env_amt=0.0, lfo_cut=0.0,
         dly="1/16.", dly_mix=0.3, dly_fb=0.55, rev=0.2, warp=W_FM, warp_amt=0.05,
         b=dict(table="Sine", pos=0.0, octave=1, semi=7, level=0.0))
    part(c, "Wooden Pluck", "Harmonic Sweep", 0.4, dec=0.1, rel=0.08, cutoff=1400, res=0.4, env_amt=0.3,
         filt=COMBP, rev=0.12, size=0.35, rev_mode=ROOM)


def build_pianos_more():
    c = "Pianos"
    p = piano(c, "Rave House Piano", BRIGHT, tone=0.6, dyn=0.2, sus=0.35, damper=0.3, strings=0.45, body=0.3,
              rev=0.15, size=0.5, mode=PLATE)
    eq(comp(p, thresh=-22.0, ratio=4.0, attack=2.0, release=90.0), low_freq=250.0, low=-5.0, high_freq=4500.0,
       high=3.5)
    space_macros(p)
    space_macros(piano(c, "Late Night Grand", GRAND, tone=-0.3, sus=0.6, damper=0.6, width=0.6, hammer=0.4,
                       rev=0.2, size=0.65, mode=PLATE))
    space_macros(piano(c, "Soft Practice Upright", UPRIGHT, tone=-0.45, dyn=0.55, hammer=0.55, body=0.6,
                       rev=0.08, size=0.3, mode=ROOM))
    p = piano(c, "Cassette Piano", UPRIGHT, tone=-0.2, rev=0.12, size=0.4, mode=ROOM)
    dist(p, mode=TAPE, drive=0.35, tone=0.4, mix=0.7)
    lfo(p, 3, rate=0.5, shape=DRIFT_L, mode=FREE)
    p.mod("Lfo3", "PnoTone", 0.04, True)
    space_macros(fxfilter(p, kind=F_LP12, cutoff=3200.0, res=0.05))
    p = piano(c, "Stage Tine Bark", TINE, tone=0.5, dyn=0.45, rev=0.1, size=0.4, mode=ROOM)
    space_macros(dist(p, mode=TUBE, drive=0.3, tone=0.55, mix=0.5))
    p = piano(c, "Soul Reed Keys", REED, tone=0.1, dyn=0.45, rev=0.12, size=0.45, mode=ROOM)
    space_macros(chorus(tremolo(p, div="1/8", depth=0.2), rate=0.4, depth=0.3, mix=0.3))


def main():
    # Each category's newer presets follow its first ones, so a category stays together in the bank.
    for first, more in ((build_bass, build_bass_more), (build_leads, build_leads_more),
                        (build_keys, build_keys_more), (build_pads, build_pads_more),
                        (build_strings, build_strings_more), (build_synths, build_synths_more),
                        (build_deep_house, build_deep_house_more), (build_minimal, build_minimal_more),
                        (build_pianos, build_pianos_more)):
        first()
        start = len(BANK)
        more()
        for p in BANK[start:]:
            p.post_gain = True

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
        if getattr(p, "post_gain", False) and p.v.get("fx_cmp_mix", 1.0) >= 1.0:
            # The newer presets take their level match after the effects, at the
            # compressor's output gain (-12 to +24 dB), so drive and compression
            # sound the same at any level. Only a cut past -12 dB moves the volume.
            db = 20.0 * math.log10(k)
            if abs(db) > 0.05:
                if not p.v.get("fx_cmp_on"):
                    comp(p, thresh=0.0, ratio=1.0)
                want = p.v.get("fx_cmp_gain", 0.0) + db
                got = max(-12.0, min(24.0, want))
                p.v["fx_cmp_gain"] = round(got, 2)
                kv = 10.0 ** ((want - got) / 20.0)
                p.v["master_vol"] = round(p.v["master_vol"] * kv, 4)
                p.mods = [(s_, d, round(a * kv, 4) if d == "MasterVol" else a, b) for s_, d, a, b in p.mods]
            continue
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
