"""Build the Cruz Drums pack.

    python3 generate.py [out_dir]

For every genre and drum type this renders a pool of candidates, rejects any that
fail the checks below, then keeps the 12 that differ most from each other (so no two
samples in a folder are near copies). Output: 24-bit 48 kHz WAVs in Genre/Type folders,
plus report.csv with the measurements of every kept sample.
"""

import csv
import os
import sys
import zlib
from multiprocessing import Pool

import numpy as np
import soundfile as sf
from scipy import signal

from drumsynth import SR, finish
from genres import GENRES
from instruments import VOICES

KEEP = 12
POOL = 22
SINGULAR = {"Kicks": "Kick", "Snares": "Snare", "Claps": "Clap", "Rims": "Rim", "Hats": "Hat", "Percs": "Perc"}
MAX_LEN = {"Kicks": 2.8, "Snares": 1.4, "Claps": 1.6, "Rims": 0.8, "Hats": 1.0, "Percs": 1.6}


def db(v):
    return 20 * np.log10(max(v, 1e-12))


def measure(x):
    mono = x.mean(axis=0)
    n = mono.size
    tp = np.max(np.abs(signal.resample_poly(x, 4, 1, axis=-1)))
    env = np.abs(signal.hilbert(mono))
    env = np.convolve(env, np.ones(48) / 48, mode="same")
    pk = np.argmax(env)
    below = np.nonzero(env[pk:] < env[pk] * 0.1)[0]
    t20 = (below[0] if len(below) else n - pk) / SR
    spec = np.abs(np.fft.rfft(mono * np.hanning(n), 1 << int(np.ceil(np.log2(max(n, 4096))))))
    freqs = np.fft.rfftfreq((len(spec) - 1) * 2, 1 / SR)
    centroid = np.sum(freqs * spec) / (np.sum(spec) + 1e-12)
    flat = np.exp(np.mean(np.log(spec + 1e-12))) / (np.mean(spec) + 1e-12)
    dom = freqs[np.argmax(spec * (freqs > 25))]
    hf = signal.sosfilt(signal.butter(2, 8000 / (SR / 2), "hp", output="sos"), mono)
    hf_spike = np.max(np.abs(hf[int(0.05 * n):])) / (np.max(np.abs(mono)) + 1e-12) if n > 2000 else 0
    rms = np.sqrt(np.mean(mono[: min(n, int(0.1 * SR))] ** 2))
    return dict(
        length=n / SR, true_peak_db=db(tp), sample_peak_db=db(np.max(np.abs(x))),
        dc=float(np.max(np.abs(x.mean(axis=1)))), start=float(np.max(np.abs(x[:, 0]))),
        end_db=db(np.sqrt(np.mean(x[:, -96:] ** 2))), t20=t20, centroid=centroid, flatness=flat,
        dominant=dom, crest_db=db(np.max(np.abs(mono)) / (np.sqrt(np.mean(mono ** 2)) + 1e-12)),
        rms100_db=db(rms), channels=x.shape[0], hf_spike=hf_spike,
    )


def qa(m, kind):
    fails = []
    if not np.isfinite(m["true_peak_db"]):
        fails.append("nan")
    if m["true_peak_db"] > -0.25 or m["sample_peak_db"] > -0.25:
        fails.append("over")
    if m["dc"] > 0.002:
        fails.append("dc")
    if m["start"] > 0.01:
        fails.append("start click")
    if m["end_db"] > -50:
        fails.append("end click")
    if m["length"] < 0.03:
        fails.append("too short")
    if kind == "Kicks" and m["dominant"] > 120:
        fails.append("kick lacks low end")
    if kind in ("Kicks", "Snares", "Claps") and m["rms100_db"] < -24:
        fails.append("weak")
    if kind == "Hats" and m["centroid"] < 4000:
        fails.append("dull hat")
    return fails


def features(m):
    return np.array([np.log(m["length"]), np.log(m["t20"] + 1e-3), np.log(m["centroid"]),
                     np.log(m["dominant"] + 20), m["crest_db"] / 6, m["flatness"] * 4])


def pick(cands, keep):
    """Group-aware farthest point selection: keeps the mix of sub-kinds, drops near duplicates."""
    groups = {}
    for c in cands:
        groups.setdefault(c["group"], []).append(c)
    total = len(cands)
    quota = {g: max(1, round(keep * len(v) / total)) for g, v in groups.items()}
    while sum(quota.values()) > keep:
        g = max(quota, key=lambda k: quota[k] - len(groups[k]) * keep / total)
        quota[g] -= 1
    while sum(quota.values()) < keep:
        g = max(quota, key=lambda k: len(groups[k]) - quota[k])
        quota[g] += 1
    F = np.array([c["feat"] for c in cands])
    F = (F - F.mean(0)) / (F.std(0) + 1e-9)
    for c, f in zip(cands, F):
        c["z"] = f
    chosen = []
    for g, v in groups.items():
        sel = [min(v, key=lambda c: np.sum(c["z"] ** 2))]
        while len(sel) < min(quota[g], len(v)):
            rest = [c for c in v if all(c is not s for s in sel)]
            sel.append(max(rest, key=lambda c: min(np.sum((c["z"] - s["z"]) ** 2) for s in sel + chosen)))
        chosen += sel
    return chosen


def build(args):
    genre, kind, out = args
    style = GENRES[genre]
    voice = VOICES[kind]
    cands, rejected = [], []
    for i in range(POOL):
        seed = zlib.crc32(f"{genre}/{kind}/{i}".encode())
        rng = np.random.default_rng(seed)
        x, words, note = voice(rng, style, i)
        x = finish(x, max_len=MAX_LEN[kind])
        m = measure(x)
        fails = qa(m, kind)
        if fails:
            rejected.append((i, fails))
            continue
        group = words[0] if kind in ("Hats", "Percs", "Rims") else ("808" if words[0] == "808" else "kick")
        cands.append(dict(x=x, words=words, note=note, m=m, feat=features(m), group=group, i=i))
    chosen = pick(cands, KEEP) if len(cands) > KEEP else cands
    chosen.sort(key=lambda c: (c["group"], c["m"]["dominant"]))
    rows = []
    folder = os.path.join(out, genre, kind)
    os.makedirs(folder, exist_ok=True)
    for k, c in enumerate(chosen, 1):
        words = list(dict.fromkeys(c["words"]))
        name = f"{genre} {SINGULAR[kind]} {k:02d} - {' '.join(words)}"
        if c["note"]:
            name += f" ({c['note']})"
        path = os.path.join(folder, name + ".wav")
        sf.write(path, c["x"].T, SR, subtype="PCM_24")
        rows.append(dict(file=os.path.relpath(path, out), **{k2: round(v, 4) if isinstance(v, float) else v
                                                              for k2, v in c["m"].items()}))
    return genre, kind, len(cands), rejected, rows


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "Cruz Drums"
    jobs = [(g, k, out) for g in GENRES for k in VOICES]
    with Pool() as p:
        results = p.map(build, jobs)
    allrows = []
    for genre, kind, ok, rejected, rows in results:
        print(f"{genre:14s} {kind:7s} kept {len(rows):2d} of {ok:2d} passing, {len(rejected)} rejected"
              + (f" {rejected}" if rejected else ""))
        if len(rows) < 10:
            raise SystemExit(f"Only {len(rows)} samples for {genre}/{kind}")
        allrows += rows
    with open(os.path.join(out, "report.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(allrows[0]))
        w.writeheader()
        w.writerows(allrows)
    print(f"{len(allrows)} samples written to {out}")


if __name__ == "__main__":
    main()
