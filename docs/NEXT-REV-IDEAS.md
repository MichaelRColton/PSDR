# PSDR — next-rev ideas & backlog

_Running list captured 2026-11-03. Context for most of these is the deafness/sensitivity work and
the RF-architecture trace in `HMC1023-DEBUG-2026-11-03.md`._

## Sensitivity / front end (the deafness — biggest wins)
- **LNA ahead of the mixer.** The single biggest MDS lever. The RX front end is a *passive* ADE-2ASK
  diode-ring mixer with no low-noise gain in front of it, so mixer conversion loss + the first
  baseband amp's noise set a poor NF. A real LNA before the mixer fixes the root cause.
- **Quieter first baseband amp.** The MCP6N11 in-amps (U272/U255) are zero-drift precision parts
  (~35 nV/rtHz) — noisy for a first stage after a lossy mixer. Use a low-noise amp, or put gain ahead.
- **Low-noise 5 V.** The HMC1023's `LPF_5V` comes through Q208 off `+5V_BOOST` (ADP1613), the switcher
  that desensitizes the HF front end. A quiet LDO for the analog 5 V (or better filtering/layout)
  would let the preamp help without the switcher penalty.
- **Measure MDS properly.** Attenuators on order (VNA floors at -40 dBm). Baseline MDS on the bypass
  path, then A/B every change against it. Current estimate ~-100 dBm (~30-40 dB deaf vs a good HF rx).

## HMC1023 (already working on `fix/hmc1023-lpf`)
- It's an **RF preamp + ~72 MHz roofing LPF ahead of the mixer** (not baseband), so its ~20 dB *can*
  help sensitivity. Pending test: **bypass (M1, boost off) vs HMC1023-in (M2, +20 dB, boost on)** MDS —
  if the preamp gain beats the switcher noise, make it the default RX path.
- Gate the Si5338 CLK1A cal clock to on-during-cal-only (runs continuously now; possible front-end spurs).
- Fold the bench scaffolding (g/M/L/H/G/r/R) into clean, labelled controls once validated.

## Architecture: QSD return vs ring mixers
- Ring mixers (ADE-2ASK) were chosen to reach ~150 MHz, at the cost of sensitivity. A return to a
  **quadrature sampling detector (QSD/Tayloe)** would likely recover a lot of HF sensitivity.
- The old blocker was switch **toggle speed** (not bandwidth): the FST3253 (prop delay ~0.25 ns,
  on/off 1-5 ns) was the classic pick; ordinary muxes optimise steady-state bandwidth and switch far
  too slowly. Modern equivalent: TI `SN74CB3Q3253`/`74CBT3253`. No clear faster named successor found —
  worth a deeper parts hunt (VHF-rated bus switches / low charge-injection analog switches).
- **2 m (144-148 MHz) is NOT free.** Firmware now tunes there (console F ceiling lifted to 175 MHz;
  `setFreq` x2 -> 288 MHz Si5338 LO, /2 in the SY10EP52), but RX heard nothing even at a strong level —
  needs a VHF-capable RF front end and a verified VHF LO. Scope the LO at the mixer to disambiguate.

## Direct-sampling pocket RX (fun minimal toy)
- Bare STM32F746: internal ADC ~2.4 MSPS x3, triple-**interleaved** ~7.2 MSPS -> Nyquist ~3.6 MHz.
  So a **DC-~3.6 MHz direct-sampling receiver** (160/80 m) with NO mixer/LO/QSD: antenna -> ~3.5 MHz
  anti-alias LPF -> buffer/gain -> ADC -> **DDC in firmware** (NCO mix + CIC decimate + FIR/demod at a
  low rate; CIC-first keeps full-rate work to adds). M7 @216 MHz + CMSIS-DSP can do it.
- Second/third ADC = **interleave for bandwidth** (direct sampling is a real signal; I/Q needs a mixer
  or a DSP Hilbert, which doesn't add bandwidth). In the QSD/direct-conversion case the 2nd channel is Q.
- The **DAC is only ~1 MSPS** — too slow to synthesise HF directly, so TX still needs up-conversion.

## USB
- **Composite CDC + USB-Audio (UAC)** so the PSDR is a USB sound card (stream demod audio to the PC for
  WSJT-X/fldigi) while keeping the console on CDC. FS bandwidth is plenty; endpoints fit. _Enumerates on
  Windows (COM port + mic), committed on `fix/hmc1023-lpf` (433ecf7). Remaining: tune the isochronous IN
  pacing for clean real-time 48 kHz (iso-IN-incomplete / SOF frame-parity handling), and consider a USB
  audio OUT (speaker) so the PC can feed TX audio too._
- **DFU firmware update over USB.** Michael believes the board is wired so the F746 ROM bootloader can be
  entered (BOOT0 pin) for DFU updates — confirm the BOOT0 strapping/footprint, then document the
  `dfu-util`/DfuSe recipe. Nice-to-have: a console command or button combo that jumps to the ROM DFU
  loader so updates don't need the ST-Link. (Also open: "was there something else I wanted to run over
  USB?" — revisit if it resurfaces.)

## TX
- **HMC482 (U304)** is the only RF gain ("PA"), QRP-level, with an atypical **dual-port DC bias**
  (L11 + L19 both feeding bias) — verify the intended operating point/direction before trusting TX power.
- **No hardware TX I/Q trim** (the MAX5388 pot is RX-only) — correct carrier/image balance in
  firmware/DAC scaling.
- The `74CBTLV3253` bus-switch sits in the RF path on TX too — limited power handling / possible
  distortion source.

## Station / antenna
- Reconnect the HF antenna fully (gives real signals to judge MDS against).
- Add a **common-mode choke** on the feedline (current balun at the feedpoint + ferrite at the shack
  entry). The cable->AT&T-fiber switch already removed a likely HF noise source (cable modem/MoCA RFI)
  and the feedline no longer parallels it.

## Tooling
- `tools/psdr_panel.py` — the RF control-panel GUI (console-driven). Michael has ideas to make it cooler.
