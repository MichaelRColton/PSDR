#!/usr/bin/env python3
"""Report coincident connection points in generated KiCad schematics.

A pin endpoint, a global-label anchor and a no-connect marker connect when
their coordinates match.  This tool lists every point where more than one
object lands and therefore where two different nets could short.

Usage:
    python tools/kicad_sch_geomcheck.py dist/kicad/PSDR_2.6.1.kicad_sch
"""
import argparse
import glob
import os
import sys


def tokenize(text):
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c in "()":
            yield c
            i += 1
        elif c == '"':
            j = i + 1
            buf = []
            while j < n:
                if text[j] == "\\" and j + 1 < n:
                    buf.append(text[j + 1])
                    j += 2
                    continue
                if text[j] == '"':
                    break
                buf.append(text[j])
                j += 1
            yield ("str", "".join(buf))
            i = j + 1
        elif c.isspace():
            i += 1
        else:
            j = i
            while j < n and not text[j].isspace() and text[j] not in '()"':
                j += 1
            yield ("atom", text[i:j])
            i = j


def parse(text):
    stack = [[]]
    for tok in tokenize(text):
        if tok == "(":
            stack.append([])
        elif tok == ")":
            node = stack.pop()
            stack[-1].append(node)
        else:
            stack[-1].append(tok[1])
    return stack[0][0]


def kids(node, name):
    return [c for c in node if isinstance(c, list) and c and c[0] == name]


def kid(node, name):
    found = kids(node, name)
    return found[0] if found else None


def at_xy(node):
    a = kid(node, "at")
    return (round(float(a[1]), 3), round(float(a[2]), 3))


def lib_pins(root):
    out = {}
    libs = kid(root, "lib_symbols")
    if not libs:
        return out
    for sym in kids(libs, "symbol"):
        name = sym[1]
        pins = {}
        for unit in kids(sym, "symbol"):
            for pin in kids(unit, "pin"):
                num = kid(pin, "number")[1]
                x, y = at_xy(pin)
                pins[num] = (x, y)
        out[name] = pins
    return out


def check(path):
    root = parse(open(path, encoding="utf-8").read())
    pins = lib_pins(root)
    objects = {}
    for sym in kids(root, "symbol"):
        lib_id = kid(sym, "lib_id")
        if not lib_id:
            continue
        ref = ""
        for prop in kids(sym, "property"):
            if prop[1] == "Reference":
                ref = prop[2]
                break
        ox, oy = at_xy(sym)
        for num, (x, y) in pins[lib_id[1]].items():
            pt = (round(ox + x, 3), round(oy - y, 3))
            objects.setdefault(pt, []).append(("pin", ref, num))
    for lab in kids(root, "global_label"):
        objects.setdefault(at_xy(lab), []).append(("label", lab[1]))
    for nc in kids(root, "no_connect"):
        objects.setdefault(at_xy(nc), []).append(("nc",))
    conflicts = []
    for pt, items in objects.items():
        if len(items) < 2:
            continue
        refs = {i[1] for i in items if i[0] == "pin"}
        labels = {i[1] for i in items if i[0] == "label"}
        if len(refs) > 1 or len(labels) > 1:
            conflicts.append((pt, items))
    return len(objects), conflicts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root")
    args = ap.parse_args()
    base = args.root[:-len(".kicad_sch")]
    files = [args.root] + sorted(glob.glob(base + "-p*.kicad_sch"))
    bad = 0
    for path in files:
        points, conflicts = check(path)
        print(f"{os.path.basename(path)}: connection points {points}, "
              f"conflicts {len(conflicts)}")
        for pt, items in conflicts[:8]:
            print("   ", pt, items)
        bad += len(conflicts)
    print(f"total conflicts {bad}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
