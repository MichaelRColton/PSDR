# PSDR — Wishlist & Roadmap

_Captured 2026-11-02 on branch `fix/hmc1023-lpf`. A living list — edit freely. Nothing below is committed to beyond the "Now" section._

**The vision:** make the PSDR a genuinely functional, pocketable two-way HF radio. Keep what the PSDR already does well — big color LCD, Cortex-M7 DSP headroom, waterfall, ergonomics, case, VNA/antenna analyzer, GPS — and fold in what the uSDX / (t)rueSDX does well: an efficient switching PA and real output filtering.

## Now — active work (this branch)
- **Boot fixes (issue #19):** remove the dead debug `while(1)` before the main loop (`main.c:792`); bound the Si5338 I²C wait so a missing/stuck synth shows a visible fault instead of a silent hang.
- **HMC1023 LPF/VGA bring-up:** blue-wire VDDBG→5 V (done on the bench); firmware to enable `LPF_POWER` (PE11), SPI-configure the chip, and flip the mux (PE10/PD8) from bypass into the filter path. Adds gain + selectivity.
- **Chase the real deafness:** the mux parks on **bypass**, so the dead filter is NOT the cause — suspect the QSD/mixer front end (the mux→mixer change) and/or the RF amp (HMC482 / `AMP_POWER`) path. Hardware review to follow.

## Firmware-only (works on the current hardware)
- **GPS bring-up (U185 RXM-GPS):** NMEA time/position; then **discipline the reference** — use GPS 1PPS to steady the Si5338 (a GPSDO-lite). This underpins accurate TX frequency for the digital modes below.
- **SD card:** record raw IQ samples and/or decoded audio; band-scope captures and logs. (F746 SDMMC — watch sustained write rate for raw IQ; decimate/compress as needed.)
- **Smart CW (Morse) decoder:** Goertzel / narrow band-pass + adaptive threshold + WPM tracking; decoded text under the waterfall.
- **Beacon / automatic modes:** scheduled CW idents, band-hop beaconing.
- **Low-power field digital modes:** WSPR, FT8/FT4, JS8 — slow weak-signal modes ideal for backpacking. These need an accurate, stable TX frequency → pairs directly with GPS discipline.
- **RX DSP quality:** AGC, noise blanker/reduction, notch, better filtering.
- **AM channel filter + AM S-meter: DONE 2026-11-09.** I and Q pass through a 6th-order low-pass (corner = the filter's upper edge) before the envelope detector, so AM now only hears what is inside the filter bar, and the S-meter in AM reads the power in that channel. Bench check with a -60 dBm carrier: flat inside the bar, -3 dB at the edge, into the noise floor by about 1.5x the edge, same on both sides; the meter reads within about 1 dB of LSB.
- **AM: tone that rises as you tune away from the carrier (to-do).** Michael hears the sound change, and a pitch that climbs, as he tunes off the carrier even while it is still inside the passband. Envelope detection should not care. Suspects, not yet measured: the I/Q image of the carrier (about 40 dB down, on the other side of zero) beating with the carrier at twice the tuning offset, and any leftover DC beating at the offset itself. Measure the demod audio spectrum versus offset over USB. Fixes to try: the existing I/Q balance correction in AM, or a carrier-tracking (synchronous) detector.
- **Frequency accuracy check (to-do).** With the Wavetek set to 7.000 MHz the radio found the carrier at about 6.9955 MHz on its dial, a 4.5 kHz (about 640 ppm) disagreement. Not yet known which one is off. Check against the 10 MHz GPSDO (through plenty of attenuation), then add a reference-frequency calibration value if the radio is the one that is off.
- **Code cleanup and review (to-do, later).** `main.c` is one very large file with a lot of commented-out history, guessed pin polarities and bench scaffolding. A proper cleanup and review is due, but deliberately NOT now: other people have just started reading this code, so hold off on moving things around until that settles, and do it in small, reviewable steps. A known example of what a review should catch: the overlap-save code wrote one sample before the start of `samplesOverlap` for years (fixed 2026-11-09).
- **Display: faster text.** Fills, lines, bitmaps and the waterfall now go out over DMA (full-screen fill 538 ms -> 24 ms), but text and the address-window setup are still sent a byte at a time, so a full UI redraw only went 307 -> ~222 ms. Batch `setAddrWindow` and render glyphs through the pixel stream.

- **USB audio class (sound-card front end) — bigger project.** Make the USB interface stream RX/TX audio to/from the PC as a standard USB Audio Class device, so the PSDR can front-end existing PC SDR/digital-mode software (WSJT-X, fldigi, etc.). Firmware-only on the F746 (add a UAC interface alongside the CDC console). "Way later" per Michael.

## Needs hardware (next rev / add-on)
- **Efficient PA — the big one:** adopt a uSDX-style switching/Class-E PA + a proper output LPF bank for real, efficient QRP output. The PSDR's weakest area (your own note in #16). Keep the PSDR front-end/UI.
- **Bluetooth:** no BT part on the current board (BOM has GPS, not BT) — a next-rev add (module, or a wireless-class MCU).
- **Front-end sensitivity:** revisit the QSD (mux/Tayloe vs the current mixer) and RF preamp; fix the HMC1023 VDDBG supply (no 5 V on pin 28) and whatever else the HW review turns up.
- **Power sequencing:** battery-threshold UVLO so it won't brown-out on power-up. Can be firmware-staged (boost enable = PE2, POWER_GOOD = PD0) and/or hardened in hardware.

## "Merge the best of uSDX" notes
- **Take from uSDX:** efficient Class-E PA + output filtering; simpler/cheaper RF where it actually helps sensitivity.
- **Keep from PSDR:** M7 DSP, color LCD + waterfall, ergonomics, case, VNA/antenna analyzer, GPS.
