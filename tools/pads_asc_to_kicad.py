#!/usr/bin/env python3
"""Build a KiCad 10 symbol library and a net-label schematic from PADS data.

Inputs are the PADS Layout ASCII database (pin lists, part placement, routing)
and the 2016-07-31 schematic PDF, which supplies one graphical position per
reference designator per page.  Every symbol pin is tied to a short wire and a
global label carrying the PADS net name; connectivity therefore lives in the
labels, not in reproduced wiring.  The original drawing is the PDF.

Usage:
    python tools/pads_asc_to_kicad.py \
        --asc Hardware/PSDR.asc \
        --pdf Hardware/PSDR_2.6.1_Schematic_2016-07-31.pdf \
        --pcb dist/kicad/PSDR_2.6.1.kicad_pcb \
        --out dist/kicad --name PSDR_2.6.1
"""
import argparse
import json
import math
import os
import re
import sys
import uuid

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pads_asc_netlist as nl

LIB = "PSDR_2.6.1"
PROJECT = "PSDR_2.6.1"
PIN_PITCH = 2.54
PIN_LEN = 3.81
STUB = 2.54
FONT = 1.27
LBL_FONT = 1.0
BODY_MARGIN = 2.54
EDGE = 4.0
TOP_RESERVE = 22.0
PAGE_W = 1584 * 0.3527777778
PAGE_H = 1224 * 0.3527777778

PAGE_NAMES = {
    2: "MCU / GPS / USB",
    3: "Clock (Si5338)",
    4: "Analog switches / drivers",
    5: "RX/TX baseband",
    6: "Audio / microphone",
    7: "UI: paddles / LEDs / LCD",
    8: "Power",
    9: "RF front end",
    10: "Variable filter",
}

PT_HEADER = re.compile(
    r"^(\S+)\s+(\S+)\s+(\S+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+([YN])$")
PT_GATE = re.compile(r"^[GS]\s+\d+\s+(\d+)\s*$")
PT_PIN = re.compile(r"^(\d+)\.(\d)\.([A-Z])\.([^\s]*)$")
PART_HEADER = re.compile(
    r"^(\S+)\s{2,}(\S+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+\.\d+)\s+([UG])\s+"
    r"([MN])\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(-?\d+)\s+(\d+)\s*$")

PREFIX_ORDER = ["C", "R", "U", "L", "Q", "D", "J", "Y", "X", "F", "M",
                "E", "SW", "TP", "FB", "ANT"]


def uid(*key):
    return str(uuid.uuid5(uuid.NAMESPACE_URL, "psdr261/" + "/".join(key)))


def esc(text):
    return (text.replace("\\", "\\\\").replace('"', '\\"')
                .replace("\n", "\\n"))


def overbar(text):
    """PADS marks an overlined token with a leading backslash."""
    return re.sub(r"\\([A-Za-z0-9_]+)", r"~{\1}", text)


SAFE = re.compile(r"[^A-Za-z0-9_.+-]")


def safe_name(ptype, used):
    base = SAFE.sub("_", ptype).strip("_") or "PART"
    if base[0].isdigit():
        base = "P_" + base
    name, n = base, 2
    while name in used and used[name] != ptype:
        name = f"{base}_{n}"
        n += 1
    used[name] = ptype
    return name


