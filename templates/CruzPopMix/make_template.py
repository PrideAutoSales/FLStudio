#!/usr/bin/env python3
"""Build the Cruz Pop Mix templates (.flp) for FL Studio 2024 and FL Studio 20.

The project is written straight in FL's event format, the way FL 20.8 saves
it, so FL 20, FL 2024 and later all open it. Nothing here needs FL Studio.

    python3 make_template.py OUT_DIR

OUT_DIR gets "Cruz Pop Mix/Cruz Pop Mix.flp" (FL 2024 and newer) and
"Cruz Pop Mix (FL 20)/Cruz Pop Mix (FL 20).flp". The two differ only in the
chorus and phaser sends: FL 20 has no Hyper Chorus or Vintage Phaser, so its
copy uses Vintage Chorus and Fruity Phaser. The Cruz Synth sounds come from
synth_states/keys.xml, bass.xml and lead.xml: factory presets saved by the
tools/synth_state harness (Soft Rhodes, Round Bass, Smooth Lead).

The template has no MIDI: every pattern is empty. Drums are FL sample
channels holding Cruz Drums one-shots. Every effect is a Cruz plugin except
the chorus and phaser on their sends, which are FL's own.
"""

import argparse
import datetime
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fl_defaults as D  # noqa: E402

FL_VERSION = "20.8.4.2576"
FL_BUILD = 2576
PPQ = 96
BAR = PPQ * 4
TEMPO = 120
NUM_INSERTS = 127          # 0 = master ... 126 = "current"
NUM_TRACKS = 500

# Template name -> the FL effects on the chorus and phaser sends.
VARIANTS = {
    "Cruz Pop Mix": {"chorus": "Hyper Chorus", "phaser": "Vintage Phaser"},
    "Cruz Pop Mix (FL 20)": {"chorus": "Vintage Chorus", "phaser": "Fruity Phaser"},
}


def sample_dir(template):
    return "%FLStudioUserData%/Projects/Templates/Cruz/" + template + "/Samples/"

# ---------------------------------------------------------------- colours --


def rgb(hex_colour):
    """'#RRGGBB' -> FL's colour word (R in the low byte)."""
    v = int(hex_colour.lstrip("#"), 16)
    r, g, b = (v >> 16) & 255, (v >> 8) & 255, v & 255
    return r | (g << 8) | (b << 16)


C = {
    "master":   "#3C4048",
    "drums":    "#E8553E", "drums_bus": "#B23A28",
    "sc":       "#F4C430", "sc_bus":    "#C99A12",
    "bass":     "#8E5BE8",
    "keys":     "#3D8BF2",
    "lead":     "#21C1D6", "inst_bus":  "#2D5FB8",
    "vox":      "#F25C9A", "vox_bus":   "#B83A70", "rec": "#FF3B3B",
    "mix_bus":  "#E9E9EE",
    "verb":     "#4FD1A5",
    "delay":    "#38B48B",
    "slap":     "#2E8F70",
    "chorus":   "#5BA8E0",
    "phaser":   "#7C6CE0",
    "fx":       "#A0A6B3", "auto": "#6B7280", "sections": "#2A2D34",
}

# ------------------------------------------------------- Cruz plugins (VST3) --

PLUGINS = {
    # name: (plugin code, is instrument)
    "Cruz EQ": ("Creq", False),
    "Cruz Limiter": ("Crlm", False),
    "Cruz Opto": ("Crla", False),
    "Cruz FET": ("Crfe", False),
    "Cruz Tape": ("Crtp", False),
    "Cruz Drive": ("Crds", False),
    "Cruz Verb": ("Crrv", False),
    "Cruz Echo": ("Crdy", False),
    "Cruz Synth": ("Crsy", True),
    "Cruz Sidechain": ("Crsc", False),
}

# Effects with a sidechain input bus. FL lists both input buses in record 31;
# the values are what FL 20 writes for a JUCE plugin with a sidechain bus.
SIDECHAIN_PLUGINS = {"Cruz Sidechain"}


