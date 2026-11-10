#!/usr/bin/env python3
"""
PSDR transmit trim calibration (carrier leak and opposite sideband)
===================================================================
Keys the radio with its 1 kHz test tone in LSB and watches the output on a
Tektronix RSA306. Around the wanted tone there are two unwanted lines:
  - the carrier (LO leak), 1 kHz above the tone;
  - the opposite sideband (image), 2 kHz above the tone.
The radio has four trim values for these (console `z`): a DC offset on I and on
Q, which cancels the carrier, and a phase and a gain correction, which cancel
the image. This script searches for the best four values one at a time,
repeating until nothing improves, then stores them on the radio (`zs`).

Setup: antenna port -> attenuator -> RSA306. It transmits a carrier: never run
it into an antenna.

Run:   python tools/tx_trim_cal.py --att 10
       python tools/tx_trim_cal.py --att 10 --dry-run     (search, don't store)
Deps:  pyserial, numpy, and the rsa306 package from the rsa306-tools repo
       (--rsa-path if it is not importable).

The values are per radio, and they drift with frequency: calibrate on the band
you care about most. (Per-band values are a to-do.)
"""
import argparse
import sys
import time

import serial
import serial.tools.list_ports


def find_port():
    for p in serial.tools.list_ports.comports():
        hwid = (p.hwid or "").upper()
        if "0483" in hwid and "5740" in hwid:
            return p.device
    return None


def cmd(ser, text, wait=0.25):
    ser.write((text + "\r\n").encode())
    time.sleep(wait)
    return ser.read(4000).decode(errors="replace").strip()


def main():
    ap = argparse.ArgumentParser(description="Null the PSDR's transmit carrier leak and opposite sideband.")
    ap.add_argument("--att", type=float, required=True, help="attenuation between radio and analyzer, dB")
    ap.add_argument("--freq", type=float, default=7100000.0)
    ap.add_argument("--port")
    ap.add_argument("--ref", type=float, default=10.0, help="analyzer reference level, dBm")
    ap.add_argument("--rsa-path")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    if args.rsa_path:
        sys.path.insert(0, args.rsa_path)
    import numpy as np
    from rsa306 import RSA306

    port = args.port or find_port()
    if not port:
        sys.exit("No PSDR found. Plug it in or pass --port.")
    ser = serial.Serial(port, 115200, timeout=0.5, write_timeout=1.0)
    ser.dtr = True
    time.sleep(0.3)
    ser.reset_input_buffer()
    cmd(ser, "D0"); cmd(ser, "K0 68"); cmd(ser, "F%d" % int(args.freq), 0.6)
    tone = 1000.0
    trim = {"dcI": 0.0, "dcQ": 0.0, "c1": 0.0, "c2": 1.0}
    keyed_until = [0.0]

    def apply():
        cmd(ser, "z %d %d %d %d" % (round(trim["dcI"] * 10), round(trim["dcQ"] * 10), round(trim["c1"] * 10000), round(trim["c2"] * 10000)), 0.12)

    def measure(rsa):
        """returns (wanted dBm, carrier dBc, image dBc)"""
        if time.time() > keyed_until[0] - 2.0:               # keep the key down in 9 s stretches
            cmd(ser, "X9500 %d 0" % int(tone), 0.15)
            keyed_until[0] = time.time() + 9.0
            time.sleep(0.5)
        time.sleep(0.08)
        iq, fs, _ = rsa.iq_capture(args.freq, bandwidth_hz=20e3, duration_s=0.12, ref_level_dbm=args.ref)
        w = np.hanning(len(iq))
        S = np.fft.fftshift(np.abs(np.fft.fft(iq * w)) ** 2)
        fr = np.fft.fftshift(np.fft.fftfreq(len(iq), 1 / fs))
        k = int(np.argmax(S))
        fw = fr[k]                                           # the wanted tone (LSB: carrier is 1 kHz above it)

        def band(fc):
            m = np.abs(fr - fc) < 60
            return float(S[m].max())
        want = band(fw)
        dbm = 10 * np.log10(10 * np.mean(np.abs(iq) ** 2)) + args.att
        return dbm, 10 * np.log10(band(fw + tone) / want), 10 * np.log10(band(fw + 2 * tone) / want)

    def search(rsa, key, which, step, lo, hi):
        """1-D search on trim[key] minimising carrier (which=1) or image (which=2)."""
        best_v = trim[key]
        apply(); best = measure(rsa)[which]
        while step >= (0.2 if key.startswith("dc") else 0.0005):
            improved = False
            for cand in (best_v + step, best_v - step):
                if cand < lo or cand > hi:
                    continue
                trim[key] = cand; apply()
                v = measure(rsa)[which]
                if v < best - 0.3:
                    best, best_v, improved = v, cand, True
                    break
            trim[key] = best_v
            if not improved:
                step /= 2.0
        apply()
        return best

    try:
        with RSA306() as rsa:
            apply()
            dbm, car, img = measure(rsa)
            print("before : %+6.1f dBm, carrier %6.1f dBc, opposite sideband %6.1f dBc" % (dbm, car, img))
            for rnd in range(3):
                c = search(rsa, "dcI", 1, 16.0, -300.0, 300.0)
                c = search(rsa, "dcQ", 1, 16.0, -300.0, 300.0)
                i = search(rsa, "c1", 2, 0.02, -0.3, 0.3)
                i = search(rsa, "c2", 2, 0.02, 0.7, 1.3)
                print("round %d: carrier %6.1f dBc, opposite sideband %6.1f dBc   dcI=%.1f dcQ=%.1f c1=%.4f c2=%.4f"
                      % (rnd + 1, c, i, trim["dcI"], trim["dcQ"], trim["c1"], trim["c2"]))
            apply()
            dbm, car, img = measure(rsa)
            print("after  : %+6.1f dBm, carrier %6.1f dBc, opposite sideband %6.1f dBc" % (dbm, car, img))
    finally:
        cmd(ser, "X0", 0.3)
        if args.dry_run:
            cmd(ser, "z 0 0 0 10000")
            print("dry run: trim reset, nothing stored")
        else:
            print(cmd(ser, "zs", 0.4))
        ser.close()


if __name__ == "__main__":
    main()
