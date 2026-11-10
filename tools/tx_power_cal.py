#!/usr/bin/env python3
"""
PSDR transmit power calibration
===============================
Output power differs from radio to radio and band to band, so the dBm / mW
shown next to "TX" on the display comes from a per-unit calibration table. This
script fills that table: it steps through the TX power settings, keys the
radio with its 1 kHz test tone for a few seconds each, reads the output on a
Tektronix RSA306 spectrum analyzer, and stores each measured value on the radio
(console `y<step> <dBm x10>`).

It also looks at 2-3 GHz while keyed and warns if the amplifier is oscillating
there (seen on one unit at some drive levels; see the comment above
txPowerTable in main.c).

Setup: antenna port -> attenuator -> RSA306. NEVER connect the radio straight to
the analyzer without knowing the level is safe, and never run this into an
antenna: it transmits a carrier.

Run:   python tools/tx_power_cal.py --att 40 --freq 7100000
       python tools/tx_power_cal.py --att 40 --dry-run      (measure, don't store)
Deps:  pyserial, numpy, and the rsa306 Python package from the rsa306-tools repo
       (pass its folder with --rsa-path if it is not importable).

Without an analyzer you can still calibrate by hand: read the power on any
meter and type  y<step> <dBm x10>  on the radio's console, e.g. `y3 37` for
+3.7 dBm at step 3.
"""
import argparse
import sys
import time

import serial
import serial.tools.list_ports

STEPS = 5


def find_port():
    for p in serial.tools.list_ports.comports():
        hwid = (p.hwid or "").upper()
        if "0483" in hwid and "5740" in hwid:
            return p.device
    return None


def cmd(ser, text, wait=0.4):
    ser.write((text + "\r\n").encode())
    time.sleep(wait)
    return ser.read(4000).decode(errors="replace").strip()


def main():
    ap = argparse.ArgumentParser(description="Calibrate the PSDR's TX power readout with an RSA306.")
    ap.add_argument("--att", type=float, required=True, help="attenuation between radio and analyzer, in dB")
    ap.add_argument("--freq", type=float, default=7100000.0, help="frequency to calibrate at, Hz (default 7.1 MHz)")
    ap.add_argument("--port", help="radio's COM port (default: auto-detect)")
    ap.add_argument("--ref", type=float, default=-10.0, help="analyzer reference level, dBm (default -10)")
    ap.add_argument("--rsa-path", help="folder containing the rsa306 package")
    ap.add_argument("--dry-run", action="store_true", help="measure and print, but do not store on the radio")
    args = ap.parse_args()

    if args.rsa_path:
        sys.path.insert(0, args.rsa_path)
    import numpy as np
    from rsa306 import RSA306

    port = args.port or find_port()
    if not port:
        sys.exit("No PSDR found (VID 0483 / PID 5740). Plug it in or pass --port.")
    ser = serial.Serial(port, 115200, timeout=0.5, write_timeout=1.0)
    ser.dtr = True
    time.sleep(0.3)
    ser.reset_input_buffer()

    cmd(ser, "D0")                                   # LSB
    cmd(ser, "F%d" % int(args.freq), 0.6)
    previous = cmd(ser, "w")                         # remember the current step
    results = []
    try:
        with RSA306() as rsa:
            # 2-3 GHz with the radio in receive, as the reference for the oscillation check
            _, baseline, _ = rsa.sweep(2.0e9, 3.0e9, rbw_hz=1e6, ref_level_dbm=args.ref, max_hold=4)
            for step in range(STEPS):
                reply = cmd(ser, "w%d" % step)       # e.g. "txpower=3 drive=2500/100 amp=1"
                amp = 1 if "amp=1" in reply else 0   # the step says whether it uses the amplifier
                cmd(ser, "X3500 1000 %d" % amp, 0.2)  # key 3.5 s with a 1 kHz tone
                time.sleep(0.7)
                iq, fs, _ = rsa.iq_capture(args.freq, bandwidth_hz=40e3, duration_s=0.3, ref_level_dbm=args.ref)
                cmd(ser, "X0", 0.3)
                time.sleep(0.4)
                dbm = 10 * np.log10((10 * np.abs(iq) ** 2).mean()) + args.att
                results.append(dbm)
                cmd(ser, "X3500 1000 %d" % amp, 0.2)  # key again and look for a parasitic oscillation
                time.sleep(0.7)
                f, w, _ = rsa.sweep(2.0e9, 3.0e9, rbw_hz=1e6, ref_level_dbm=args.ref, max_hold=2)
                cmd(ser, "X0", 0.3)
                time.sleep(0.4)
                rise = w - baseline                  # anything that appears only while keyed
                i = int(np.argmax(rise))
                osc = w[i] + args.att
                warn = "   ** %+.1f dBm at %.0f MHz while keyed (amp oscillating?) **" % (osc, f[i] / 1e6) if rise[i] > 15.0 else ""   # 15 dB: peak-detected noise alone wanders by up to ~10 dB
                print("step %d (%s): %+6.1f dBm  (%.4f mW)%s" % (step, "amp" if amp else "no amp", dbm, 10 ** (dbm / 10), warn))
                if not args.dry_run:
                    cmd(ser, "y%d %d" % (step, round(dbm * 10)))
    finally:
        cmd(ser, "X0", 0.3)                          # make sure it is unkeyed
        m = [c for c in previous if c.isdigit()]
        if m:
            cmd(ser, "w" + m[0])
        if not args.dry_run:
            print(cmd(ser, "y"))
        ser.close()


if __name__ == "__main__":
    main()
