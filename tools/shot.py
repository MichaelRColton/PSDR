#!/usr/bin/env python3
"""
PSDR screenshot tool
====================
Reads the PSDR's 320x240 LCD back over the USB-CDC console and saves it as a PNG.

The firmware's `p<0-3>` console command streams one 60-row band of the screen:
a text header `IMG 320 <rows> 565 <y0>` followed by rows*640 bytes of RGB565
(big-endian, 2 bytes/pixel) and a 2-byte zero flush. A full frame in one
transfer stalls the CDC endpoint, so the host pulls four bands and stitches them.

Run:   python tools/shot.py                 (auto-finds the port, writes psdr-<time>.png)
       python tools/shot.py -p COM28 -o screen.png
Deps:  pyserial only. Close the control panel first (one program per COM port),
       or use the panel's own Screenshot button, which uses the functions below.
"""
import argparse
import re
import struct
import sys
import time
import zlib

WIDTH = 320
HEIGHT = 240
BAND_ROWS = 60
BANDS = HEIGHT // BAND_ROWS
ROW_BYTES = WIDTH * 2
VID_PID = ("0483", "5740")

IMG_HEADER = re.compile(r"^IMG (\d+) (\d+) 565 (\d+)$")


def rgb565_to_rgb(data):
    """Big-endian RGB565 bytes -> packed 8-bit RGB bytes."""
    out = bytearray(len(data) // 2 * 3)
    j = 0
    for i in range(0, len(data) - 1, 2):
        px = (data[i] << 8) | data[i + 1]
        r = (px >> 11) & 0x1F
        g = (px >> 5) & 0x3F
        b = px & 0x1F
        out[j] = (r << 3) | (r >> 2)
        out[j + 1] = (g << 2) | (g >> 4)
        out[j + 2] = (b << 3) | (b >> 2)
        j += 3
    return bytes(out)


def stitch(bands):
    """{y0: rgb565 band bytes} -> full-frame RGB bytes. Missing bands stay black."""
    frame = bytearray(WIDTH * HEIGHT * 3)
    for y0, raw in bands.items():
        rgb = rgb565_to_rgb(raw)
        frame[y0 * WIDTH * 3: y0 * WIDTH * 3 + len(rgb)] = rgb
    return bytes(frame[:WIDTH * HEIGHT * 3])


def ppm_bytes(rgb, width=WIDTH, height=HEIGHT):
    """RGB bytes -> binary PPM (what tk.PhotoImage(data=...) can load)."""
    return b"P6\n%d %d\n255\n" % (width, height) + rgb


def write_png(path, rgb, width=WIDTH, height=HEIGHT):
    """Write packed RGB bytes as a PNG using only the standard library."""
    def chunk(tag, body):
        return (struct.pack(">I", len(body)) + tag + body
                + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF))

    stride = width * 3
    raw = b"".join(b"\x00" + rgb[y * stride:(y + 1) * stride] for y in range(height))
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def find_port():
    import serial.tools.list_ports
    for p in serial.tools.list_ports.comports():
        hwid = (p.hwid or "").upper()
        if VID_PID[0] in hwid and VID_PID[1] in hwid:
            return p.device
    return None


def read_band(ser, band, timeout=6.0):
    """Send p<band> and return (y0, rgb565 bytes) for that band."""
    ser.reset_input_buffer()
    ser.write(("p%d\r\n" % band).encode())
    deadline = time.time() + timeout
    buf = b""
    while time.time() < deadline:
        buf += ser.read(4096)
        # Other console output (status lines) may precede the header; skip it.
        while b"\n" in buf:
            line, rest = buf.split(b"\n", 1)
            m = IMG_HEADER.match(line.decode(errors="replace").strip().lstrip("\x00"))
            if not m:
                buf = rest
                continue
            rows, y0 = int(m.group(2)), int(m.group(3))
            need = rows * ROW_BYTES
            data = rest
            while len(data) < need and time.time() < deadline:
                data += ser.read(need - len(data))
            if len(data) < need:
                raise TimeoutError("band %d: got %d of %d bytes" % (band, len(data), need))
            return y0, data[:need]
    raise TimeoutError("band %d: no IMG header from the device" % band)


def capture(ser):
    """Pull all four bands from an open serial port; returns full-frame RGB bytes."""
    bands = {}
    for band in range(BANDS):
        y0, data = read_band(ser, band)
        bands[y0] = data
        time.sleep(0.05)   # let the trailing 2-byte flush arrive before the next band
    return stitch(bands)


def main():
    import serial
    ap = argparse.ArgumentParser(description="Save the PSDR's screen as a PNG.")
    ap.add_argument("-p", "--port", help="COM port (default: auto-detect VID 0483 / PID 5740)")
    ap.add_argument("-o", "--out", help="output file (default: psdr-<date>-<time>.png)")
    args = ap.parse_args()

    port = args.port or find_port()
    if not port:
        sys.exit("No PSDR found (VID 0483 / PID 5740). Plug it in or pass -p COMx.")
    out = args.out or time.strftime("psdr-%Y%m%d-%H%M%S.png")
    with serial.Serial(port, 115200, timeout=0.5, write_timeout=1.0) as ser:
        ser.dtr = True
        rgb = capture(ser)
    write_png(out, rgb)
    print("saved", out)


if __name__ == "__main__":
    main()
