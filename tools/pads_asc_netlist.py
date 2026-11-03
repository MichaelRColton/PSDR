#!/usr/bin/env python3
"""Extract a netlist from a PADS Layout ASCII design database (.asc).

Parses *PARTTYPE* (pin list per part type), *PART* (refdes -> part type) and
*ROUTE* (per-signal connection lines, ``REFDES.PIN`` columns).  The union of
all columns of every *SIGNAL* cluster is a net's pin set; a net may appear in
several clusters when it is split into unconnected copper islands.

Usage:
    python tools/pads_asc_netlist.py Hardware/PSDR.asc \
        --csv dist/netlist/PSDR_2.6.1_netlist.csv \
        --json dist/netlist/PSDR_2.6.1_netlist.json
"""
import argparse
import csv
import json
import re
import sys

PT_HEADER = re.compile(
    r"^(\S+)\s+(\S+)\s+(\S+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+([YN])$")
PT_GATE = re.compile(r"^[GS]\s+\d+\s+(\d+)\s*$")
PT_PIN = re.compile(r"(\d+)\.(\d)\.([A-Z])\.(\S*)")
PART_HEADER = re.compile(
    r"^(\S+)\s{2,}(\S+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+\.\d+)\s+([UG])\s+"
    r"([MN])\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(\d+)\s*$")
SIGNAL = re.compile(r"^\*SIGNAL\*\s+(\S+)\s+(.*)$")
PIN_REF = re.compile(r"^([^.\s]+)\.([^.\s]+)$")


def section(lines, name):
    for i, line in enumerate(lines):
        if line.startswith(name):
            return i
    raise SystemExit(f"section {name} not found")


def parse_parttypes(lines, start, end):
    types = {}
    i = start
    while i < end:
        m = PT_HEADER.match(lines[i].strip())
        if not m:
            i += 1
            continue
        name = m.group(1)
        i += 1
        g = PT_GATE.match(lines[i].strip()) if i < end else None
        if not g:
            raise SystemExit(f"part type {name}: expected G/S line at {i + 1}")
        want = int(g.group(1))
        i += 1
        pins = {}
        while len(pins) < want and i < end:
            for num, _swap, _kind, pin_name in PT_PIN.findall(lines[i]):
                pins[num] = pin_name
            i += 1
        if len(pins) != want:
            raise SystemExit(f"part type {name}: expected {want} pins, got {len(pins)}")
        types[name] = pins
    return types


def parse_parts(lines, start, end):
    parts = {}
    for i in range(start, end):
        m = PART_HEADER.match(lines[i].rstrip())
        if m:
            parts[m.group(1)] = m.group(2)
    return parts


def parse_route(lines, start, end):
    nets = {}
    clusters = {}
    current = None
    for i in range(start, end):
        s = lines[i].strip()
        if not s or s.startswith("*REMARK*"):
            continue
        m = SIGNAL.match(s)
        if m:
            current = m.group(1)
            clusters[current] = clusters.get(current, 0) + 1
            nets.setdefault(current, set())
            continue
        if current is None or re.match(r"^-?\d", s):
            continue
        refs = []
        for column in re.split(r"\s{2,}", s):
            m = PIN_REF.match(column)
            if not m:
                refs = []
                break
            refs.append((m.group(1), m.group(2)))
        for ref in refs:
            nets[current].add(ref)
    return nets, clusters


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("asc")
    ap.add_argument("--csv")
    ap.add_argument("--json")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    with open(args.asc, encoding="latin-1") as fh:
        lines = fh.read().splitlines()

    i_pt = section(lines, "*PARTTYPE*")
    i_part = section(lines, "*PART*")
    i_route = section(lines, "*ROUTE*")
    i_end = len(lines)
    for i in range(i_route, len(lines)):
        if lines[i].startswith(("*MISC*", "*END*")):
            i_end = i
            break

    parttypes = parse_parttypes(lines, i_pt, i_part)
    parts = parse_parts(lines, i_part, i_route)
    nets, clusters = parse_route(lines, i_route, i_end)

    problems = []
    for ref, ptype in parts.items():
        if ptype not in parttypes:
            problems.append(f"part {ref}: unknown part type {ptype}")

    rows = []
    connected = set()
    for net in sorted(nets):
        for ref, pin in sorted(nets[net]):
            connected.add((ref, pin))
            ptype = parts.get(ref, "")
            pin_name = parttypes.get(ptype, {}).get(pin, "")
            if ref not in parts:
                problems.append(f"net {net}: unknown refdes {ref}")
            elif pin not in parttypes[ptype]:
                problems.append(f"net {net}: {ref} has no pin {pin}")
            rows.append((net, ref, pin, pin_name, ptype))

    unconnected = []
    for ref in sorted(parts):
        for pin, pin_name in parttypes[parts[ref]].items():
            if (ref, pin) not in connected:
                unconnected.append((ref, pin, pin_name, parts[ref]))

    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf-8") as fh:
            w = csv.writer(fh)
            w.writerow(["net", "refdes", "pin", "pin_name", "part_type"])
            w.writerows(rows)
    if args.json:
        doc = {
            "source": args.asc,
            "part_count": len(parts),
            "net_count": len(nets),
            "connected_pin_count": len(connected),
            "parts": dict(sorted(parts.items())),
            "nets": {n: [{"refdes": r, "pin": p,
                          "pin_name": parttypes.get(parts.get(r, ""), {}).get(p, "")}
                         for r, p in sorted(nets[n])] for n in sorted(nets)},
            "unconnected_pins": [
                {"refdes": r, "pin": p, "pin_name": pn, "part_type": t}
                for r, p, pn, t in unconnected],
        }
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump(doc, fh, indent=1, ensure_ascii=False)

    if not args.quiet:
        placed_pins = sum(len(parttypes[pt]) for pt in parts.values())
        print(f"parts            {len(parts)}")
        print(f"part types       {len(parttypes)}")
        print(f"nets             {len(nets)}")
        print(f"signal clusters  {sum(clusters.values())} "
              f"({sum(1 for c in clusters.values() if c > 1)} nets with >1 island)")
        print(f"connected pins   {len(connected)}")
        print(f"unconnected pins {len(unconnected)}")
        print(f"placed pins      {placed_pins}")
        print(f"problems         {len(problems)}")
        for p in problems[:20]:
            print("  !", p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
