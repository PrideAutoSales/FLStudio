#!/usr/bin/env python3
"""Check a built Cruz Pop Mix folder: the .flp parses, holds no notes, every
plugin sits in the slot and insert it names, the sends and vocal bus are
chained the way Pedro asked, and every sample the project points to is there.

    python3 check_template.py "dist/CruzPopMix-mac/Cruz Pop Mix"
"""

import os
import struct
import sys

import make_template as T


def events(path):
    b = open(path, "rb").read()
    assert b[:4] == b"FLhd", "not an FL project"
    i = 8 + struct.unpack_from("<I", b, 4)[0]
    assert b[i:i + 4] == b"FLdt"
    end = i + 8 + struct.unpack_from("<I", b, i + 4)[0]
    i += 8
    out = []
    while i < end:
        eid = b[i]
        i += 1
        if eid < 64:
            n = 1
        elif eid < 128:
            n = 2
        elif eid < 192:
            n = 4
        else:
            n = shift = 0
            while True:
                c = b[i]
                i += 1
                n |= (c & 0x7F) << shift
                shift += 7
                if not c & 0x80:
                    break
        out.append((eid, b[i:i + n]))
        i += n
    assert i == end, "event data overruns the file"
    return out


def text(d):
    return d.decode("utf-16-le").rstrip("\0")


def main(folder):
    template = os.path.basename(os.path.normpath(folder))
    ev = events(os.path.join(folder, template + ".flp"))
    errors = []

    if any(e == 224 for e, _ in ev):
        errors.append("the project has notes in it")

    # Mixer: walk inserts (each ends with a 147) and collect their slots.
    params = [d for e, d in ev if e == 225][0]
    off = set()
    for k in range(0, len(params), 12):
        _, rid, value = struct.unpack_from("<4sIi", params, k)
        if 0x20000000 <= rid < 0x40000000 and rid & 0xFFFF == 0x1F00 and value == 0:
            rel = rid - 0x20000000
            off.add((rel >> 22, (rel >> 16) & 0x3F))
    start = next(k for k, (e, _) in enumerate(ev) if e == 236)
    while ev[start - 1][0] in (149, 95, 204):
        start -= 1
    inserts, insert, plugin, slots = [], 0, None, {}
    for e, d in ev[start:]:
        if e == 201:
            plugin = text(d)
        elif e == 212 and plugin is not None:
            at, slot = struct.unpack_from("<ii", d, 0)
            if at != insert:
                errors.append(f"insert {insert}: a plugin says it belongs to insert {at}")
            slots[slot] = plugin
        elif e == 203 and plugin == "Fruity Wrapper" and slots:
            slots[max(slots)] = text(d)
        elif e == 147:
            inserts.append(slots)
            insert, plugin, slots = insert + 1, None, {}
    if len(inserts) != T.NUM_INSERTS:
        errors.append(f"{len(inserts)} mixer inserts, expected {T.NUM_INSERTS}")

    variant = T.VARIANTS[template]
    for i, spec in T.INSERTS.items():
        want = {s: variant.get(p, p) for s, p, _ in spec.get("slots", [])}
        if inserts[i] != want:
            errors.append(f"{spec['name']}: slots {inserts[i]}, expected {want}")
        muted = {s for (n, s) in off if n == i}
        if muted != spec.get("muted", set()):
            errors.append(f"{spec['name']}: slots switched off {sorted(muted)}")

    sends = [i for i, s in T.INSERTS.items() if s["name"].startswith("FX ")]
    for i in sends:
        chain = [inserts[i][s] for s in sorted(inserts[i])]
        if len(chain) < 2 or chain[-1] != "Cruz EQ" or "Cruz EQ" in chain[:-1]:
            errors.append(f"{T.INSERTS[i]['name']}: the EQ must come last, after the effect ({chain})")
    if "Cruz Limiter" not in inserts[0].values() or "Fruity Limiter" in inserts[0].values():
        errors.append("the master must use Cruz Limiter and not Fruity Limiter")
    vocal = [inserts[T.VOX_BUS][s] for s in sorted(inserts[T.VOX_BUS])]
    if vocal != ["Cruz EQ", "Cruz Opto", "Cruz FET", "Cruz EQ", "Cruz Drive", "Cruz EQ", "Cruz Opto"]:
        errors.append(f"VOCAL BUS chain is {vocal}")

    prefix = T.sample_dir(template)
    for e, d in ev:
        if e == 196:
            path = text(d)
            if not path.startswith(prefix):
                errors.append(f"sample outside the template folder: {path}")
            elif not os.path.isfile(os.path.join(folder, "Samples", path[len(prefix):])):
                errors.append(f"missing sample: {path[len(prefix):]}")

    for msg in errors:
        print("FAIL:", msg)
    print(f"{template}: {len(ev)} events, {sum(1 for s in inserts if s)} inserts with plugins, "
          f"{'OK' if not errors else str(len(errors)) + ' problem(s)'}")
    return not errors


if __name__ == "__main__":
    ok = all([main(f) for f in sys.argv[1:]])
    sys.exit(0 if ok else 1)
