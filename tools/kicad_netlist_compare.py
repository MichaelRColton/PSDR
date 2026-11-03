#!/usr/bin/env python3
"""Compare a KiCad-exported XML netlist against the PADS-derived netlist.

Connectivity is compared as a partition of (refdes, pin) pairs, so net-name
sanitising inside KiCad cannot hide a wrong or missing connection.

Usage:
    python tools/kicad_netlist_compare.py \
        --pads dist/netlist/PSDR_2.6.1_netlist.json \
        --kicad PSDR_2.6.1.xml
"""
import argparse
import json
import sys
import xml.etree.ElementTree as ET


def pads_partition(doc):
    nets = {}
    for name, pins in doc["nets"].items():
        nets[name] = {(p["refdes"], p["pin"]) for p in pins}
    return nets


def kicad_partition(path):
    root = ET.parse(path).getroot()
    nets = {}
    for net in root.iter("net"):
        nodes = {(n.get("ref"), n.get("pin")) for n in net.iter("node")}
        if nodes:
            nets[net.get("name")] = nodes
    comps = {}
    for comp in root.iter("comp"):
        comps[comp.get("ref")] = comp.findtext("footprint") or ""
    return nets, comps


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pads", required=True)
    ap.add_argument("--kicad", required=True)
    args = ap.parse_args()

    pads_doc = json.load(open(args.pads, encoding="utf-8"))
    pads = pads_partition(pads_doc)
    kicad_all, comps = kicad_partition(args.kicad)

    universe = set().union(*pads.values())
    kicad = {}
    for name, pins in kicad_all.items():
        keep = pins & universe
        if keep:
            kicad[name] = keep

    pads_sig = {frozenset(v): k for k, v in pads.items()}
    kicad_sig = {frozenset(v): k for k, v in kicad.items()}

    only_pads = [pads_sig[s] for s in pads_sig.keys() - kicad_sig.keys()]
    only_kicad = [kicad_sig[s] for s in kicad_sig.keys() - pads_sig.keys()]

    seen = {}
    dup = []
    for pins in kicad.values():
        for pin in pins:
            if pin in seen:
                dup.append((pin, seen[pin]))
            seen[pin] = pin
    missing = universe - set(seen)

    print(f"pads nets            {len(pads)}")
    print(f"kicad nets (raw)     {len(kicad_all)}")
    print(f"kicad nets (in pads) {len(kicad)}")
    print(f"pads connected pins  {len(universe)}")
    print(f"kicad covered pins   {len(set(seen) & universe)}")
    print(f"components           {len(comps)}")
    print(f"identical nets       {len(pads_sig.keys() & kicad_sig.keys())}")
    print(f"only in pads         {len(only_pads)}")
    print(f"only in kicad        {len(only_kicad)}")
    print(f"pins missing         {len(missing)}")
    print(f"pins in >1 net       {len(dup)}")
    for name in sorted(only_pads)[:10]:
        print("  pads-only:", name, len(pads[name]), sorted(pads[name])[:4])
    for name in sorted(only_kicad)[:10]:
        print("  kicad-only:", name, len(kicad[name]), sorted(kicad[name])[:4])
    for pin in sorted(missing)[:10]:
        print("  missing pin:", pin)
    if not (only_pads or only_kicad or missing or dup):
        print("RESULT: partitions identical")
        return 0
    print("RESULT: DIFFERENCES FOUND")
    return 1


if __name__ == "__main__":
    sys.exit(main())
