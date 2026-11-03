# PSDR 2.6.1 KiCad schematic

KiCad 10 schematic for the PSDR 2.6.1 main board (STM32F746/F429 HF SDR,
0-35 MHz). It is generated from the 2.6.1 PADS Layout ASCII netlist and laid
out one sheet per page of the 2016-07-31 schematic PDF.

## Why this exists

`master` ships the PADS Logic/Layout binaries (`Hardware/PSDR.sch`,
`Hardware/PSDR.pcb`) and an 8-page 2014 schematic PDF. The 2.6.1 ASCII
(`Hardware/PSDR.asc`) and the 10-page 2016 PDF are only on `PSDR_Rebuild` /
`PSDR_F7` / `PSDR_F7_DMA` at `468dc41`. No KiCad-native schematic existed, so
the design could not be opened, searched or checked without PADS.

## What it is, and what it is not

Every pin gets a global label carrying its PADS net name, placed exactly at the
pin endpoint. Pins with no net get a no-connect flag. There are **no wires**:
the connectivity is carried entirely by labels.

This is a **connectivity model, not redrawn artwork**. Trace shapes, symbol
graphics and the designer's margin notes live only in
`Hardware/PSDR_2.6.1_Schematic_2016-07-31.pdf` (page N here corresponds to PDF
page N). It is meant to be read next to that PDF, and to make the netlist
machine-checkable.

## Contents

| File | Contents |
|---|---|
| `PSDR_2.6.1.kicad_sch` | Root sheet, A2, nine hierarchical sheets |
| `PSDR_2.6.1-p02..p10.kicad_sch` | One sheet per PDF page, 391 parts total |
| `PSDR_2.6.1.kicad_sym` | 57 reconstructed symbols (one per PADS part type) |
| `PSDR_2.6.1.kicad_pro`, `sym-lib-table` | Project and symbol library wiring |
| `../netlist/PSDR_2.6.1_netlist.csv` | `net,refdes,pin,pin_name,part_type`, 1132 rows |
| `../netlist/PSDR_2.6.1_netlist.json` | Same data plus the 41 unconnected pins |

Page map: 2 MCU/GPS/USB, 3 clock (Si5338), 4 analog switches/drivers,
5 RX/TX baseband, 6 audio/microphone, 7 UI paddles/LEDs/LCD, 8 power,
9 RF front end, 10 variable filter. PDF page 1 carries notes only, so there is
no sheet for it.

## Regenerating

Requires Python 3 (standard library only) and KiCad 10 for the checks.

```
python tools/pads_asc_to_kicad.py --asc Hardware/PSDR.asc \
    --pdf Hardware/PSDR_2.6.1_Schematic_2016-07-31.pdf \
    --out dist/kicad
```

UUIDs are derived with `uuid5(NAMESPACE_URL, "psdr261/...")`, so the same input
produces byte-identical output; a diff after regeneration is a real change.
The generator writes LF line endings, and `.gitattributes` pins the KiCad files
to LF so a Windows checkout with `core.autocrlf=true` does not turn every
regenerated file into a whole-file diff.

`--pcb FILE` optionally copies the footprint field from a `.kicad_pcb` (the
PADS decal name, e.g. `PSDR_PADS:CAP_0603`). Without it that one property is
empty; nothing else changes (391 lines out of ~104000). The board itself is
not part of this change: `.emn`/`.emp` carry only outline, holes and placement,
so the reconstructed board has no pads for 366 of 391 parts and no routing.
Note that with the footprint field empty ERC reports no warnings at all, since
`footprint_link_issues` is the only check that fires.

## Verification

```
python tools/pads_asc_netlist.py Hardware/PSDR.asc \
    --csv dist/netlist/PSDR_2.6.1_netlist.csv \
    --json dist/netlist/PSDR_2.6.1_netlist.json

kicad-cli sch export netlist --format kicadxml -o out.xml dist/kicad/PSDR_2.6.1.kicad_sch
python tools/kicad_netlist_compare.py --pads dist/netlist/PSDR_2.6.1_netlist.json --kicad out.xml

python tools/kicad_sch_geomcheck.py dist/kicad/PSDR_2.6.1.kicad_sch
kicad-cli sch erc --severity-all dist/kicad/PSDR_2.6.1.kicad_sch
```

Results:

- Netlist: 278 of 278 PADS nets have identical pin sets, 1132 of 1132 connected
  pins are covered, 0 nets only on one side, 0 pins in more than one net,
  391 components. The comparison is a partition check, not a name check, so a
  short between two nets fails it.
- Geometry: no two connection points coincide in any of the 10 files (1173
  points = 1132 labels + 41 no-connects). This is checked separately because
  KiCad silently merges coincident pins and the netlist export would not show it.
- ERC: 0 errors, 391 warnings, all `footprint_link_issues` because the
  `PSDR_PADS` footprint library is deliberately not shipped.
- Names: 276 of 278 match character for character. The other two differ only by
  the PADS overbar escape: `AUDIO_AMP_\SHDN` -> `AUDIO_AMP_~{SHDN}`,
  `\EARPHONE_INSERTED` -> `~{EARPHONE_INSERTED}`.

The exported XML netlist differs between runs only in its `<date>` element.

## Known limits

- Eleven one-pin test pads have ambiguous refdes text in the PDF (`+`, `-`,
  `1PPS`, `CLK`, `DAC1`, `DAC2`, `GND`, `LIPO`, `NRST`, `SCL`, `SDA`). They are
  placed in a strip at the bottom of their page. Connectivity is unaffected: the
  netlist is keyed by refdes, not by position.
- No hierarchical sheet pins. Global labels already connect across sheets.
- No footprint library is shipped, hence the 391 ERC warnings.

## Provenance

- `Hardware/PSDR.asc`: PSDR 2.6.1 PADS Layout ASCII, upstream `468dc41`
  (`Hardware/PSDR.asc`).
- `Hardware/PSDR_2.6.1_Schematic_2016-07-31.pdf`: upstream `468dc41`
  (`Hardware/PSDR Schematic.pdf`), 10 pages, `CreationDate` 2016-07-26.
- Netlist extraction and the sheet layout follow the rules in
  `tools/pads_asc_netlist.py` and `tools/pads_asc_to_kicad.py`.