# FL's own effects: the 212 wrapper and 213 state from each plugin's bundled
# Default.fst (FL fills in the insert and slot). Hyper Chorus and Vintage
# Phaser ship with FL 21 and newer only.
NATIVE = {
    "Hyper Chorus": (
        "050000000000000002000000000000004101000000000000000000000000000000000000cc030000db010000eb020000d9000000",
        "02000000f4010000f4010000f4010000f4010000f4010000f401000000000000f055000078000000f4010000f401000000000000"),
    "Vintage Phaser": (
        "020000000000000002000000000000004101000000000000000000000000000000000000b40500009b030000eb020000d9000000",
        "0300000046ac0d00043f1800b80b0000000000007d000000000000000100000000000000000000000000000000000000000000"
        "00000000000000000000000000b80b0000010000000000000000000000800c0000d007000032000000000000000000000000"
        "0000000000000088130000d0070000d007000019000000f0490200701700000000000001000000881300000000000000"),
    "Vintage Chorus": (
        "020000000000000002000000000000004101000000000000000000000000000000000000a8000000fc000000eb020000d9000000",
        "0100000000000000000000005000000000000000b80b00003c000000b80b0000b80b000000000000000000000000000"
        "0b80b000000000000b80b0000000000000000000000000000"),
    "Fruity Phaser": (
        "0100000004000000020000000000000040010000000000000000000000000000000000000c000000600000000000000000000000",
        "01000000dc050000000000008403000000000000000200000b000000b502000000000000c4090000"),
}
WRAPPER_FLAGS = 0x140   # threaded processing + hide settings; editor closed


def vst3_guid(plugin_code):
    """JUCE's VST3 component class id {ABCDEF01-9182-FAEB-'Cruz'-code}, in the
    Windows GUID byte layout FL stores (also on macOS)."""
    return (struct.pack("<IHH", 0xABCDEF01, 0x9182, 0xFAEB)
            + b"Cruz" + plugin_code.encode("ascii"))


def juce_state(xml):
    """AudioProcessor::copyXmlToBinary: 'VC2!', length, UTF-8 XML, NUL."""
    text = xml.encode("utf-8")
    return struct.pack("<II", 0x21324356, len(text)) + text + b"\0"


def fl_vst3_state(component_state):
    """FL's VST3 state record: an 80-byte header, then chunk 3 (the
    component state). JUCE writes no controller state, so FL adds nothing else."""
    header = struct.pack("<IIIII", 1, 1, 64, 0, 1) + bytes(60)
    return header + struct.pack("<IQ", 3, len(component_state)) + component_state


def wrapper_records(records):
    return b"".join(struct.pack("<IQ", rid, len(data)) + data for rid, data in records)


def vst3_wrapper(name, state=None):
    code, instrument = PLUGINS[name]
    path = "/Library/Audio/Plug-Ins/VST3/" + name + ".vst3"
    # Records as FL 20.9 writes them for the Cruz plugins on Pedro's Mac.
    records = [
        (1, bytes.fromhex("ffffffffffffffff0c0000000000000000000000")),
        (2, bytes.fromhex("00a0000000190000008c7d20a4000000000100000000000000")),
    ]
    if instrument:
        records += [
            (30, bytes.fromhex("00000000010000000000000000000000")),
            (32, bytes.fromhex("000000000100000000000000")),
        ]
    elif name in SIDECHAIN_PLUGINS:
        records += [
            (30, bytes.fromhex("02000000010000000000000000000000")),
            (31, bytes.fromhex("000000000100000000000000000000000000000000000000")),
            (32, bytes.fromhex("000000000100000000000000")),
        ]
    else:
        records += [
            (30, bytes.fromhex("01000000010000000000000000000000")),
            (31, bytes.fromhex("000000000100000000000000")),
            (32, bytes.fromhex("000000000100000000000000")),
        ]
    records += [
        (50, bytes.fromhex("08000000000000000000000000000000" if instrument
                           else "07000000000000000000000000000000")),
        (52, vst3_guid(code)),
        (54, name.encode("utf-8")),
        (55, path.encode("utf-8")),
        (56, b"Cruz Audio"),
    ]
    if state is not None:
        records.append((53, fl_vst3_state(state)))
    return struct.pack("<I", 10) + wrapper_records(records)


def echo_state(note=None, ms=None, feedback=30, style=2, mode=1, low_cut=20, high_cut=20000,
               saturation=15, wobble=15, width=100, duck=0):
    notes = ["1/64", "1/32T", "1/32", "1/32D", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
             "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1/1"]
    params = {
        "sync": 1.0 if note else 0.0,
        "timeNote": float(notes.index(note)) if note else 11.0,
        "timeMs": float(ms or 375.0),
        "feel": 0.0,
        "feedback": float(feedback),
        "groove": 0.0,
        "style": float(style),        # Digital, Analog, Tape, Space, Lo-Fi
        "mode": float(mode),          # Single, Stereo, Ping-Pong
        "lowCut": float(low_cut),
        "highCut": float(high_cut),
        "saturation": float(saturation),
        "wobble": float(wobble),
        "width": float(width),
        "duck": float(duck),
        "mix": 100.0,                 # a send return is all wet
        "output": 0.0,
    }
    body = "".join(f'<PARAM id="{k}" value="{v:g}"/>' for k, v in params.items())
    return juce_state(f"<CruzEcho>{body}</CruzEcho>")


