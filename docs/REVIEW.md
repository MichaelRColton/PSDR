# PSDR 2.6.1 — Hardware & Firmware Review

_2026-11-02, branch `fix/hmc1023-lpf`. Findings from the netlist (`Hardware/PSDR.asc` → `tools/pads_asc_netlist.py`), the `PSDR_F7` source, and live bench reads of a unit (STM32F746, SWD)._

## 1. Receive sensitivity — the "deaf as a brick" problem

The receiver currently runs with **no active RF gain stage and no selectivity**, by firmware design. Three independent contributors, most of them firmware-fixable:

1. **RF gain block (U304, HMC482) is OFF and bypassed in receive.** `signalPath()` `case recieve_no_filter` (`Source/src/main.c:823`) sets `AMP_POWER=1` (off), `FIVE_VOLT_REGULATOR_ENABLE=0` (off → no `+5V_AMP`), and the amp relay (U16, `AMP_SWITCH`) to bypass. So the one switchable in-line gain block is powered down and routed around during normal RX. U304 is a general MMIC gain block biased through chokes (L11 in, L19→`+5V_AMP` out) and inserted/bypassed by the DPDT U16 — i.e. it *can* be put in the RX path.
2. **Amp-relay polarity is unverified.** The same `AMP_SWITCH_A=0 / AMP_SWITCH_B=1` is written for both the "bypass" cases and the `transmit_with_amp` "on" case (`main.c:833`, `:868`, `:901`), each tagged **"0 to bypass (I think!)"**. One setting can't be both bypass and insert — the MASWSS0129 truth table needs confirming on the bench (drive V1/V2, watch which through-path conducts).
3. **Baseband LPF/VGA (U287, HMC1023) is bypassed + unpowered.** Filter muxes (U211/U298) park on bypass (`FILTER_S0=1/S1=0`), `LPF_POWER`/PE11 is never driven, and pin 28 `VDDBG` has no 5 V on the board (bench blue-wire added: C292→LPF_5V). The `recieve_with_filter` case (`main.c:855`) is an **empty stub** — it was never implemented.

Also relevant: the original **mux-based QSD** (74CBTLV3253, preserved in Drive `PSDR_2.0.0`) was replaced by the current mixer, which Michael recalls hurt sensitivity. That's a hardware path question for a future rev.

**Prioritized experiments (firmware-first, cheapest → deepest):**
- **A. Add a real receive-with-amp path.** Power U304 (`AMP_POWER=0`, 5V reg on) and set `AMP_SWITCH` to *insert* it (confirm polarity first). This is the most likely large, quick sensitivity win. Add it as the active RX mode (or a toggle).
- **B. Bench-verify the `AMP_SWITCH` and `FILTER_S0/S1` truth tables** so the "(I think!)" comments become facts.
- **C. Bring up the HMC1023** (needs the blue wire + PE11 + SPI config + mux→filter) for selectivity and extra VGA gain. Fill in `recieve_with_filter`.
- **D. If RX is still weak with front-end gain in,** investigate the mixer/QSD (consider the old mux/Tayloe detector) — hardware, next rev.

## 2. Hardware notes / bugs

- **HMC1023 VDDBG (U287 pin 28): no 5 V on the board** — only C292 to ground. Bias supply is absent; the chip can't work until fed. Fix in next rev (tie to LPF_5V); bench blue-wire in place now.
- **No power-up UVLO / brown-out protection.** Power sequencing is entirely MCU-controlled (boost enable = PE2, POWER_GOOD = PD0 (read only), charge-source = PB8). On a low battery the firmware can enable the boost + loads and starve itself. Mitigate in firmware (gate enables on a battery-voltage threshold at boot); optionally harden in hardware.
- **Switchable gain block is single-ended, output-biased through L19 from `+5V_AMP`** (gated by `AMP_POWER`/PE3). Fine, but it means "amp on" depends on both the supply gate *and* the relay — two controls that must agree.
- Front-end is the mixer-QSD rework; the low-loss mux QSD is the fallback design if sensitivity can't be recovered otherwise.

## 3. Firmware notes

- **`signalPath()` modes are incomplete/uncertain:** `recieve_with_filter` is empty; amp-relay polarity is guessed; there's no "receive with amp" mode at all. This is where the sensitivity work lives.
- **No HMC1023 driver** anywhere on `PSDR_F7` (no PE11 drive, no SPI config) — the filter was never brought up. `LPF_POWER` exists only as a macro.
- **Boot is clean on `PSDR_F7`** — the dead `while(1)` and unbounded Si5338 wait that issue #19 flagged are from the DMA-era tree, not here.
- **From-source build runs, but the image bus-faults at boot** (HFSR FORCED + CFSR bus fault; CPU alive in the fault handler). A clean GCC-13 build of the 2018 source doesn't run yet — suspect the multi-RAM-section startup init (`OS_INCLUDE_STARTUP_INIT_MULTIPLE_RAM_SECTIONS`) / linker-symbol match, or a codegen difference vs the original toolchain. **This must be solved before any firmware fix can be flashed.** (A verified backup of a unit's original firmware is saved outside the repo.)
- **Build howto:** `bash build.sh` (CubeIDE GNU-ARM toolchain). Needed for GCC 13: `-DARM_MATH_CM7 -D__FPU_PRESENT=1`, compile `Source/DSP_Lib` + `-lm`, `-fcommon`, exclude `*_template.c`.

## 4. Suggested order of work

1. **Fix the from-source build so it runs** (boot bus-fault) — unblocks everything.
2. Bench-verify `AMP_SWITCH` / `FILTER` relay truth tables.
3. Implement **receive-with-amp** (experiment A) → measure sensitivity (WaveTek sig gen down to ~−130 dBm is ideal for MDS).
4. HMC1023 bring-up (PE11 + SPI + mux→filter) and fill `recieve_with_filter`.
5. Firmware power-up UVLO.
6. Next-rev HW: fix VDDBG supply, revisit QSD (mux vs mixer), add an efficient PA (see `WISHLIST.md`).