def parse_asc(path):
    lines = open(path, encoding="latin-1").read().splitlines()
    i_pt = nl.section(lines, "*PARTTYPE*")
    i_part = nl.section(lines, "*PART*")
    i_route = nl.section(lines, "*ROUTE*")
    i_end = len(lines)
    for i in range(i_route, len(lines)):
        if lines[i].startswith(("*MISC*", "*END*")):
            i_end = i
            break

    type_pins = {}
    i = i_pt
    while i < i_part:
        m = PT_HEADER.match(lines[i].strip())
        if not m:
            i += 1
            continue
        name = m.group(1)
        i += 1
        g = PT_GATE.match(lines[i].strip())
        if not g:
            raise SystemExit(f"{name}: expected G/S line near {i + 1}")
        want = int(g.group(1))
        i += 1
        pins = []
        while len(pins) < want and i < i_part:
            for tok in lines[i].split():
                pm = PT_PIN.match(tok)
                if pm:
                    pins.append((pm.group(1), pm.group(3), pm.group(4)))
            i += 1
        if len(pins) != want:
            raise SystemExit(f"{name}: {want} pins declared, {len(pins)} parsed")
        type_pins[name] = pins

    parts = {}
    for line in lines[i_part:i_route]:
        m = PART_HEADER.match(line.rstrip())
        if m:
            parts[m.group(1)] = {
                "type": m.group(2), "x": int(m.group(3)), "y": int(m.group(4))}

    nets, clusters = nl.parse_route(lines, i_route, i_end)
    return type_pins, parts, nets, clusters, len(lines)


def read_pdf_positions(pdf, refs):
    import pymupdf
    doc = pymupdf.open(pdf)
    hits = {}
    for pno in range(doc.page_count):
        for block in doc[pno].get_text("dict")["blocks"]:
            for line in block.get("lines", []):
                for span in line["spans"]:
                    text = span["text"].strip()
                    if text in refs:
                        x0, y0, x1, y1 = span["bbox"]
                        hits.setdefault(text, []).append(
                            (pno + 1, (x0 + x1) / 2, (y0 + y1) / 2))
    return hits


def read_footprints(pcb):
    if not pcb or not os.path.exists(pcb):
        return {}
    text = open(pcb, encoding="utf-8").read()
    out = {}
    for block in re.split(r"\n\t\(footprint ", text)[1:]:
        name = block.split('"')[1]
        m = re.search(r'\(property "Reference" "([^"]+)"', block)
        if m:
            out[m.group(1)] = name
    return out


def prefix_for(ptype, parts):
    counts = {}
    for ref, info in parts.items():
        if info["type"] != ptype:
            continue
        m = re.match(r"^[A-Za-z]+", ref)
        if m:
            counts[m.group(0)] = counts.get(m.group(0), 0) + 1
    for want in PREFIX_ORDER:
        if want in counts:
            return want
    return max(counts, key=counts.get) if counts else "U"


def geometry(pins):
    n = len(pins)
    if n == 1:
        left, right = [], list(pins)
    else:
        half = (n + 1) // 2
        left, right = list(pins[:half]), list(pins[half:])
    longest = max([len(p[2]) for p in pins] + [0])
    width = max(7.62, longest * 0.9 * FONT + 2 * PIN_LEN)
    width = min(width, 60.0)
    width = max(7.62, math.ceil(width / 2.54 - 1e-9) * 2.54)
    rows = max(len(left), len(right), 1)
    height = max(5.08, rows * PIN_PITCH + PIN_PITCH)

    placed = []
    for col, group in ((0, left), (1, right)):
        count = len(group)
        for idx, (num, kind, name) in enumerate(group):
            y = (count - 1) * PIN_PITCH / 2.0 - idx * PIN_PITCH
            if col == 0:
                placed.append((num, kind, name, -(width / 2 + PIN_LEN), y, 0))
            else:
                placed.append((num, kind, name, width / 2 + PIN_LEN, y, 180))
    return {"w": width, "h": height, "pins": placed}


def label_text(name, ptype, ref, pin):
    text = overbar(name)
    return text if text else ""


def label_width(text):
    return len(text) * LBL_FONT * 0.85 + 3.0