SLOPES = [6, 12, 18, 24, 30, 36, 48, 72, 96]


def eq_bandpass_state(low_cut, high_cut, slope_db=24):
    """Cruz EQ with band 1 a low cut and band 2 a high cut: a band-pass that
    pre-shapes a send return."""
    slope = float(SLOPES.index(slope_db))
    params = {
        "b1_used": 1.0, "b1_shape": 2.0, "b1_freq": float(low_cut), "b1_slope": slope,
        "b2_used": 1.0, "b2_shape": 4.0, "b2_freq": float(high_cut), "b2_slope": slope,
    }
    body = "".join(f'<PARAM id="{k}" value="{v:g}"/>' for k, v in params.items())
    return juce_state(f"<CruzEQ>{body}</CruzEQ>")


def verb_state():
    params = {"mix": 100.0, "decay": 2.2, "predelay": 30.0, "ducking": 25.0, "width": 120.0}
    body = "".join(f'<PARAM id="{k}" value="{v:g}"/>' for k, v in params.items())
    return juce_state(f'<CruzVerb program="0">{body}</CruzVerb>')


# ------------------------------------------------------------ the session --

# Drum placeholders: (channel name, Cruz Drums file, mixer insert)
DRUM_SAMPLES = [
    ("Kick", "Pop Kick 07 - Punch Tight (F#).wav", 1),
    ("Snare", "Pop Snare 07 - Layered Crisp Big Room.wav", 2),
    ("Clap", "Pop Clap 02 - Group Crisp Tight Roomy.wav", 3),
    ("Hats", "Pop Hat 01 - Closed Crisp Short.wav", 4),
    ("Perc", "Pop Perc 08 - Tambourine Crisp Roomy.wav", 5),
    ("Rim", "Pop Rim 01 - Cross Stick Roomy (A).wav", 6),
]
SC_SAMPLE = "Pop Kick 07 - Punch Tight (F#).wav"

# Mixer inserts. name, colour, icon, routes {dest: level or None}, slots, extras.
#   level None = 0 dB route, a number = send level (FL raw, 12800 = 0 dB),
#   "sidechain" = FL's "Sidechain to this track" (a route at level 0).
# Slots are (slot index 0-9, plugin name, state or None); muted lists slots
# that start switched off.
# Icons are FL's shared mixer/playlist icon numbers, matched to the names
# they carry in Pedro's own projects (5 kick, 30 mic, 116 master, 119 reverb...).
DRUM_BUS, SC_BUS, INST_BUS, VOX_BUS, MIX_BUS = 20, 21, 22, 23, 24
REVERB, D12, D14, D18, D116, SLAP, CHORUS, PHASER = 26, 27, 28, 29, 30, 31, 32, 33

SEND_HI, SEND_MID, SEND_LO = 7200, 5000, 3200

