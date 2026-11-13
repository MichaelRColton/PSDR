PSDR
====

PortableSDR - a stand-alone, pocket-sized HF software defined transceiver.
[Hackaday.io project page](https://hackaday.io/project/1538-portablesdr)

The PortableSDR is an open-source QRP (low-power) radio for ham operators, hikers, backpackers and DIY enthusiasts. It is a self-contained HF/shortwave SDR with its own display, battery, speaker and controls, and it carries the hardware for a vector network analyzer / antenna analyzer and GPS. It is meant to be a flexible platform for development, a learning aid, and a useful instrument.

## What's new (November 2026)

The firmware is being actively worked on again. `master` now holds the current STM32F746 firmware for the 2.6.1 board, with:

- **Working HMC1023 low-pass filter / RF preamp path**, with an on-device **LPF** setting: Bypass, 0 dB or +20 dB. *(Needs a small hardware modification: one wire to give U287 pin 28, `VDDBG`, its 5 V. Without it, leave the setting on Bypass, which is the default.)*
- **AM reception that works**, on the built-in speaker and over USB.
- **Wideband FM (broadcast band) reception**, mono. Works on strong stations.
- **Narrow FM reception** (2 m and other two-way radio) with automatic gain and a **squelch** setting shown as a marker on the S-meter. Clean on a signal generator (about 29 dB SINAD at -20 dBm); sensitivity is still poor (about -90 dBm usable).
- **Tunes from 2.5 MHz to 175 MHz** on receive and transmit (the 2 m band included).
- **Transmit works and has been measured**: SSB with a 5-step power menu calibrated per unit and shown in dBm and mW, sideband trim (opposite sideband about -55 dBc), and an optional path through the HMC1023 filter that gives about 0 dBm (1 mW) with harmonics below -45 dBc. See the transmit notes below before using the HMC482 amplifier.
- **CW**: iambic paddle keyer and a text-to-Morse sender from the console.
- **Frequency calibration** against a reference, saved in the radio (console `J`).
- **Waterfall** with a fixed, adjustable scroll rate and peak hold, and a wide (about ±112 kHz) view in wideband FM. It can also be **streamed to the PC control panel**.
- First firmware for the **GPS** (NMEA received and parsed), the **MicroSD** slot (card identified, sectors read) and the **antenna analyzer** (AD8302 readings through the bridge). None of these is a finished feature yet.
- **USB composite device**: plug the radio into a PC and you get
  - a **serial console** for control and diagnostics,
  - **"PSDR Demod"**, a sound card carrying the demodulated audio, and
  - **"PSDR I/Q"**, a stereo sound card carrying raw I/Q (48 kHz) for HDSDR, Quisk and similar.
- **A PC control panel** (`tools/psdr_panel.py`), shown below, including a button that captures the radio's own screen.
- **Real-time clock**: time and date on the display, kept running from the battery.
- **Battery voltage, percentage and charge status** on the display.
- **A real "off"**: low-power sleep that wakes on the knob, and the radio remembers frequency, mode, filter, volume and LPF setting.
- **Faster display**: screen fills go out over DMA (about 22x faster than before).
- Cleaner sampling: all three ADCs are DMA-driven at a true 48 kHz with 15x oversampling.
- About 40% less processor time per audio sample, which fixed dropped audio in wideband FM.
- A command-line build (`build.sh`), so no IDE is needed.

![PSDR control panel with the device-screen viewer open](docs/images/control-panel.png)

*The control panel talking to a radio over USB. The "PSDR screen" window is a live capture of the radio's LCD, taken with the panel's Screenshot button.*

## Control panel

```
pip install pyserial
python tools/psdr_panel.py
```

It finds the radio's serial port automatically and gives you a large tuning readout, the receive signal chain as a diagram, mode and filter controls, level meters, "Listen on PC speakers" (needs ffmpeg/ffplay), "Set from PC time" for the clock, a **Waterfall** window (a scrolling copy of the radio's waterfall with frequency labels and a speed slider), filter-path and squelch controls, and **Screenshot**, which reads the radio's LCD back and shows it in a window you can save as a PNG. `tools/shot.py` does the same capture from the command line.

## What works today

