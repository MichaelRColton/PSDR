# Cell-level change list for the two 2.6.1 workbooks

The workbooks are binary `.xls`, so the GitHub diff shows only a byte-count change.
This file lists every cell that differs between `master` (`45acd0f`) and this branch, so the
change can be reviewed without opening Excel. It is generated from a cell-by-cell dump of both
versions (LibreOffice UNO: content, formula, fill colour, font and size per cell).

## `Hardware/BOMs/PSDR_2.6.1/PSDR2.6.1_BOM.xls`

7 cells changed (sheet `PSDR25BOM`):

| cell | before | after |
|---|---|---|
| `B45` | R6,14,268 | R268 |
| `B56` | R97,-99 | R97-99 |
| `G81` | *(empty)* | RF Micro Devices (RFMD) |
| `H81` | *(empty)* | SXB-2089Z |
| `H83` | *(empty)* | MASWSS0129TR |
| `J26` | *(empty)* | DNP by design - the footprints are still on the board, they were never deleted. Both are shunts: C307 = U314.8 to GND, C320 = GND to U314.14. Neither sits in... |
| `J64` | *(empty)* | DNP by design - footprints still on the board. Bridging resistors across the mixer LO pairs: R79 = MIXER_LO1N/MIXER_LO1P (L81.1/L81.6), R72 = MIXER_LO2N/MIXE... |

## `Hardware/BOMs/PSDR_2.6.1/PSDR2.6_BOM.xls`

6 cells changed (sheet `PSDR25BOM`):

| cell | before | after |
|---|---|---|
| `B54` | R97,-99 | R97-99 |
| `I79` | *(empty)* | RF Micro Devices (RFMD) |
| `J79` | *(empty)* | SXB-2089Z |
| `J81` | *(empty)* | MASWSS0129TR |
| `L26` | *(empty)* | DNP by design - the footprints are still on the board, they were never deleted. Both are shunts: C307 = U314.8 to GND, C320 = GND to U314.14. Neither sits in... |
| `L62` | *(empty)* | DNP by design - footprints still on the board. Bridging resistors across the mixer LO pairs: R79 = MIXER_LO1N/MIXER_LO1P (L81.1/L81.6), R72 = MIXER_LO2N/MIXE... |

## Full text of the long note cells

`PSDR2.6.1_BOM.xls` cell `J26`:

    DNP by design - the footprints are still on the board, they were never deleted. Both are shunts: C307 = U314.8 to GND, C320 = GND to U314.14. Neither sits in series with anything, so no 0 ohm jumper is needed.

`PSDR2.6.1_BOM.xls` cell `J64`:

    DNP by design - footprints still on the board. Bridging resistors across the mixer LO pairs: R79 = MIXER_LO1N/MIXER_LO1P (L81.1/L81.6), R72 = MIXER_LO2N/MIXER_LO2P (L41.1/L41.6). Do NOT fit 0 ohm: both nets already reach the LO winding.

`PSDR2.6_BOM.xls` cell `L26`:

    DNP by design - the footprints are still on the board, they were never deleted. Both are shunts: C307 = U314.8 to GND, C320 = GND to U314.14. Neither sits in series with anything, so no 0 ohm jumper is needed.

`PSDR2.6_BOM.xls` cell `L62`:

    DNP by design - footprints still on the board. Bridging resistors across the mixer LO pairs: R79 = MIXER_LO1N/MIXER_LO1P (L81.1/L81.6), R72 = MIXER_LO2N/MIXER_LO2P (L41.1/L41.6). Do NOT fit 0 ohm: both nets already reach the LO winding.

## What did NOT change

- No other cell: the 2.6.1 sheet's non-empty count goes 1144 -> 1146, exactly the two added
  DNP notes; everything else is identical at the cell level.
- Merged cells (0), custom number formats (0) and the single-sheet layout are unchanged.
- All 8 embedded row hyperlinks survive the re-save, including the
  `rfmd/SXB-2089Z/599-1068-1-ND/937042` target the manufacturer number is based on.

## Known side effect

LibreOffice writes `.xls` column widths back in storage units, so every column moves by 1-2
units. Cell text, `Arial 10`, fill colours and hyperlinks are unchanged; that is why the file
size change is larger than the edits.

## Why each change

- `B45` (2.6.1) `R6,14,268` -> `R268`: R6 and R14 already have their own rows.
- `B56` (2.6.1) / `B54` (2.6) `R97,-99` -> `R97-99`: the comma made a one-element list and R98
  disappeared; the range form restores it (R97/R98/R99 are on the schematic).
- the 4 note cells: DNP reasons for issue #14 (shunts or LO-pair bridges, none in series,
  so no 0 ohm jumper; fitting one across the LO pair would short it).
- the 6 manufacturer cells: issue #13, with the evidence strength stated in the PR text
  (U304 from that row's own hyperlink; switches from the sheet's `Type` string only).

## Regenerate

`git show 45acd0f:<path> > up.xls`, `git show <branch>:<path> > pr.xls`, dump every cell of
both with LibreOffice UNO and diff; the procedure is recorded in `docs/WO-psdr-takeover.md`
sections C-7 and G-1.