INSERTS = {
    0: dict(name="Master", colour="master", icon=116, slots=[(0, "Cruz EQ", None), (9, "Cruz Limiter", None)]),
    1: dict(name="Kick", colour="drums", icon=5, routes={DRUM_BUS: None}, slots=[(0, "Cruz EQ", None)],
            separator=True),
    2: dict(name="Snare", colour="drums", icon=4, routes={DRUM_BUS: None, REVERB: SEND_LO},
            slots=[(0, "Cruz EQ", None), (1, "Cruz FET", None)]),
    3: dict(name="Clap", colour="drums", icon=43, routes={DRUM_BUS: None, REVERB: SEND_LO},
            slots=[(0, "Cruz EQ", None)]),
    4: dict(name="Hats", colour="drums", icon=6, routes={DRUM_BUS: None}, slots=[(0, "Cruz EQ", None)]),
    5: dict(name="Perc", colour="drums", icon=7, routes={DRUM_BUS: None, D18: SEND_LO},
            slots=[(0, "Cruz EQ", None)]),
    6: dict(name="Rim", colour="drums", icon=7, routes={DRUM_BUS: None, REVERB: SEND_LO},
            slots=[(0, "Cruz EQ", None)]),
    7: dict(name="SC Trigger", colour="sc", icon=93, routes={SC_BUS: "sidechain"}, separator=True),
    10: dict(name="Bass", colour="bass", icon=14, routes={SC_BUS: None},
             slots=[(0, "Cruz EQ", None), (1, "Cruz Opto", None)], separator=True),
    11: dict(name="Keys", colour="keys", icon=12, routes={SC_BUS: None, REVERB: SEND_MID, D14: SEND_LO},
             slots=[(0, "Cruz EQ", None)]),
    12: dict(name="Lead", colour="lead", icon=13, routes={INST_BUS: None, REVERB: SEND_MID, D18: SEND_LO,
                                                  D116: SEND_LO},
             slots=[(0, "Cruz EQ", None), (1, "Cruz Drive", None)]),
    14: dict(name="Vocal Rec", colour="rec", icon=30, routes={VOX_BUS: None, REVERB: SEND_MID, SLAP: SEND_LO},
             slots=[(0, "Cruz EQ", None), (1, "Cruz FET", None), (2, "Cruz Opto", None)],
             separator=True),
    15: dict(name="Lead Vox", colour="vox", icon=20, routes={VOX_BUS: None, REVERB: SEND_MID, D14: SEND_LO,
                                                     D18: SEND_LO, SLAP: SEND_LO},
             slots=[(0, "Cruz EQ", None), (1, "Cruz FET", None), (2, "Cruz Opto", None)]),
    16: dict(name="Vox Double", colour="vox", icon=21, routes={VOX_BUS: None, REVERB: SEND_MID},
             slots=[(0, "Cruz EQ", None), (1, "Cruz Opto", None)]),
    17: dict(name="Harmonies", colour="vox", icon=22, routes={VOX_BUS: None, REVERB: SEND_HI, D12: SEND_LO},
             slots=[(0, "Cruz EQ", None), (1, "Cruz Opto", None)]),
    18: dict(name="Adlibs", colour="vox", icon=21, routes={VOX_BUS: None, REVERB: SEND_MID, D12: SEND_MID,
                                                   D116: SEND_LO},
             slots=[(0, "Cruz EQ", None), (1, "Cruz Opto", None)]),
    DRUM_BUS: dict(name="DRUM BUS", colour="drums_bus", icon=40, routes={MIX_BUS: None},
                   slots=[(0, "Cruz EQ", None), (1, "Cruz FET", None), (2, "Cruz Tape", None)], dock="right",
                   separator=True),
    # Cruz Sidechain ducks this bus from SC Trigger (its sidechain route); the EQ follows it.
    SC_BUS: dict(name="SC BUS", colour="sc_bus", icon=93, routes={INST_BUS: None},
                 slots=[(0, "Cruz Sidechain", None), (1, "Cruz EQ", None)], dock="right"),
    INST_BUS: dict(name="INST BUS", colour="inst_bus", icon=69, routes={MIX_BUS: None},
                   slots=[(0, "Cruz EQ", None)], dock="right"),
    # Pedro's vocal bus chain. The first Opto and the FET start switched off.
    VOX_BUS: dict(name="VOCAL BUS", colour="vox_bus", icon=69, routes={MIX_BUS: None},
                  slots=[(0, "Cruz EQ", None), (1, "Cruz Opto", None), (2, "Cruz FET", None),
                         (3, "Cruz EQ", None), (4, "Cruz Drive", None), (5, "Cruz EQ", None),
                         (6, "Cruz Opto", None)],
                  muted={1, 2}, dock="right"),
    MIX_BUS: dict(name="MIX BUS", colour="mix_bus", icon=39, routes={0: None},
                  slots=[(0, "Cruz EQ", None), (1, "Cruz Opto", None), (2, "Cruz Tape", None)],
                  dock="right"),
    # Every send return: the effect, then a Cruz EQ band-pass after it, so the
    # whole effect output is filtered (the "pre-mix").
    REVERB: dict(name="FX Reverb", colour="verb", icon=119, routes={MIX_BUS: None},
                 slots=[(0, "Cruz Verb", "verb"), (1, "Cruz EQ", ("bp", 300, 8000))], dock="right",
                 separator=True),
    D12: dict(name="FX Delay 1/2", colour="delay", icon=26, routes={MIX_BUS: None},
              slots=[(0, "Cruz Echo", ("1/2", None, 35, 2, 1)), (1, "Cruz EQ", ("bp", 400, 5000))],
              dock="right"),
    D14: dict(name="FX Delay 1/4", colour="delay", icon=26, routes={MIX_BUS: None},
              slots=[(0, "Cruz Echo", ("1/4", None, 30, 2, 2)), (1, "Cruz EQ", ("bp", 350, 6000))],
              dock="right"),
    D18: dict(name="FX Delay 1/8", colour="delay", icon=26, routes={MIX_BUS: None},
              slots=[(0, "Cruz Echo", ("1/8", None, 25, 1, 2)), (1, "Cruz EQ", ("bp", 400, 7000))],
              dock="right"),
    D116: dict(name="FX Delay 1/16", colour="delay", icon=26, routes={MIX_BUS: None},
               slots=[(0, "Cruz Echo", ("1/16", None, 20, 0, 1)), (1, "Cruz EQ", ("bp", 500, 8000))],
               dock="right"),
    SLAP: dict(name="FX Slap", colour="slap", icon=26, routes={MIX_BUS: None},
               slots=[(0, "Cruz Echo", (None, 95, 5, 1, 0)), (1, "Cruz EQ", ("bp", 250, 6000))],
               dock="right"),
    # "chorus" and "phaser" become the variant's FL effect (see VARIANTS).
    CHORUS: dict(name="FX Chorus", colour="chorus", icon=118, routes={MIX_BUS: None},
                 slots=[(0, "chorus", None), (1, "Cruz EQ", ("bp", 200, 12000))], dock="right"),
    PHASER: dict(name="FX Phaser", colour="phaser", icon=118, routes={MIX_BUS: None},
                 slots=[(0, "phaser", None), (1, "Cruz EQ", ("bp", 250, 10000))], dock="right"),
}