def sym_lines(full_name, body, prefix, indent="\t\t", external=False):
    w, h = body["w"], body["h"]
    out = []
    a = out.append
    a(f'{indent}(symbol "{full_name}"')
    a(f'{indent}\t(pin_names (offset 0.508))')
    a(f'{indent}\t(exclude_from_sim no)')
    a(f'{indent}\t(in_bom yes)')
    a(f'{indent}\t(on_board yes)')

    def prop(name, value, x, y, hide=False):
        a(f'{indent}\t(property "{name}" "{esc(value)}"')
        a(f'{indent}\t\t(at {x:.4f} {y:.4f} 0)')
        a(f'{indent}\t\t(effects (font (size {FONT} {FONT}))'
          + (' (hide yes)' if hide else '') + ')')
        a(f'{indent}\t)')

    prop("Reference", prefix, 0, h / 2 + BODY_MARGIN)
    prop("Value", body["display"], 0, -(h / 2 + BODY_MARGIN))
    prop("Footprint", "", 0, 0, True)
    prop("Datasheet", "", 0, 0, True)
    prop("Description", "Reconstructed from PADS part type " + body["display"],
         0, 0, True)

    a(f'{indent}\t(symbol "{body["name"]}_0_1"')
    a(f'{indent}\t\t(rectangle (start {-w / 2:.4f} {h / 2:.4f}) '
      f'(end {w / 2:.4f} {-h / 2:.4f})')
    a(f'{indent}\t\t\t(stroke (width 0.254) (type default))')
    a(f'{indent}\t\t\t(fill (type background))')
    a(f'{indent}\t\t)')
    a(f'{indent}\t)')

    a(f'{indent}\t(symbol "{body["name"]}_1_1"')
    for num, kind, name, x, y, angle in body["pins"]:
        pname = overbar(name)
        a(f'{indent}\t\t(pin passive line')
        a(f'{indent}\t\t\t(at {x:.4f} {y:.4f} {angle})')
        a(f'{indent}\t\t\t(length {PIN_LEN})')
        a(f'{indent}\t\t\t(name "{esc(pname)}" '
          f'(effects (font (size {FONT} {FONT}))))')
        a(f'{indent}\t\t\t(number "{esc(num)}" '
          f'(effects (font (size {FONT} {FONT}))))')
        a(f'{indent}\t\t)')
    a(f'{indent}\t)')
    if external:
        a(f'{indent}\t(embedded_fonts no)')
    a(f'{indent})')
    return out


def symbol_instance(part, body, lib_id, value, footprint, root_uuid, sheet_uuid):
    ox, oy = part["x"], part["y"]
    w, h = body["w"], body["h"]
    out = []
    a = out.append
    a("\t(symbol")
    a(f'\t\t(lib_id "{lib_id}")')
    a(f'\t\t(at {ox:.4f} {oy:.4f} 0)')
    a("\t\t(unit 1)")
    a("\t\t(exclude_from_sim no)")
    a("\t\t(in_bom yes)")
    a("\t\t(on_board yes)")
    a("\t\t(dnp no)")
    a(f'\t\t(uuid "{uid("sym", part["ref"])}")')

    def prop(name, text, x, y, hide=False):
        a(f'\t\t(property "{name}" "{esc(text)}"')
        a(f'\t\t\t(at {x:.4f} {y:.4f} 0)')
        if hide:
            a("\t\t\t(hide yes)")
        a(f'\t\t\t(effects (font (size {FONT} {FONT})))')
        a("\t\t)")

    prop("Reference", part["ref"], ox, oy - h / 2 - BODY_MARGIN)
    prop("Value", value, ox, oy + h / 2 + BODY_MARGIN)
    prop("Footprint", footprint, ox, oy, True)
    prop("Datasheet", "", ox, oy, True)
    prop("Description", "Reconstructed from PADS part type " + value, ox, oy, True)

    for num, _kind, _name, _x, _y, _angle in body["pins"]:
        a(f'\t\t(pin "{esc(num)}"')
        a(f'\t\t\t(uuid "{uid("pin", part["ref"], num)}")')
        a("\t\t)")
    a("\t\t(instances")
    a(f'\t\t\t(project "{PROJECT}"')
    a(f'\t\t\t\t(path "/{root_uuid}/{sheet_uuid}"')
    a(f'\t\t\t\t\t(reference "{esc(part["ref"])}")')
    a("\t\t\t\t\t(unit 1)")
    a("\t\t\t\t)")
    a("\t\t\t)")
    a("\t\t)")
    a("\t)")
    return out