| Area | Status |
|---|---|
| Receive, LSB / USB / AM / wideband FM | Working |
| Receive, narrow FM with squelch | Working on a signal generator; not yet proven on the air |
| Waterfall display, S-meter, adjustable filter | Working |
| USB serial console, demod audio, I/Q audio | Working |
| HMC1023 filter / preamp path | Working with the `VDDBG` wire; Bypass otherwise |
| Clock, battery and charge display, sleep with saved state | Working |
| Frequency calibration | Working (per unit, console `J`) |
| Transmit, SSB | Working at milliwatt level, measured with a test tone; microphone audio not yet checked |
| Transmit, CW | Keyer and console sender working on the analyzer; not yet tried with real paddles, no sidetone |
| CW narrow filter, decode | Not implemented |
| FM and AM transmit | Not implemented |
| Vector network analyzer / antenna analyzer | First readings only (console `h`): a 50 ohm load reads correctly; no display or calibration yet |
| GPS | NMEA received and parsed (console `N`); not yet used by the radio |
| MicroSD | Card identified and sectors read (console `S`); no file system yet |

### Receive sensitivity

Sensitivity is the weak point of the 2.6.1 hardware. Measured noise-equivalent input in a 2.4 kHz bandwidth is about -85 dBm with the filter on Bypass and about -105 to -110 dBm through the HMC1023 at +20 dB (up to about 70 MHz; a little worse above 100 MHz). The noise floor appears to be set in the baseband stages after the mixer, so only gain ahead of it helps. Bench notes and ideas for the next board revision are in `docs/WISHLIST.md`, `docs/REVIEW.md` and `docs/NEXT-REV-IDEAS.md`.

### Transmit notes

- Output is in the milliwatt range: about -31 to -17 dBm without the amplifier, about +9 to +12 dBm with the HMC482 amplifier, and about 0 dBm through the HMC1023 path.
- **The HMC482 amplifier (U304) is conditionally unstable.** On the unit measured it oscillates near 2.5 GHz at some drive levels and loads, sometimes with no drive at all. The power menu marks the amplifier steps in red. Do not use them on the air until the hardware is fixed.
- Without the HMC1023 path the output is rich in odd harmonics (third about -9 dBc), so it needs an external low-pass filter. The HMC1023 path needs the `VDDBG` wire.

## Hardware (board 2.6.1)

- STM32F746 ARM Cortex-M7
- 320x240 color LCD
- Si5338 clock synthesizer driving quadrature LO dividers
- Direct-conversion receiver and exciter using diode-ring mixers
- HMC1023 programmable low-pass filter / gain stage ahead of the mixers
- Digitally controlled instrumentation amplifiers
- Morse key (the "Giblet" on the bottom right corner of the enclosure) and paddle inputs
- Magnitude and phase measurement chip with impedance bridge (for VNA and antenna analysis)
- Dual SMA connectors, smartphone-style earphone/microphone connector, USB
- GPS, MicroSD slot
- Built-in microphone and speaker
- Internal lithium polymer battery with charger
- Pads for raw I/Q signals, in and out

Design files are in `Hardware/` (PADS originals, BOMs, schematic PDF) and `dist/kicad/` (a KiCad schematic generated from the PADS netlist).

## Building and flashing the firmware

The firmware is in `Source/`. On Windows with STM32CubeIDE installed (for its bundled GNU ARM toolchain):

```
bash build.sh
```

This produces `build/PSDR_F7.elf`, `.hex` and `.bin`. The toolchain path is set at the top of `build.sh`; edit it for your install. Flash over SWD with an ST-Link, for example:

```
STM32_Programmer_CLI -c port=SWD -w build/PSDR_F7.bin 0x08000000 -v -rst
```

Back up your radio's existing firmware first (`STM32_Programmer_CLI -c port=SWD -u 0x08000000 0x100000 backup.bin`) so you can always go back.

## Repository layout

| Path | Contents |
|---|---|
| `Source/` | Firmware (STM32F746) |
| `tools/` | PC control panel, screenshot tool, transmit power and sideband calibration scripts, PADS-to-KiCad scripts |
| `docs/` | Bench notes, design review, wishlist, next-revision ideas |
| `Hardware/` | PADS design files, BOMs, schematic PDF |
| `dist/` | Generated KiCad schematic and netlists |

Older lines of development are kept as tags: `archive/master-F4` (the original STM32F4 firmware), `archive/PSDR_F7`, `archive/PSDR_Rebuild` and others.

## Why did I make it?

I'm an avid backpacker and radio enthusiast. To me there is something exciting about making contact with others from remote locations where cell phones and the internet can't reach. I wanted an advanced radio that was ideally suited to tough environments. That meant it needed to be very compact, integrated, and durable. As an engineer, I wanted an open platform that really takes advantage of the hardware and has the potential to grow and be used for other applications.

There are numerous, excellent radios out there, but none offered the combination I wanted. The best radios are closed source, larger, and expensive, while most open designs are very basic and lacking in flexibility.

## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for ways to help keep this project going. The project is MIT licensed; see [LICENSE.md](LICENSE.md).