# Channel rack: (name, kind, insert, colour, extra)
CHANNELS = [(n, "sampler", ins, "drums", f) for n, f, ins in DRUM_SAMPLES]
CHANNELS += [
    ("SC Trigger", "sampler", 7, "sc", SC_SAMPLE),
    ("Bass", "synth", 10, "bass", "bass"),
    ("Keys", "synth", 11, "keys", "keys"),
    ("Lead", "synth", 12, "lead", "lead"),
]

# Song structure in bars (modern pop, 92 bars = about 3:04 at 120 BPM).
SECTIONS = [
    ("INTRO", 4, "#6C7A89"),
    ("VERSE 1", 8, "#3FA34D"),
    ("PRE-CHORUS 1", 8, "#E8A33D"),
    ("CHORUS 1", 8, "#E8453C"),
    ("POST-CHORUS", 4, "#D9407A"),
    ("VERSE 2", 8, "#2F8A3C"),
    ("PRE-CHORUS 2", 4, "#E8A33D"),
    ("BUILD-UP", 4, "#F27B21"),
    ("CHORUS 2", 8, "#E8453C"),
    ("BRIDGE", 8, "#8A4FD8"),
    ("BREAK", 4, "#4A5568"),
    ("BUILD-UP", 4, "#F27B21"),
    ("FINAL CHORUS", 16, "#FF2E2E"),
    ("OUTRO", 4, "#6C7A89"),
]

# Empty, named clips laid on their lanes. Lane -> (pattern name, sections it plays in).
FULL = {"CHORUS 1", "CHORUS 2", "FINAL CHORUS"}
LANE_PLAN = [
    # (track name, colour, pattern name or None for an empty lane, sections)
    ("SECTIONS", "sections", None, None),
    ("Chords (Keys)", "keys", "Chords", {s for s, _, _ in SECTIONS}),
    ("Kick", "drums", "Kick", {"VERSE 1", "PRE-CHORUS 1", "POST-CHORUS", "VERSE 2", "PRE-CHORUS 2"} | FULL),
    ("Snare", "drums", "Snare", {"PRE-CHORUS 1", "PRE-CHORUS 2", "POST-CHORUS", "BRIDGE"} | FULL),
    ("Clap", "drums", "Clap", {"POST-CHORUS"} | FULL),
    ("Hats", "drums", "Hats", {"INTRO", "VERSE 1", "PRE-CHORUS 1", "POST-CHORUS", "VERSE 2", "PRE-CHORUS 2",
                               "BRIDGE", "OUTRO"} | FULL),
    ("Perc", "drums", "Perc", {"PRE-CHORUS 1", "VERSE 2", "POST-CHORUS"} | FULL),
    ("Rim", "drums", "Rim", {"VERSE 1", "VERSE 2", "BRIDGE"}),
    ("Build / Fills", "drums", "Build", {"BUILD-UP"}),
    ("SC Trigger", "sc", "SC Trigger", {"VERSE 1", "PRE-CHORUS 1", "POST-CHORUS", "VERSE 2", "PRE-CHORUS 2"} | FULL),
    ("Bass", "bass", "Bass", {"VERSE 1", "PRE-CHORUS 1", "POST-CHORUS", "VERSE 2", "PRE-CHORUS 2", "BRIDGE",
                              "BUILD-UP"} | FULL),
    ("Lead", "lead", "Lead", {"POST-CHORUS", "BRIDGE"} | FULL),
    ("Vocal Rec", "rec", None, None),
    ("Lead Vox", "vox", None, None),
    ("Vox Double", "vox", None, None),
    ("Harmonies", "vox", None, None),
    ("Adlibs", "vox", None, None),
    ("FX / Risers", "fx", None, None),
    ("Automation", "auto", None, None),
]
CLIP_BARS = 4   # named clips are 4 bars long, repeated through each section
TRACK_ICONS = {"Chords (Keys)": 12, "Kick": 5, "Snare": 4, "Clap": 43, "Hats": 6, "Perc": 7,
               "Rim": 7, "Build / Fills": 115, "SC Trigger": 93, "Bass": 14, "Lead": 13,
               "Vocal Rec": 30, "Lead Vox": 20, "Vox Double": 21, "Harmonies": 22,
               "Adlibs": 21, "FX / Risers": 115}

