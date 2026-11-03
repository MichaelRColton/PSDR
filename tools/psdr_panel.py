#!/usr/bin/env python3
"""
PSDR RF Control Panel
=====================
A little GUI over the PSDR's USB-CDC debug console. It draws the RF signal chain
and makes every console-exposed setting changeable in place, and shows the live
diagnostics the firmware streams back (the real S-meter bar, VFO, mux/switch
state, HMC1023 reg2).

Pairs with the `fix/hmc1023-lpf` console build (commands: s P f a A m v F g M L H
t x b r R G D K). Talk is 115200 8N1 over the CDC port (VID 0483 / PID 5740).
"Listen on PC speakers" plays the PSDR USB audio mic out the default output via
ffplay (install ffmpeg for that button; everything else needs only pyserial).

Run:  python tools/psdr_panel.py
Deps: pyserial (stdlib tkinter); ffmpeg/ffplay for the Listen button.
Only one program can hold the COM port at once (audio is a separate USB function).
"""
import glob
import os
import queue
import re
import shutil
import subprocess
import threading
import time
import tkinter as tk
from tkinter import ttk

import serial
import serial.tools.list_ports


# --------------------------------------------------------------------------- #
# Audio: play the PSDR USB microphone out the PC speakers via ffplay. ffplay
# handles the resample/buffering; we just locate the tools and the dshow device.
# --------------------------------------------------------------------------- #
def _find_tool(name):
    p = shutil.which(name)
    if p:
        return p
    pats = [
        os.path.expandvars(
            rf"%LOCALAPPDATA%\Microsoft\WinGet\Packages\Gyan.FFmpeg*\ffmpeg*\bin\{name}.exe"),
        rf"C:\ProgramData\chocolatey\bin\{name}.exe",
        rf"C:\ffmpeg\bin\{name}.exe",
    ]
    for pat in pats:
        hit = glob.glob(pat)
        if hit:
            return hit[0]
    return None


def _psdr_audio_name(ffmpeg):
    """Return the exact dshow capture-device name for the PSDR mic, or None."""
    try:
        r = subprocess.run([ffmpeg, "-hide_banner", "-list_devices", "true",
                            "-f", "dshow", "-i", "dummy"],
                           capture_output=True, text=True, timeout=10)
    except Exception:
        return None
    for line in (r.stderr or "").splitlines():
        if "PSDR" in line:
            m = re.search(r'"([^"]*PSDR[^"]*)"', line)
            if m:
                return m.group(1)
    return None


class AudioListen:
    def __init__(self):
        self.proc = None

    def start(self):
        ffplay = _find_tool("ffplay")
        ffmpeg = _find_tool("ffmpeg")
        if not ffplay:
            raise RuntimeError("ffplay not found - install ffmpeg (winget install Gyan.FFmpeg)")
        name = _psdr_audio_name(ffmpeg) if ffmpeg else None
        if not name:
            raise RuntimeError("PSDR audio device not found (is the USB mic enabled?)")
        self.proc = subprocess.Popen(
            [ffplay, "-hide_banner", "-nodisp", "-loglevel", "warning",
             "-fflags", "nobuffer", "-flags", "low_delay", "-probesize", "32",
             "-analyzeduration", "0", "-f", "dshow", "-i", f"audio={name}"],
            stdin=subprocess.DEVNULL)
        return name

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def stop(self):
        if self.proc:
            try:
                self.proc.terminate()
            except Exception:
                pass
            self.proc = None

VID_PID = ("0483", "5740")           # STM32 CDC
BAUD = 115200
STATUS_RE = re.compile(
    r"rms=(\d+)\s+bar=(-?\d+)\s+S=(\d+)\s+vfo=(-?\d+)\s+filt=(\d+)\s+mux=(\d+)"
    r"\s+amp=(\d+)/pol(\d+)\s+trx=(\d+)\s+txrf=(\d+)\s+imp=(\d+)\s+reg2=([0-9A-Fa-f]+)"
    r"(?:\s+mode=(\d+)\s+flo=(-?\d+)\s+fhi=(-?\d+))?"
)
MODE_NAMES = {0: "LSB", 1: "USB", 2: "AM"}
HZ_PER_BIN = 46.875                      # 24 kHz Fs over the 512-wide filter array
QUICK_RE = re.compile(r"rms=(\d+)\s+bar=(-?\d+)")

