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
- **Transmit: first bench numbers (2026-11-09) and what to fix.** Unmodified 2.6.1 unit, 7.1 MHz, test tone, 40 dB attenuator into an RSA-306, console `X<ms> <tone bin> <amp>` and `O<drive>`:
  - Re-measured after the DSP fixes (1000 Hz time-domain tone, LSB, levels at the antenna port):

    | Drive (`O`) | No amp | With HMC482 |
    |---|---|---|
    | 1 | -49.0 dBm | -21.7 dBm |
    | 4 | | -9.7 dBm |
    | 8 | -31.2 dBm | -3.5 dBm |
    | 16 | | +3.6 dBm |
    | 25 | -21.3 dBm | +8.6 dBm |
    | 50 | | +12.0 dBm (saturated) |
    | 80 | -17.8 dBm (compressing) | |
    | 100 | | +5.9 dBm (collapses, overdriven) |

    Amp gain is about 27 dB. Without the amp the chain compresses near -18 dBm. With it, output is reasonably clean up to about +3 dBm (2 mW): opposite sideband about -30 dBc, close-in products below -25 dBc. At +8.6 dBm products are about -20 dBc, and it saturates near +12 dBm (16 mW). The earlier "gain dip at mid drive" did not reproduce after the fixes. LO leakage at the antenna port is about -66 dBm, in receive as well.
  - **No output filter:** odd harmonics are those of a square wave (3rd about -7 to -10 dBc at every level, 2nd rising to about -8 dBc near saturation). A low-pass filter is needed before this goes near an antenna.
  - **DSP block glitch: FIXED 2026-11-09.** The FFT filter multiplied by a raw 0/1 mask, whose impulse response is as long as the whole 256-sample block, so its tail wrapped around into the samples that are kept; the error differs per block, giving a step at every block seam (every 156 samples = 308 Hz). `kernelFromMask()` now turns the mask into a 101-tap Kaiser-windowed kernel that fits the 100-sample overlap. Measured on transmit with a 1000 Hz tone: 308 Hz comb -32 dBc -> -64 dBc, envelope ripple 4.6 dB -> 0.8 dB. Filter edges are a little softer (about -5 dB at 300 Hz with the edge at 0). Receive uses the same filter, so the faint 308 Hz buzz there should be gone too - not yet checked by ear.
  - **Transmit sideband was inverted: FIXED.** LSB mode transmitted above the carrier. I and Q are now swapped on transmit; LSB measures 986 Hz below and USB 1014 Hz above for a 1000 Hz tone.
  - **Opposite sideband is about -30 dBc** with a 1000 Hz tone. That is the analog I/Q balance of the transmit chain, not the filter; a transmit I/Q calibration (gain/phase trim in the DSP) would improve it.
  - **Transmit audio response tilts upward:** with the filter wide open, 937 Hz came out 3 dB below 1500 Hz and 5 dB below 2500 Hz. Probably AC coupling in the DAC-to-mixer path; measure and equalise.
  - The console test tone is now given in Hz and generated in the time domain, so it exercises the real microphone path: `X<ms> <tone Hz> <amp>`.
  - Still to do: microphone SSB (only the test tone was measured), a sensible default drive and an ALC, drive calibration per band, a transmit indicator on screen, CW keying with sidetone, USB audio/I-Q *input* for digital modes (add playback endpoints to the two existing sound cards rather than new devices).
- **Band overlays on the waterfall (idea, 2026-11-09).** Like the white bars that show the part of the waterfall being listened to, draw bars in other colours marking the ham bands and shortwave broadcast bands as you tune through them. Read the band list from a text file on the MicroSD so people can edit what is shown. Popular stations could carry a short note that appears when you are tuned near them. Needs the SD card working first; a built-in default table would do until then.
- **Frequency memories (idea, 2026-11-09).** Save and recall frequency + mode + filter. Michael rarely operates, so features like this (and multiple VFOs) were never on his list, but people who use the radio to communicate will expect them. Store in flash (or on the SD card) rather than the battery-backed registers. Low priority.
- **Beacon / automatic modes:** scheduled CW idents, band-hop beaconing.
- **Low-power field digital modes:** WSPR, FT8/FT4, JS8 — slow weak-signal modes ideal for backpacking. These need an accurate, stable TX frequency → pairs directly with GPS discipline.
- **RX DSP quality:** AGC, noise blanker/reduction, notch, better filtering.
- **AM channel filter + AM S-meter: DONE 2026-11-09.** I and Q pass through a 6th-order low-pass (corner = the filter's upper edge) before the envelope detector, so AM now only hears what is inside the filter bar, and the S-meter in AM reads the power in that channel. Bench check with a -60 dBm carrier: flat inside the bar, -3 dB at the edge, into the noise floor by about 1.5x the edge, same on both sides; the meter reads within about 1 dB of LSB.
- **AM: tone that rises as you tune away from the carrier (to-do).** Michael hears the sound change, and a pitch that climbs, as he tunes off the carrier even while it is still inside the passband. Envelope detection should not care. Suspects, not yet measured: the I/Q image of the carrier (about 40 dB down, on the other side of zero) beating with the carrier at twice the tuning offset, and any leftover DC beating at the offset itself. Measure the demod audio spectrum versus offset over USB. Fixes to try: the existing I/Q balance correction in AM, or a carrier-tracking (synchronous) detector.
- **Frequency calibration: DONE 2026-11-09.** Console `J<ppb>` sets and saves the reference error (parts per billion); `J` alone reports it. Measured one unit against a 10 MHz GPSDO: zero-beat at 10.000757 MHz on the dial, so `J-75700`; after that it zero-beats at 10.000000 MHz within a few Hz. (The bench Wavetek turned out to be the one that was 4.5 kHz off.) Each unit needs its own value. Still to do: a calibrate button in the control panel, storing the value in flash instead of the battery-backed registers, and eventually disciplining it from GPS 1PPS.
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