# ---------------------------------------------------------- event writing --


class Events:
    def __init__(self):
        self.buf = bytearray()

    def add(self, eid, value):
        if eid < 64:
            self.buf += bytes([eid]) + struct.pack("<B", value & 0xFF)
        elif eid < 128:
            self.buf += bytes([eid]) + struct.pack("<H", value & 0xFFFF)
        elif eid < 192:
            self.buf += bytes([eid]) + struct.pack("<I", value & 0xFFFFFFFF)
        else:
            data = bytes(value)
            n = len(data)
            out = bytearray([eid])
            while True:
                b = n & 0x7F
                n >>= 7
                out.append(b | (0x80 if n else 0))
                if not n:
                    break
            self.buf += out + data

    def text(self, eid, s):
        self.add(eid, (s + "\0").encode("utf-16-le"))

    def ascii(self, eid, s):
        self.add(eid, (s + "\0").encode("ascii"))

    def raw(self, items):
        for eid, hexdata in items:
            data = bytes.fromhex(hexdata)
            if eid < 192:
                self.buf += bytes([eid]) + data
            else:
                self.add(eid, data)


def patch(items, eid, data_hex):
    return [(e, data_hex if e == eid else d) for e, d in items]


def timestamp():
    epoch = datetime.datetime(1899, 12, 30)
    now = (datetime.datetime(2026, 10, 29) - epoch).days
    return struct.pack("<dd", float(now), 0.0)