MUX_NAMES = {0: "ext filter (DNF)", 1: "bypass (stock)",
             2: "HMC1023 → RX", 3: "HMC1023 → TX"}
COARSE_MHZ = ["5", "7", "10", "14", "20", "28", "40", "50", "72"]

BG = "#10141c"; PANEL = "#1b2230"; INK = "#e6ecf5"; MUTE = "#5d6b82"
ACCENT = "#46b1ff"; HOT = "#ff6b6b"; GOOD = "#4fd08a"; WIRE = "#3a4559"


# --------------------------------------------------------------------------- #
# Serial link: a reader thread feeds decoded lines into a queue.
# --------------------------------------------------------------------------- #
class SerialLink:
    def __init__(self):
        self.ser = None
        self.rx = queue.Queue()
        self._stop = threading.Event()
        self._buf = b""

    def auto_port(self):
        for p in serial.tools.list_ports.comports():
            hwid = (p.hwid or "").upper()
            if f"VID:PID={VID_PID[0]}:{VID_PID[1]}" in hwid or (
                VID_PID[0] in hwid and VID_PID[1] in hwid):
                return p.device
        return None

    def ports(self):
        return [p.device for p in serial.tools.list_ports.comports()]

    def open(self, port):
        self.close()
        self.ser = serial.Serial(port, BAUD, timeout=0.2)
        self._stop.clear()
        threading.Thread(target=self._reader, daemon=True).start()

    def _reader(self):
        while not self._stop.is_set() and self.ser:
            try:
                data = self.ser.read(256)
            except Exception as e:
                self.rx.put(("err", str(e)))
                break
            if data:
                self._buf += data
                while b"\n" in self._buf:
                    line, self._buf = self._buf.split(b"\n", 1)
                    self.rx.put(("line", line.decode(errors="replace").strip()))

    def write(self, cmd):
        if self.ser:
            try:
                self.ser.write((cmd + "\r\n").encode())
                self.rx.put(("tx", cmd))
            except Exception as e:
                self.rx.put(("err", str(e)))

    def close(self):
        self._stop.set()
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None