def text_block(text, x, y, size=1.5, uuid_key=None):
    out = []
    a = out.append
    a(f'\t(text "{esc(text)}"')
    a("\t\t(exclude_from_sim no)")
    a(f"\t\t(at {x:.4f} {y:.4f} 0)")
    a(f"\t\t(effects (font (size {size} {size})) (justify left top))")
    a(f'\t\t(uuid "{uid("text", uuid_key or text)}")')
    a("\t)")
    return out


def global_label(net, x, y, angle):
    justify = "left" if angle == 0 else "right"
    out = []
    a = out.append
    a(f'\t(global_label "{esc(overbar(net))}"')
    a("\t\t(shape passive)")
    a(f"\t\t(at {x:.4f} {y:.4f} {angle})")
    a("\t\t(fields_autoplaced yes)")
    a(f"\t\t(effects (font (size {LBL_FONT} {LBL_FONT})) (justify {justify}))")
    a(f'\t\t(uuid "{uid("lbl", net, str(round(x, 3)), str(round(y, 3)))}")')
    a('\t\t(property "Intersheetrefs" "${INTERSHEET_REFS}"')
    a(f"\t\t\t(at {x:.4f} {y:.4f} 0)")
    a(f'\t\t\t(effects (font (size {FONT} {FONT})) (justify {justify}) (hide yes))')
    a("\t\t)")
    a("\t)")
    return out


def no_connect(x, y, key):
    return ["\t(no_connect", f"\t\t(at {x:.4f} {y:.4f})",
            f'\t\t(uuid "{uid("nc", key)}")', "\t)"]


def resolve_placement(hits, refs, page_of):
    """One (page, x, y) per ref; ambiguous refs go to a bottom strip."""
    placed, ambiguous = {}, []
    for ref in refs:
        found = hits.get(ref, [])
        if len(found) == 1:
            pno, x, y = found[0]
            placed[ref] = (pno, x, y)
        else:
            pno = page_of.get(ref, 2)
            ambiguous.append(ref)
            placed[ref] = (pno, None, None)
    return placed, ambiguous


def relax(items, page_w, page_h, top):
    """Push overlapping pin envelopes apart, keeping them on the page."""
    for _ in range(60):
        moved = False
        for i in range(len(items)):
            a = items[i]
            for j in range(i + 1, len(items)):
                b = items[j]
                ax0 = a["ox"] - a["w"] / 2 - PIN_LEN - 0.5
                ax1 = a["ox"] + a["w"] / 2 + PIN_LEN + 0.5
                ay0, ay1 = a["oy"] - a["h"] / 2, a["oy"] + a["h"] / 2
                bx0 = b["ox"] - b["w"] / 2 - PIN_LEN - 0.5
                bx1 = b["ox"] + b["w"] / 2 + PIN_LEN + 0.5
                by0, by1 = b["oy"] - b["h"] / 2, b["oy"] + b["h"] / 2
                dx = min(ax1, bx1) - max(ax0, bx0) + 1.5
                dy = min(ay1, by1) - max(ay0, by0) + 1.5
                if dx > 0 and dy > 0:
                    if dx < dy:
                        push = dx / 2 + 0.01
                        if a["ox"] <= b["ox"]:
                            a["ox"] -= push
                            b["ox"] += push
                        else:
                            a["ox"] += push
                            b["ox"] -= push
                    else:
                        push = dy / 2 + 0.01
                        if a["oy"] <= b["oy"]:
                            a["oy"] -= push
                            b["oy"] += push
                        else:
                            a["oy"] += push
                            b["oy"] -= push
                    moved = True
        for it in items:
            it["ox"] = min(max(it["ox"], it["left"] + 1), page_w - it["right"] - 1)
            it["oy"] = min(max(it["oy"], top + it["h"] / 2 + 1),
                           page_h - it["h"] / 2 - 1)
        if not moved:
            break
    overlap = 0
    for i in range(len(items)):
        for j in range(i + 1, len(items)):
            a, b = items[i], items[j]
            if (abs(a["ox"] - b["ox"]) < (a["w"] + b["w"]) / 2 + PIN_LEN + 0.5 and
                    abs(a["oy"] - b["oy"]) < (a["h"] + b["h"]) / 2):
                overlap += 1
    return overlap