def build(template, synth_states):
    variant = VARIANTS[template]
    ev = Events()

    # --- project ---
    ev.ascii(199, FL_VERSION)
    ev.add(159, FL_BUILD)
    ev.add(28, 1)
    ev.add(37, 1)
    ev.add(156, TEMPO * 1000)
    ev.add(67, 1)
    ev.add(9, 1)
    ev.add(11, 0)
    ev.add(80, 0)
    ev.add(17, 4)
    ev.add(18, 4)
    ev.add(35, 1)
    ev.add(23, 0)
    ev.add(30, 1)
    ev.add(10, 1)
    ev.text(194, template)
    ev.text(206, "Pop")
    ev.text(207, "Pedro Cruz")
    ev.text(202, "")
    ev.text(195, PROJECT_NOTES)
    ev.add(237, timestamp())
    for group in ("Drums", "Synths", "Sidechain"):
        ev.text(231, group)
    ev.add(146, 0xFFFFFFFF)
    ev.add(216, b"")

    # --- patterns: section markers + one empty clip per lane, no notes ---
    patterns = [(name, colour) for name, _, colour in SECTIONS]
    seen = []
    for name, colour in patterns:
        if name not in seen:
            seen.append(name)
    section_patterns = {name: i + 1 for i, name in enumerate(seen)}
    section_colours = {name: c for name, _, c in SECTIONS}
    lane_patterns = {}
    nxt = len(seen) + 1
    for track, colour, pat, _ in LANE_PLAN:
        if pat and pat not in lane_patterns:
            lane_patterns[pat] = (nxt, C[colour])
            nxt += 1
    all_patterns = [(i, n, section_colours[n]) for n, i in section_patterns.items()]
    all_patterns += [(i, n, c) for n, (i, c) in lane_patterns.items()]

    # --- channel rack ---
    def channel(index, name, kind, insert, colour, extra):
        group = 0 if kind == "sampler" and name != "SC Trigger" else (2 if name == "SC Trigger" else 1)
        ev.add(64, index)
        if kind == "sampler":
            ev.add(21, 0)
            ev.text(201, "")
            ev.add(212, bytes.fromhex(D.SAMPLER_212))
            ev.text(203, name)
            ev.add(155, 0)
            ev.add(128, rgb(C[colour]))
            tail = D.SAMPLER_CHANNEL_TAIL
        else:
            ev.add(21, 2)
            ev.text(201, "Fruity Wrapper")
            ev.add(212, bytes.fromhex(D.PLUGIN_CHANNEL_212))
            ev.text(203, name)
            ev.add(155, 0)
            ev.add(128, rgb(C[colour]))
            ev.add(213, vst3_wrapper("Cruz Synth", synth_states.get(extra)))
            tail = D.PLUGIN_CHANNEL_TAIL
        tail = patch(tail, 22, f"{insert:02x}")
        tail = patch(tail, 145, struct.pack("<I", group).hex())
        ev.raw(tail)
        if kind == "sampler":
            ev.text(196, sample_dir(template) + extra)

    for idx, (name, kind, insert, colour, extra) in enumerate(CHANNELS):
        channel(idx, name, kind, insert, colour, extra)
        if idx == 0:
            for pid, pname, pcolour in sorted(all_patterns):
                ev.add(65, pid)
                ev.text(193, pname)
                ev.add(150, rgb(pcolour) if isinstance(pcolour, str) else pcolour)
                ev.add(157, 0xFFFFFFFF)
                ev.add(158, 0xFFFFFFFF)
                ev.add(164, 0)

    # --- playlist ---
    ev.add(99, 0)
    ev.text(241, "Arrangement")
    ev.add(36, 0)
    items = bytearray()
    starts = []
    pos = 0
    for name, bars, _ in SECTIONS:
        starts.append((name, pos, bars))
        pos += bars * BAR

    def item(position, pattern, length, track):
        return (struct.pack("<IHHIHH", position, 20480, 20480 + pattern, length, NUM_TRACKS - 1 - track, 0)
                + b"\x78\x00" + struct.pack("<H", 64) + bytes([64, 100, 128, 128])
                + bytes.fromhex(D.PATTERN_ITEM_OFFSETS))

    clips = []
    for name, start, bars in starts:
        clips.append((start, section_patterns[name], bars * BAR, 0))
    for track, (tname, colour, pat, plays) in enumerate(LANE_PLAN):
        if not pat:
            continue
        pid = lane_patterns[pat][0]
        for name, start, bars in starts:
            if name in plays:
                for b in range(0, bars, CLIP_BARS):
                    clips.append((start + b * BAR, pid, min(CLIP_BARS, bars - b) * BAR, track))
    for c in sorted(clips):
        items += item(*c)
    ev.add(233, bytes(items))

    for name, start, bars in starts:
        ev.add(148, start)
        ev.add(33, 4)
        ev.add(34, 4)
        ev.text(205, name)

    track_default = bytearray(bytes.fromhex(D.TRACK_DATA))
    for t in range(NUM_TRACKS):
        data = bytearray(track_default)
        struct.pack_into("<I", data, 0, t + 1)
        name = None
        if t < len(LANE_PLAN):
            name, colour, _, _ = LANE_PLAN[t]
            struct.pack_into("<I", data, 4, rgb(C[colour]))
            struct.pack_into("<I", data, 8, TRACK_ICONS.get(name, 0))
            if t == 0:
                struct.pack_into("<f", data, 13, 1.35)   # taller section lane
        ev.add(238, bytes(data))
        if name:
            ev.text(239, name)

    ev.add(100, 0)
    ev.add(29, 1)
    ev.add(39, 1)
    ev.add(40, 0)
    ev.add(31, 0)
    ev.add(38, 3)

    # --- mixer ---
    for i in range(NUM_INSERTS):
        spec = INSERTS.get(i, {})
        if spec:
            ev.add(149, rgb(C[spec["colour"]]))
            ev.add(95, spec.get("icon", 0))
            ev.text(204, spec["name"])
        flags = 0x0C if i in (0, NUM_INSERTS - 1) else 0x4C
        if spec.get("dock") == "right":
            flags = 0x8C
        if spec.get("separator"):
            flags |= 0x400
        ev.add(236, struct.pack("<III", 0, flags, 0))
        slots = {s: (p, st) for s, p, st in spec.get("slots", [])}
        for s in range(10):
            if s in slots:
                pname, st = slots[s]
                pname = variant.get(pname, pname)
                if pname in NATIVE:
                    wrapper, state = (bytes.fromhex(h) for h in NATIVE[pname])
                    d = bytearray(wrapper)
                    struct.pack_into("<iiI", d, 0, i, s, 2)
                    struct.pack_into("<I", d, 16, WRAPPER_FLAGS)
                    ev.text(201, pname)
                    ev.add(212, bytes(d))
                    ev.add(155, 0)
                    ev.add(128, rgb(C[spec["colour"]]))
                    ev.add(213, state)
                    ev.add(98, s)
                    continue
                if st == "verb":
                    state = verb_state()
                elif isinstance(st, tuple) and st[0] == "bp":
                    state = eq_bandpass_state(st[1], st[2])
                elif isinstance(st, tuple):
                    note, ms, fb, style, mode = st
                    state = echo_state(note=note, ms=ms, feedback=fb, style=style, mode=mode)
                else:
                    state = None
                ev.text(201, "Fruity Wrapper")
                d = bytearray(bytes.fromhex(D.EFFECT_SLOT_212))
                struct.pack_into("<ii", d, 0, i, s)   # 212: insert @0, slot @4
                ev.add(212, bytes(d))
                ev.text(203, pname)
                ev.add(155, 0)
                ev.add(128, rgb(C[spec["colour"]]))
                ev.add(213, vst3_wrapper(pname, state))
            ev.add(98, s)
        routing = bytearray(NUM_INSERTS)
        if i not in (0, NUM_INSERTS - 1):
            routes = spec.get("routes", {0: None})
            for dest in routes:
                routing[dest] = 1
        ev.add(235, bytes(routing))
        ev.add(154, 0xFFFFFFFF)
        ev.add(147, 0 if i == 0 else 0xFFFFFFFF)

    ev.add(225, mixer_params())
    ev.add(133, 646)

    data = bytes(ev.buf)
    header = b"FLhd" + struct.pack("<IhHH", 6, 0, len(CHANNELS), PPQ)
    return header + b"FLdt" + struct.pack("<I", len(data)) + data