# --------------------------------------------------------------------------- #
# The app.
# --------------------------------------------------------------------------- #
class Panel(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("PSDR RF Control Panel")
        self.configure(bg=BG)
        self.link = SerialLink()

        # local model of chip/switch state (mirrors the firmware)
        self.mux = 1
        self.coarse = 4          # 20 MHz
        self.gain10 = 1
        self.bypass = 0
        self.lpf_pwr = 0
        self.bench = 0
        self.filt = 0
        self.amp = 0
        self.diag = {}           # last parsed status
        self.bar = 0
        self.audio = AudioListen()
        self.mode_i = 0          # 0 LSB 1 USB 2 AM
        self.flo = 0             # DSP filter HPF edge (bins)
        self.fhi = 68            # DSP filter LPF edge (bins)
        self._filt_sync = False  # suppress echo while syncing sliders from status

        self._build()
        self.after(120, self._pump)

    # ---- layout -------------------------------------------------------- #
    def _build(self):
        style = ttk.Style(self)
        try:
            style.theme_use("clam")
        except Exception:
            pass
        style.configure("TButton", padding=4)

        top = tk.Frame(self, bg=BG)
        top.pack(fill="x", padx=10, pady=(10, 4))
        tk.Label(top, text="Port", bg=BG, fg=MUTE).pack(side="left")
        self.port_var = tk.StringVar()
        self.port_box = ttk.Combobox(top, textvariable=self.port_var, width=10,
                                     values=self.link.ports())
        self.port_box.pack(side="left", padx=6)
        auto = self.link.auto_port()
        if auto:
            self.port_var.set(auto)
        self.conn_btn = tk.Button(top, text="Connect", command=self.toggle_conn,
                                  bg=PANEL, fg=INK, activebackground=ACCENT, width=10)
        self.conn_btn.pack(side="left")
        self.poll_var = tk.BooleanVar(value=True)
        tk.Checkbutton(top, text="auto-poll (s)", variable=self.poll_var, bg=BG,
                       fg=MUTE, selectcolor=PANEL, activebackground=BG).pack(side="left", padx=10)
        self.status_lbl = tk.Label(top, text="disconnected", bg=BG, fg=HOT)
        self.status_lbl.pack(side="right")

        # canvas diagram
        self.canvas = tk.Canvas(self, width=1000, height=240, bg=BG,
                                highlightthickness=0)
        self.canvas.pack(fill="x", padx=10, pady=4)

        body = tk.Frame(self, bg=BG)
        body.pack(fill="both", expand=True, padx=10, pady=4)
        self._build_controls(body)
        self._build_diag(body)

        self.draw()

    def _group(self, parent, title):
        f = tk.LabelFrame(parent, text=title, bg=PANEL, fg=ACCENT,
                          labelanchor="nw", bd=1, relief="solid")
        f.configure(highlightbackground=WIRE)
        return f

    def _build_controls(self, parent):
        col = tk.Frame(parent, bg=BG)
        col.pack(side="left", fill="y")

        # --- front end / path ---
        g = self._group(col, "Front end  (bench override)")
        g.pack(fill="x", pady=3)
        self.bench_var = tk.BooleanVar()
        tk.Checkbutton(g, text="bench override (g)", variable=self.bench_var,
                       command=lambda: self.send_flag("g", self.bench_var, "bench"),
                       bg=PANEL, fg=INK, selectcolor=BG).grid(row=0, column=0, columnspan=2, sticky="w")
        tk.Label(g, text="mux route (M)", bg=PANEL, fg=MUTE).grid(row=1, column=0, sticky="w")
        self.mux_var = tk.IntVar(value=self.mux)
        for i, name in MUX_NAMES.items():
            tk.Radiobutton(g, text=f"{i}: {name}", variable=self.mux_var, value=i,
                           command=self.set_mux, bg=PANEL, fg=INK, selectcolor=BG,
                           anchor="w").grid(row=2 + i, column=0, columnspan=2, sticky="w")
        self.filt_var = tk.BooleanVar()
        tk.Checkbutton(g, text="filter_in / 5V boost (f)", variable=self.filt_var,
                       command=lambda: self.send_flag("f", self.filt_var, "filt"),
                       bg=PANEL, fg=INK, selectcolor=BG).grid(row=7, column=0, columnspan=2, sticky="w")

        # --- HMC1023 ---
        g = self._group(col, "HMC1023  (RF preamp, pre-mixer)")
        g.pack(fill="x", pady=3)
        tk.Button(g, text="Configure for RX  (H)", command=self.hmc_config,
                  bg=BG, fg=GOOD).grid(row=0, column=0, columnspan=2, sticky="we", pady=2)
        self.lpf_var = tk.BooleanVar()
        tk.Checkbutton(g, text="LPF power (L)", variable=self.lpf_var,
                       command=lambda: self.send_flag("L", self.lpf_var, "lpf_pwr"),
                       bg=PANEL, fg=INK, selectcolor=BG).grid(row=1, column=0, sticky="w")
        self.gain_var = tk.BooleanVar(value=True)
        tk.Checkbutton(g, text="+10 dB/ch gain", variable=self.gain_var,
                       command=self.push_reg2, bg=PANEL, fg=INK,
                       selectcolor=BG).grid(row=1, column=1, sticky="w")
        self.byp_var = tk.BooleanVar()
        tk.Checkbutton(g, text="filter bypass", variable=self.byp_var,
                       command=self.push_reg2, bg=PANEL, fg=INK,
                       selectcolor=BG).grid(row=2, column=0, sticky="w")
        tk.Label(g, text="corner MHz", bg=PANEL, fg=MUTE).grid(row=2, column=1, sticky="e")
        self.coarse_var = tk.StringVar(value=COARSE_MHZ[self.coarse])
        cb = ttk.Combobox(g, textvariable=self.coarse_var, values=COARSE_MHZ,
                          width=5, state="readonly")
        cb.grid(row=3, column=1, sticky="e")
        cb.bind("<<ComboboxSelected>>", lambda e: self.push_reg2())
        self.reg2_lbl = tk.Label(g, text="reg2=0x115", bg=PANEL, fg=MUTE)
        self.reg2_lbl.grid(row=3, column=0, sticky="w")

        # --- baseband / tuning ---
        g = self._group(col, "Baseband & tuning")
        g.pack(fill="x", pady=3)
        tk.Label(g, text="gain pot (G, MCP6N11)", bg=PANEL, fg=MUTE).grid(row=0, column=0, sticky="w")
        self.gpot = tk.Scale(g, from_=0, to=255, orient="horizontal", length=180,
                             bg=PANEL, fg=INK, troughcolor=BG, highlightthickness=0,
                             command=self.set_gpot)
        self.gpot.set(200)
        self.gpot.grid(row=1, column=0, columnspan=2, sticky="we")
        tk.Label(g, text="volume (v)", bg=PANEL, fg=MUTE).grid(row=2, column=0, sticky="w")
        self.vol = tk.Scale(g, from_=0, to=2, resolution=0.05, orient="horizontal",
                            length=180, bg=PANEL, fg=INK, troughcolor=BG,
                            highlightthickness=0, command=self.set_vol)
        self.vol.set(0.5)
        self.vol.grid(row=3, column=0, columnspan=2, sticky="we")
        tk.Label(g, text="VFO MHz (F)", bg=PANEL, fg=MUTE).grid(row=4, column=0, sticky="w")
        self.vfo_var = tk.StringVar(value="7.032")
        e = tk.Entry(g, textvariable=self.vfo_var, width=10, bg=BG, fg=INK,
                     insertbackground=INK)
        e.grid(row=4, column=1, sticky="e")
        e.bind("<Return>", lambda ev: self.set_vfo())
        tk.Button(g, text="Set", command=self.set_vfo, bg=BG, fg=INK).grid(row=5, column=1, sticky="e")

        # --- demod + audio filter + listen ---
        g = self._group(col, "Demod, filter & audio")
        g.pack(fill="x", pady=3)
        tk.Label(g, text="mode (D)", bg=PANEL, fg=MUTE).grid(row=0, column=0, sticky="w")
        self.mode_var = tk.IntVar(value=self.mode_i)
        mf = tk.Frame(g, bg=PANEL)
        mf.grid(row=0, column=1, sticky="e")
        for val, nm in MODE_NAMES.items():
            tk.Radiobutton(mf, text=nm, variable=self.mode_var, value=val,
                           command=self.set_mode, bg=PANEL, fg=INK, selectcolor=BG,
                           indicatoron=True).pack(side="left")
        tk.Label(g, text="HPF edge (K lo)", bg=PANEL, fg=MUTE).grid(row=1, column=0, sticky="w")
        self.hpf = tk.Scale(g, from_=0, to=128, orient="horizontal", length=180,
                            bg=PANEL, fg=INK, troughcolor=BG, highlightthickness=0,
                            command=self.set_filter)
        self.hpf.set(self.flo)
        self.hpf.grid(row=2, column=0, columnspan=2, sticky="we")
        tk.Label(g, text="LPF edge (K hi)", bg=PANEL, fg=MUTE).grid(row=3, column=0, sticky="w")
        self.lpf = tk.Scale(g, from_=1, to=128, orient="horizontal", length=180,
                            bg=PANEL, fg=INK, troughcolor=BG, highlightthickness=0,
                            command=self.set_filter)
        self.lpf.set(self.fhi)
        self.lpf.grid(row=4, column=0, columnspan=2, sticky="we")
        self.filt_hz_lbl = tk.Label(g, text="passband —", bg=PANEL, fg=MUTE)
        self.filt_hz_lbl.grid(row=5, column=0, columnspan=2, sticky="w")
        tk.Label(g, text="USB level (Z)", bg=PANEL, fg=MUTE).grid(row=6, column=0, sticky="w")
        self.usbg = tk.Scale(g, from_=0, to=50, resolution=1, orient="horizontal", length=180,
                             bg=PANEL, fg=INK, troughcolor=BG, highlightthickness=0,
                             command=self.set_usbgain)
        self.usbg.set(10)
        self.usbg.grid(row=7, column=0, columnspan=2, sticky="we")
        self.listen_btn = tk.Button(g, text="Listen on PC speakers", command=self.toggle_listen,
                                    bg=BG, fg=GOOD)
        self.listen_btn.grid(row=8, column=0, columnspan=2, sticky="we", pady=(4, 2))

        # --- TX / switches ---
        g = self._group(col, "T/R & RF switches")
        g.pack(fill="x", pady=3)
        self.trx_var = tk.BooleanVar()
        tk.Checkbutton(g, text="TRX (t) 1=TX", variable=self.trx_var,
                       command=lambda: self.send_flag("t", self.trx_var),
                       bg=PANEL, fg=INK, selectcolor=BG).grid(row=0, column=0, sticky="w")
        self.amp_var = tk.BooleanVar()
        tk.Checkbutton(g, text="HMC482 amp (a)", variable=self.amp_var,
                       command=lambda: self.send_flag("a", self.amp_var, "amp"),
                       bg=PANEL, fg=INK, selectcolor=BG).grid(row=0, column=1, sticky="w")
        self.txrf_var = tk.BooleanVar(value=True)
        tk.Checkbutton(g, text="TX RF sw (x)", variable=self.txrf_var,
                       command=lambda: self.send_flag("x", self.txrf_var),
                       bg=PANEL, fg=INK, selectcolor=BG).grid(row=1, column=0, sticky="w")
        self.imp_var = tk.BooleanVar()
        tk.Checkbutton(g, text="imp bridge (b)", variable=self.imp_var,
                       command=lambda: self.send_flag("b", self.imp_var),
                       bg=PANEL, fg=INK, selectcolor=BG).grid(row=1, column=1, sticky="w")

        # --- raw console ---
        g = self._group(col, "Raw console")
        g.pack(fill="x", pady=3)
        self.raw_var = tk.StringVar()
        re_ = tk.Entry(g, textvariable=self.raw_var, width=18, bg=BG, fg=INK,
                       insertbackground=INK)
        re_.grid(row=0, column=0, sticky="we")
        re_.bind("<Return>", lambda ev: self.send_raw())
        tk.Button(g, text="Send", command=self.send_raw, bg=BG, fg=INK).grid(row=0, column=1)

    def _build_diag(self, parent):
        col = tk.Frame(parent, bg=BG)
        col.pack(side="left", fill="both", expand=True, padx=(10, 0))

        g = self._group(col, "Live diagnostics  (s / P)")
        g.pack(fill="x")
        self.meter = tk.Canvas(g, width=340, height=34, bg=BG, highlightthickness=0)
        self.meter.pack(fill="x", padx=6, pady=6)
        self.diag_txt = tk.Label(g, text="—", bg=PANEL, fg=INK, justify="left",
                                 font=("Consolas", 10), anchor="w")
        self.diag_txt.pack(fill="x", padx=6, pady=(0, 6))

        g = self._group(col, "Console log")
        g.pack(fill="both", expand=True, pady=(6, 0))
        self.log = tk.Text(g, height=16, bg="#0b0e14", fg="#b7c3d6",
                           insertbackground=INK, font=("Consolas", 9), wrap="none")
        self.log.pack(fill="both", expand=True)
        for tag, color in (("tx", ACCENT), ("err", HOT), ("rx", "#8fa0b8")):
            self.log.tag_config(tag, foreground=color)

    # ---- diagram ------------------------------------------------------- #
    def box(self, x, y, w, h, text, active=True, powered=None):
        fill = PANEL if active else BG
        outline = ACCENT if active else WIRE
        if powered is True:
            outline = GOOD
        elif powered is False:
            outline = MUTE
        self.canvas.create_rectangle(x, y, x + w, y + h, fill=fill, outline=outline, width=2)
        self.canvas.create_text(x + w / 2, y + h / 2, text=text, fill=INK if active else MUTE,
                                 font=("Segoe UI", 8), width=w - 8, justify="center")
        return (x, y, w, h)

    def arrow(self, x1, y1, x2, y2, active=True):
        self.canvas.create_line(x1, y1, x2, y2, fill=ACCENT if active else WIRE,
                                 width=3 if active else 1, arrow="last")

    def draw(self):
        c = self.canvas
        c.delete("all")
        through = self.mux in (2, 3)          # routed through the HMC1023
        y = 40; h = 46
        xs = [20, 150, 275, 400, 545, 675, 820]
        w = 110
        labels = [
            "ANTENNA\n(ANT/VNA-IN)",
            "MASWSS0129\nU16/U24/U30",
            "L39 balun\nSE→diff",
            "MUX U211/U298\n(FILTER_S0/S1)",
            "L82 balun\n+ ADE-2ASK\nmixers (I/Q)",
            "MCP6N11\nin-amps",
            "STM32F746\nADC",
        ]
        for i, (x, lbl) in enumerate(zip(xs, labels)):
            self.box(x, y, w, h, lbl)
            if i < len(xs) - 1:
                self.arrow(x + w, y + h / 2, xs[i + 1], y + h / 2,
                           active=not (i == 3 and through))
        # HMC1023 branch under the mux
        mx = xs[3]
        hy = y + 110
        self.box(mx - 10, hy, w + 20, h + 8,
                 "HMC1023 U287\nRF preamp +10dB/ch\n(cascade ≈ 20 dB)",
                 active=through, powered=bool(self.lpf_pwr))
        self.arrow(mx + w / 2 - 14, y + h, mx + w / 2 - 14, hy, active=through)
        self.arrow(mx + w / 2 + 14, hy, mx + w / 2 + 14, y + h, active=through)
        # LO
        c.create_text(xs[4] + w / 2, y + h + 16, text="↑ quad LO (Si5338 ÷ SY10EP52)",
                      fill=MUTE, font=("Segoe UI", 7))
        # legend / route
        c.create_text(20, 12, anchor="w", fill=MUTE, font=("Segoe UI", 8),
                      text=f"RX path   •   mux = {self.mux}: {MUX_NAMES[self.mux]}"
                           f"   •   {'THROUGH HMC1023' if through else 'straight bypass'}")
        c.create_text(980, 12, anchor="e", fill=GOOD if self.lpf_pwr else MUTE,
                      font=("Segoe UI", 8),
                      text=("HMC1023 powered" if self.lpf_pwr else "HMC1023 unpowered"))
        self.draw_meter()

    def draw_meter(self):
        m = self.meter
        m.delete("all")
        w = int(m["width"]); h = int(m["height"])
        m.create_rectangle(0, 0, w, h, outline=WIRE)
        frac = max(0.0, min(1.0, self.bar / 110.0))
        col = GOOD if frac < 0.75 else HOT
        m.create_rectangle(0, 0, int(w * frac), h, fill=col, outline="")
        m.create_text(w - 6, h / 2, anchor="e", fill=INK,
                      text=f"bar {self.bar}", font=("Consolas", 10))

    # ---- actions ------------------------------------------------------- #
    def send_flag(self, cmd, var, attr=None):
        v = 1 if var.get() else 0
        self.link.write(f"{cmd}{v}")
        if attr:
            setattr(self, attr, v)
        self.draw()

    def set_mux(self):
        self.mux = self.mux_var.get()
        self.link.write(f"M{self.mux}")
        self.draw()

    def hmc_config(self):
        self.link.write("H")
        self.lpf_pwr = 1; self.lpf_var.set(True)
        self.gain_var.set(True); self.byp_var.set(False)
        self.coarse_var.set("20"); self.coarse = 4
        self.reg2_lbl.config(text="reg2=0x115")
        self.draw()

    def push_reg2(self):
        self.gain10 = 1 if self.gain_var.get() else 0
        self.bypass = 1 if self.byp_var.get() else 0
        self.coarse = COARSE_MHZ.index(self.coarse_var.get())
        reg2 = (self.coarse << 6) | (self.bypass << 5) | (self.gain10 << 4) | (1 << 2) | 1
        self.link.write(f"r2 {reg2:X}")
        self.reg2_lbl.config(text=f"reg2=0x{reg2:X}")

    def set_gpot(self, _=None):
        self.link.write(f"G{int(float(self.gpot.get()))}")

    def set_vol(self, _=None):
        self.link.write(f"v{float(self.vol.get()):.2f}")

    def set_vfo(self):
        try:
            hz = int(round(float(self.vfo_var.get()) * 1e6))
        except ValueError:
            return
        self.link.write(f"F{hz}")

    def send_raw(self):
        cmd = self.raw_var.get().strip()
        if cmd:
            self.link.write(cmd)
            self.raw_var.set("")

    def set_mode(self):
        self.mode_i = self.mode_var.get()
        self.link.write(f"D{self.mode_i}")

    def set_usbgain(self, _=None):
        self.link.write(f"Z{int(float(self.usbg.get()))}")

    def _filt_hz_label(self):
        lo, hi = int(self.hpf.get()), int(self.lpf.get())
        self.filt_hz_lbl.config(
            text=f"passband ~{int(lo*HZ_PER_BIN)}..{int(hi*HZ_PER_BIN)} Hz  (bins {lo}..{hi})")

    def set_filter(self, _=None):
        if self._filt_sync:
            return
        lo, hi = int(self.hpf.get()), int(self.lpf.get())
        if hi <= lo:                       # keep LPF above HPF
            hi = lo + 1
            self._filt_sync = True
            self.lpf.set(hi)
            self._filt_sync = False
        self.flo, self.fhi = lo, hi
        self._filt_hz_label()
        self.link.write(f"K{lo} {hi}")

    def toggle_listen(self):
        if self.audio.alive():
            self.audio.stop()
            self.listen_btn.config(text="Listen on PC speakers", fg=GOOD)
            self._log("! audio stopped", "tx")
            return
        try:
            name = self.audio.start()
            self.listen_btn.config(text="Stop listening", fg=HOT)
            self._log(f"> listening to '{name}' on default output", "tx")
        except Exception as e:
            self._log(f"! {e}", "err")

    # ---- connection & polling ----------------------------------------- #
    def toggle_conn(self):
        if self.link.ser:
            self.link.close()
            self.conn_btn.config(text="Connect")
            self.status_lbl.config(text="disconnected", fg=HOT)
            return
        port = self.port_var.get().strip()
        if not port:
            self.status_lbl.config(text="no port", fg=HOT)
            return
        try:
            self.link.open(port)
        except Exception as e:
            self.status_lbl.config(text=f"open failed: {e}", fg=HOT)
            return
        self.conn_btn.config(text="Disconnect")
        self.status_lbl.config(text=f"connected {port}", fg=GOOD)

    def _log(self, text, tag="rx"):
        self.log.insert("end", text + "\n", tag)
        if int(self.log.index("end-1c").split(".")[0]) > 400:
            self.log.delete("1.0", "100.0")
        self.log.see("end")

    def _pump(self):
        # drain serial queue
        try:
            while True:
                kind, payload = self.link.rx.get_nowait()
                if kind == "line":
                    self._parse(payload)
                    if payload:
                        self._log("  " + payload, "rx")
                elif kind == "tx":
                    self._log("> " + payload, "tx")
                elif kind == "err":
                    self._log("! " + payload, "err")
                    self.status_lbl.config(text=payload, fg=HOT)
        except queue.Empty:
            pass
        # periodic poll
        now = time.time()
        if self.link.ser and self.poll_var.get() and now - getattr(self, "_last_poll", 0) > 0.7:
            self._last_poll = now
            self.link.write("s")
        # reflect ffplay exiting on its own
        if self.listen_btn["text"] == "Stop listening" and not self.audio.alive():
            self.listen_btn.config(text="Listen on PC speakers", fg=GOOD)
        self.after(120, self._pump)

    def _parse(self, line):
        m = STATUS_RE.search(line)
        if m:
            g = m.groups()
            (rms, bar, s, vfo, filt, mux, amp, pol, trx, txrf, imp, reg2) = g[:12]
            mode, flo, fhi = g[12], g[13], g[14]
            self.bar = int(bar)
            self.diag = dict(rms=rms, bar=bar, S=s, vfo=vfo, filt=filt, mux=mux,
                             amp=amp, pol=pol, trx=trx, txrf=txrf, imp=imp, reg2=reg2)
            mhz = int(vfo) / 1e6
            demod = ""
            if mode is not None:
                self.mode_var.set(int(mode))
                self.mode_i = int(mode)
                # initialise the sliders from the firmware once, then leave them to the user
                if not getattr(self, "_filt_init", False):
                    self._filt_sync = True
                    self.hpf.set(int(flo))
                    self.lpf.set(int(fhi))
                    self._filt_sync = False
                    self.flo, self.fhi = int(flo), int(fhi)
                    self._filt_hz_label()
                    self._filt_init = True
                demod = (f"\nmode {MODE_NAMES.get(int(mode), '?')}   filt {flo}..{fhi} bins "
                         f"(~{int(int(flo) * HZ_PER_BIN)}..{int(int(fhi) * HZ_PER_BIN)} Hz)")
            self.diag_txt.config(
                text=(f"vfo {mhz:9.4f} MHz   S(raw) {s:>5}   rms {rms:>7}\n"
                      f"mux {mux} ({MUX_NAMES.get(int(mux), '?')})   filt {filt}\n"
                      f"amp {amp}/pol{pol}  trx {trx}  txrf {txrf}  imp {imp}\n"
                      f"HMC1023 reg2 =0x{reg2}" + demod))
            self.draw_meter()
            return
        q = QUICK_RE.search(line)
        if q:
            self.bar = int(q.group(2))
            self.draw_meter()

    def destroy(self):
        self.audio.stop()
        self.link.close()
        super().destroy()


if __name__ == "__main__":
    Panel().mainloop()