def build(asc, pdf, pcb, out_dir, name):
    type_pins, parts, nets, clusters, total_lines = parse_asc(asc)

    for ptype, pins in type_pins.items():
        seen = [p[0] for p in pins]
        if len(set(seen)) != len(seen):
            raise SystemExit(f"{ptype}: duplicate pin numbers")

    pin_to_net = {}
    multi = []
    for net, pins in nets.items():
        for ref, pin in pins:
            if (ref, pin) in pin_to_net:
                multi.append((ref, pin, pin_to_net[(ref, pin)], net))
            pin_to_net[(ref, pin)] = net
    if multi:
        raise SystemExit(f"pin on more than one net: {multi[:5]}")

    used_names = {}
    sym_name = {}
    display = {}
    for ptype in sorted(type_pins):
        sym_name[ptype] = safe_name(ptype, used_names)
        display[ptype] = ptype

    hits = read_pdf_positions(pdf, set(parts))
    footprints = read_footprints(pcb)

    page_of = {}
    for ref in parts:
        found = hits.get(ref, [])
        if found:
            page_of[ref] = found[0][0]

    placed, ambiguous = resolve_placement(hits, set(parts), page_of)

    bodies = {}
    for ptype, pins in type_pins.items():
        body = geometry(pins)
        body["name"] = sym_name[ptype]
        body["display"] = ptype
        bodies[ptype] = body

    root_uuid = uid("root")
    pages = sorted({p for p, _x, _y in placed.values()})
    sheets = {p: uid("sheet", str(p)) for p in pages}

    os.makedirs(out_dir, exist_ok=True)

    # ---- external symbol library ----
    lib = ["(kicad_symbol_lib", "\t(version 20251024)",
           '\t(generator "kicad_symbol_editor")',
           '\t(generator_version "10.0")']
    for ptype in sorted(type_pins):
        lib += sym_lines(sym_name[ptype], bodies[ptype],
                         prefix_for(ptype, parts), indent="\t", external=True)
    lib.append(")")
    with open(os.path.join(out_dir, name + ".kicad_sym"), "w",
              encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(lib) + "\n")
    with open(os.path.join(out_dir, "sym-lib-table"), "w",
              encoding="utf-8", newline="\n") as fh:
        fh.write("(sym_lib_table\n  (version 7)\n")
        fh.write(f'  (lib (name "{LIB}")(type "KiCad")'
                 f'(uri "${{KIPRJMOD}}/{name}.kicad_sym")(options "")'
                 f'(descr "PSDR 2.6.1 reconstructed symbols"))\n')
        fh.write(")\n")

    # ---- child sheets ----
    stats = []
    for pno in pages:
        page_refs = sorted(r for r, v in placed.items() if v[0] == pno)
        items = []
        for ref in page_refs:
            info = parts[ref]
            body = bodies[info["type"]]
            if placed[ref][1] is None:
                continue
            _p, xp, yp = placed[ref]
            ox = xp * 0.3527777778
            oy = yp * 0.3527777778 + 1.0 + body["h"] / 2
            left = body["w"] / 2 + PIN_LEN + STUB
            right = left
            items.append({"ref": ref, "type": info["type"], "body": body,
                          "ox": ox, "oy": oy, "w": body["w"], "h": body["h"],
                          "left": left, "right": right})
        strip = [r for r in ambiguous if placed[r][0] == pno]
        strip_y = PAGE_H - 8
        for k, ref in enumerate(sorted(strip)):
            body = bodies[parts[ref]["type"]]
            items.append({"ref": ref, "type": parts[ref]["type"], "body": body,
                          "ox": 20 + (k % 16) * 32, "oy": strip_y - (k // 16) * 12,
                          "w": body["w"], "h": body["h"],
                          "left": body["w"] / 2 + PIN_LEN + STUB,
                          "right": body["w"] / 2 + PIN_LEN + STUB})

        # estimate label room using the real nets on this page
        for it in items:
            names = []
            for num, _k, _n, _x, _y, _ang in it["body"]["pins"]:
                net = pin_to_net.get((it["ref"], num))
                if net:
                    names.append(overbar(net))
            it["left"] = max([it["left"]] + [label_width(n) + 8
                                             for n in names]) if names else it["left"]
            it["right"] = it["left"]

        overlap = relax(items, PAGE_W, PAGE_H, TOP_RESERVE)
        for it in items:
            it["ox"] = round(it["ox"] / 1.27) * 1.27
            it["oy"] = round(it["oy"] / 1.27) * 1.27

        by_ref = {it["ref"]: it for it in items}
        used_ptypes = sorted({it["type"] for it in items})
        page_title = f"PSDR 2.6.1 - PDF page {pno}: {PAGE_NAMES.get(pno, '')}"
        L = ["(kicad_sch", "\t(version 20250610)", '\t(generator "eeschema")',
             '\t(generator_version "10.0")', f'\t(uuid "{sheets[pno]}")',
             f'\t(paper "User" {PAGE_W:.4f} {PAGE_H:.4f})', "\t(title_block",
             f'\t\t(title "{esc(page_title)}")', '\t\t(date "2026-10-31")',
             '\t\t(rev "generated")',
             '\t\t(comment 1 "Netlist: Hardware/PSDR.asc (PADS Layout ASCII, upstream 468dc41)")',
             f'\t\t(comment 2 "Placement: Hardware/PSDR_2.6.1_Schematic_2016-07-31.pdf page {pno}")',
             "\t\t(comment 3 \"Connectivity is carried by global labels; original wiring is in the PDF\")",
             "\t)", "\t(lib_symbols"]
        for ptype in used_ptypes:
            L += sym_lines(LIB + ":" + sym_name[ptype], bodies[ptype],
                           prefix_for(ptype, parts))
        L.append("\t)")
        L += text_block(
            f"Generated from PADS ASCII. Net names are labels, not drawn wires. "
            f"Original drawing: PSDR_2.6.1_Schematic_2016-07-31.pdf page {pno}.",
            8, 6, size=1.4, uuid_key=f"note{pno}")

        for ref in sorted(by_ref):
            it = by_ref[ref]
            info = parts[ref]
            part = {"ref": ref, "x": it["ox"], "y": it["oy"]}
            L += symbol_instance(part, it["body"],
                                 LIB + ":" + sym_name[info["type"]],
                                 info["type"], footprints.get(ref, ""),
                                 root_uuid, sheets[pno])

        for ref in sorted(by_ref):
            it = by_ref[ref]
            for num, _kind, _name, px, py, angle in it["body"]["pins"]:
                ax = it["ox"] + px
                ay = it["oy"] - py
                net = pin_to_net.get((ref, num))
                if net is None:
                    L += no_connect(ax, ay, f"{ref}.{num}")
                    continue
                langle = 180 if angle == 0 else 0
                L += global_label(net, ax, ay, langle)

        L.append(")")
        path = os.path.join(out_dir, f"{name}-p{pno:02d}.kicad_sch")
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write("\n".join(L) + "\n")
        stats.append((pno, len(items), len({it["type"] for it in items}),
                      overlap, path))

    # ---- root sheet ----
    R = ["(kicad_sch", "\t(version 20250610)", '\t(generator "eeschema")',
         '\t(generator_version "10.0")', f'\t(uuid "{root_uuid}")',
         '\t(paper "A2")', "\t(title_block",
         '\t\t(title "PSDR 2.6.1 - reconstructed KiCad schematic")',
         '\t\t(date "2026-10-31")', '\t\t(rev "generated")',
         '\t\t(comment 1 "Source: PADS Layout ASCII Hardware/PSDR.asc at upstream 468dc41")',
         '\t\t(comment 2 "Drawing reference: PSDR_2.6.1_Schematic_2016-07-31.pdf, 2016-07-31")',
         "\t)", "\t(lib_symbols)"]
    R += text_block(
        "PSDR 2.6.1 reconstructed schematic.\n"
        "Each sheet mirrors one page of the 2016-07-31 PDF; pins connect "
        "through global labels carrying PADS net names.\n"
        "This is a connectivity model, not a redrawn artwork.", 31.5, 14,
        size=2.0, uuid_key="rootnote")
    # A2 drawing frame is 594x420; the title block occupies the bottom-right
    # corner, so the 3x3 grid stops above y=316.
    cols = [31.5, 214.5, 397.5]
    rows = [40.0, 138.0, 236.0]
    SW, SH = 165.0, 80.0
    for k, pno in enumerate(pages):
        x = cols[k % 3]
        y = rows[k // 3]
        nm = f"p{pno:02d} {PAGE_NAMES.get(pno, '')}"
        R += ["\t(sheet", f"\t\t(at {x} {y})", f"\t\t(size {SW} {SH})",
              "\t\t(exclude_from_sim no)", "\t\t(in_bom yes)",
              "\t\t(on_board yes)", "\t\t(dnp no)",
              "\t\t(stroke (width 0) (type solid))",
              "\t\t(fill (color 0 0 0 0.0000))",
              f'\t\t(uuid "{sheets[pno]}")',
              f'\t\t(property "Sheetname" "{esc(nm)}"',
              f"\t\t\t(at {x} {y - 0.7} 0)",
              "\t\t\t(effects (font (size 1.5 1.5)) (justify left bottom))",
              "\t\t)", f'\t\t(property "Sheetfile" "{name}-p{pno:02d}.kicad_sch"',
              f"\t\t\t(at {x} {y + SH + 0.7} 0)",
              "\t\t\t(effects (font (size 1.5 1.5)) (justify left top))",
              "\t\t)", "\t\t(instances", f'\t\t\t(project "{PROJECT}"',
              f'\t\t\t\t(path "/{root_uuid}"', f"\t\t\t\t\t(page \"{pno}\")",
              "\t\t\t\t)", "\t\t\t)", "\t\t)", "\t)"]
    R += ["\t(sheet_instances", '\t\t(path "/"', '\t\t\t(page "1")',
          "\t\t)", "\t)", ")"]
    with open(os.path.join(out_dir, name + ".kicad_sch"), "w",
              encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(R) + "\n")

    # ---- project file ----
    pro = {
        "meta": {"filename": name + ".kicad_pro", "version": 3},
        "sheets": [[root_uuid, "Root"]] +
                  [[sheets[p], f"p{p:02d} {PAGE_NAMES.get(p, '')}"] for p in pages],
        "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []},
        "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []},
        "text_variables": {},
    }
    with open(os.path.join(out_dir, name + ".kicad_pro"), "w",
              encoding="utf-8", newline="\n") as fh:
        json.dump(pro, fh, indent=2, ensure_ascii=False)
        fh.write("\n")

    print(f"parts            {len(parts)}")
    print(f"part types       {len(type_pins)}")
    print(f"nets             {len(nets)}")
    print(f"connected pins   {len(pin_to_net)}")
    print(f"pdf pages used   {pages}")
    print(f"ambiguous refs   {len(ambiguous)} {sorted(ambiguous)}")
    print(f"unplaced refs    {sorted(set(parts) - set(placed))}")
    print("page  parts  types  body-overlaps  file")
    for pno, count, types, overlap, path in stats:
        print(f"{pno:>4}  {count:>5}  {types:>5}  {overlap:>12}  {os.path.basename(path)}")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--asc", required=True)
    ap.add_argument("--pdf", required=True)
    ap.add_argument("--pcb")
    ap.add_argument("--out", required=True)
    ap.add_argument("--name", default=PROJECT)
    args = ap.parse_args()
    return build(args.asc, args.pdf, args.pcb, args.out, args.name)


if __name__ == "__main__":
    sys.exit(main())