def mixer_params():
    """The 225 record list: FL's defaults for every insert plus our send levels.
    IDs use FL 20's layout 0x20000000 + insert<<22 + slot<<16 + control."""
    recs = [(0x40000000, 12800)]

    def rid(insert, slot, control):
        return 0x20000000 + (insert << 22) + (slot << 16) + control

    for i in range(NUM_INSERTS):
        spec = INSERTS.get(i, {})
        muted = spec.get("muted", set())
        for s in range(10):
            recs.append((rid(i, s, 0x1F00), 0 if s in muted else 1))
            recs.append((rid(i, s, 0x1F01), 12800))
        recs += [(rid(i, 0, 0x1FC0), 12800), (rid(i, 0, 0x1FC1), 0), (rid(i, 0, 0x1FC2), 0)]
        recs += [(rid(i, 0, 0x1FD0 + b), 0) for b in range(3)]
        recs += [(rid(i, 0, 0x1FD8 + b), f) for b, f in enumerate((5777, 33145, 55825))]
        recs += [(rid(i, 0, 0x1FE0 + b), 17500) for b in range(3)]
        for dest, level in spec.get("routes", {}).items():
            if level is None:
                continue
            value = 0 if level == "sidechain" else int(level)
            recs.append((rid(i, 0, 0x1F40 + dest), value))
        recs.append((rid(i, 0, 0x1F40 + NUM_INSERTS - 1), 0))
    return b"".join(struct.pack("<4sIi", b"\0" * 4, r, v) for r, v in recs)


PROJECT_NOTES = (
    "CRUZ POP MIX TEMPLATE\r\r"
    "Signal flow: tracks > buses (DRUM, INST, VOCAL) > MIX BUS > Master (Cruz Limiter).\r"
    "Bass and Keys go through SC BUS, which ducks from the SC Trigger channel "
    "(sidechain route, silent in the mix) through Cruz Sidechain in SC BUS slot 1.\r"
    "VOCAL BUS: EQ > Opto (off) > FET (off) > EQ > Drive > EQ > Opto.\r"
    "Sends: FX Reverb, FX Delay 1/2, 1/4, 1/8, 1/16, FX Slap, FX Chorus and FX Phaser all return "
    "to MIX BUS. On each, a Cruz EQ band-pass comes after the effect, so the whole return "
    "is pre-shaped.\r"
    "Vocal Rec: pick your mic input on that insert, then arm it.\r"
    "Patterns are empty on purpose. Write your chords into the Chords pattern and they "
    "play in every section that already has a Chords clip.\r"
)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dir")
    args = ap.parse_args()
    here = os.path.dirname(os.path.abspath(__file__))
    states = {}
    for part in ("keys", "bass", "lead"):
        with open(os.path.join(here, "synth_states", part + ".xml"), encoding="utf-8") as f:
            states[part] = juce_state(f.read().rstrip("\n"))
    for template in VARIANTS:
        out = os.path.join(args.out_dir, template)
        os.makedirs(out, exist_ok=True)
        path = os.path.join(out, template + ".flp")
        with open(path, "wb") as f:
            f.write(build(template, states))
        print(path)


if __name__ == "__main__":
    main()
