#!/usr/bin/env python3
"""
PSDR RF Control Panel
=====================
A little GUI over the PSDR's USB-CDC debug console. It draws the RF signal chain
and makes every console-exposed setting changeable in place, and shows the live
diagnostics the firmware streams back (the real S-meter bar, VFO, mux/switch
state, HMC1023 reg2).

Pairs with the `fix/hmc1023-lpf` console build (commands: s P f a A m v F g M L H
t x b r R G D K p). Talk is 115200 8N1 over the CDC port (VID 0483 / PID 5740).
"Listen on PC speakers" plays the PSDR USB audio mic out the default output via
ffplay (install ffmpeg for that button; everything else needs only pyserial).

Run:  python tools/psdr_panel.py
Deps: pyserial (stdlib tkinter); ffmpeg/ffplay for the Listen button.
The Screenshot button reads the device's LCD back (console p) and shows it in
a window; it uses tools/shot.py, which also works standalone.
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

import shot   # tools/shot.py: screen readback decode + PNG writer


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
    names = []
    for line in (r.stderr or "").splitlines():
        m = re.search(r'"([^"]*PSDR[^"]*)"', line)
        if m:
            names.append(m.group(1))
    for n in names:                 # Listen = the demodulated audio, not raw I/Q
        if "Demod" in n:
            return n
    return names[0] if names else None


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
        # CREATE_NO_WINDOW hides ffplay's console window (it's a console app, so
        # -nodisp alone still pops a window on Windows); DEVNULL the streams too.
        self.proc = subprocess.Popen(
            [ffplay, "-hide_banner", "-nodisp", "-loglevel", "warning",
             "-fflags", "nobuffer", "-flags", "low_delay", "-probesize", "32",
             "-analyzeduration", "0", "-f", "dshow", "-i", f"audio={name}"],
            stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
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
LPF_MODES = ["Bypass", "0 dB", "+20 dB"]
LPF_CORNERS = ["auto", "5 MHz", "7 MHz", "10 MHz", "14 MHz", "20 MHz", "28 MHz", "40 MHz", "50 MHz", "72 MHz", "off"]
LPFC_RE = re.compile(r"lpfcorner=(-?\d+) coarse=(-?\d+) ontx=(\d)")
HZ_PER_BIN = 93.75                       # 48 kHz Fs over the 512-wide filter array
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
        self.tx = queue.Queue(maxsize=256)   # commands; drained by a writer thread
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
        # write_timeout: a stuck CDC OUT (device not draining) must NOT block the
        # Tk main thread - writes raise SerialTimeoutException instead, caught in write().
        self.ser = serial.Serial(port, BAUD, timeout=0.2, write_timeout=0.4)
        self._stop.clear()
        threading.Thread(target=self._reader, daemon=True).start()
        threading.Thread(target=self._writer, daemon=True).start()

    def _writer(self):
        # All serial writes happen here, off the Tk thread, so a stuck CDC OUT
        # can never freeze the UI. Also paced: >= 25 ms between commands so a
        # slider/scroll burst can't flood the device's USB and wedge it.
        last = 0.0
        while not self._stop.is_set():
            try:
                cmd = self.tx.get(timeout=0.2)
            except queue.Empty:
                continue
            ser = self.ser
            if not ser:
                continue
            gap = time.time() - last
            if gap < 0.025:
                time.sleep(0.025 - gap)
            try:
                ser.write((cmd + "\r\n").encode())
                self.rx.put(("tx", cmd))
                last = time.time()
            except Exception as e:
                self.rx.put(("err", str(e)))

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
                    # a screenshot band ends with a 2-byte zero flush; drop it
                    self._buf = self._buf.lstrip(b"\x00")
                    if b"\n" not in self._buf:
                        break
                    line, self._buf = self._buf.split(b"\n", 1)
                    text = line.decode(errors="replace").strip()
                    m = shot.IMG_HEADER.match(text)
                    if m:
                        self._read_band(int(m.group(2)), int(m.group(3)))
                        continue
                    self.rx.put(("line", text))

    def _read_band(self, rows, y0):
        # Binary screenshot band follows its "IMG ..." header: take exactly
        # rows*640 bytes so pixel data is never mistaken for console text.
        need = rows * shot.ROW_BYTES
        deadline = time.time() + 6.0
        while len(self._buf) < need and time.time() < deadline and not self._stop.is_set():
            try:
                self._buf += self.ser.read(need - len(self._buf))
            except Exception as e:
                self.rx.put(("err", str(e)))
                return
        if len(self._buf) < need:
            got, self._buf = len(self._buf), b""
            self.rx.put(("imgerr", f"band at row {y0}: got {got} of {need} bytes"))
            return
        data, self._buf = self._buf[:need], self._buf[need:]
        self.rx.put(("img", (y0, data)))

    def write(self, cmd):
        # Non-blocking: just enqueue. The writer thread does the actual I/O.
        if self.ser:
            try:
                self.tx.put_nowait(cmd)
            except queue.Full:
                pass            # drop under backpressure rather than block the UI

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
        self.vfo_hz = 7032000    # big readout / digit tuner
        self._last_tune = 0.0    # hold status-sync briefly after a manual tune
        self._freq_after = None  # debounce timer for the F command
        self._flow = []          # signal-flow polyline (canvas coords), set by draw()
        self._dots = []          # animated flow dots
        self._dot_phase = 0.0
        self._shot_bands = None  # {y0: rgb565 bytes} while a screenshot is in progress
        self._shot_next = 0      # next band index to request
        self._shot_t0 = 0.0      # when the current band was requested (timeout)
        self._shot_rgb = None    # last complete frame (RGB bytes)
        self._shot_win = None    # viewer window
        self._shot_img = None    # tk.PhotoImage shown in the viewer (keep a reference)

        self._build()
        self.after(120, self._pump)
        self.after(400, self._autoconnect)   # connect on launch so you don't have to hit Connect

    def _autoconnect(self):
        if not self.link.ser and self.port_var.get().strip():
            self.toggle_conn()

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

        # big frequency readout + digit tuner
        self._build_freq(self)

        # canvas diagram (RF chain + MCU/DSP chain, with signal-flow dots)
        self.canvas = tk.Canvas(self, width=1000, height=400, bg=BG,
                                highlightthickness=0)
        self.canvas.pack(fill="x", padx=10, pady=4)

        body = tk.Frame(self, bg=BG)
        body.pack(fill="both", expand=True, padx=10, pady=4)
        self._build_controls(body)
        self._build_diag(body)

        self.draw()
        self.after(60, self._animate)

    # ---- big frequency readout + per-digit tuner ----------------------- #
    def _build_freq(self, parent):
        bar = tk.Frame(parent, bg=BG)
        bar.pack(fill="x", padx=10, pady=(2, 2))
        tk.Label(bar, text="VFO", bg=BG, fg=MUTE, font=("Segoe UI", 10)).pack(side="left", padx=(0, 10))
        dfont = ("Consolas", 32, "bold")
        self.digit_lbls = []
        powers = [10 ** k for k in range(8, -1, -1)]   # 100 MHz .. 1 Hz
        for idx, p in enumerate(powers):
            lbl = tk.Label(bar, text="0", bg=BG, fg=INK, font=dfont, width=1, cursor="sb_v_double_arrow")
            lbl.pack(side="left")
            lbl._decade = p
            lbl.bind("<MouseWheel>", self._digit_scroll)
            lbl.bind("<Button-4>", lambda e: self._tune(e.widget._decade))    # X11 up
            lbl.bind("<Button-5>", lambda e: self._tune(-e.widget._decade))   # X11 down
            lbl.bind("<Button-1>", self._digit_click)
            lbl.bind("<Enter>", self._digit_enter)
            lbl.bind("<Leave>", lambda e: self._refresh_freq())
            self.digit_lbls.append(lbl)
            if idx == 2:   # decimal point after the MHz ones place
                tk.Label(bar, text=".", bg=BG, fg=ACCENT, font=dfont).pack(side="left")
            elif idx == 5: # thin gap between kHz and Hz groups
                tk.Label(bar, text=" ", bg=BG, fg=BG, font=dfont).pack(side="left")
        tk.Label(bar, text="MHz", bg=BG, fg=MUTE, font=("Segoe UI", 13)).pack(side="left", padx=(8, 0))
        tk.Label(bar, text="scroll / click a digit to tune",
                 bg=BG, fg=MUTE, font=("Segoe UI", 8)).pack(side="right")
        self._refresh_freq()

    def _refresh_freq(self):
        s = f"{int(self.vfo_hz):09d}"
        seen = False
        for i, lbl in enumerate(self.digit_lbls):
            ch = s[i]
            if ch != "0":
                seen = True
            dim = (not seen) and i < 2        # dim only leading MHz zeros
            lbl.config(text=ch, fg=MUTE if dim else INK)

    def _digit_enter(self, e):
        e.widget.config(fg=ACCENT)

    def _digit_scroll(self, e):
        self._tune(e.widget._decade if e.delta > 0 else -e.widget._decade)

    def _digit_click(self, e):
        up = e.y < (e.widget.winfo_height() / 2)
        self._tune(e.widget._decade if up else -e.widget._decade)

    def _tune(self, delta):
        hz = int(self.vfo_hz) + int(delta)
        hz = max(2500000, min(175000000, hz))
        self.vfo_hz = hz
        self._last_tune = time.time()
        self._refresh_freq()                       # display updates instantly
        self.vfo_var.set(f"{hz / 1e6:.6f}")
        # debounce the actual F command: one per tuning gesture, never a flood
        if self._freq_after is not None:
            try: self.after_cancel(self._freq_after)
            except Exception: pass
        self._freq_after = self.after(120, self._send_freq)

    def _send_freq(self):
        self._freq_after = None
        self.link.write(f"F{int(self.vfo_hz)}")

    def _group(self, parent, title):
        f = tk.LabelFrame(parent, text=title, bg=PANEL, fg=ACCENT,
                          labelanchor="nw", bd=1, relief="solid")
        f.configure(highlightbackground=WIRE)
        return f

    def _build_controls(self, parent):
        col = tk.Frame(parent, bg=BG)
        col.pack(side="left", fill="y")

        # --- front end / path ---
        g = self._group(col, "Front end")
        g.pack(fill="x", pady=3)
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

        # --- LPF (the on-device "LPF" setting, plus the corner override) ---
        g = self._group(col, "LPF  (l / k / n)")
        g.pack(fill="x", pady=3)
        tk.Label(g, text="path", bg=PANEL, fg=MUTE).grid(row=0, column=0, sticky="w")
        self.lpfmode_var = tk.StringVar(value="Bypass")
        cb = ttk.Combobox(g, textvariable=self.lpfmode_var, values=LPF_MODES, width=8, state="readonly")
        cb.grid(row=0, column=1, sticky="e")
        cb.bind("<<ComboboxSelected>>", lambda e: self.link.write(f"l{LPF_MODES.index(self.lpfmode_var.get())}"))
        tk.Label(g, text="corner", bg=PANEL, fg=MUTE).grid(row=1, column=0, sticky="w")
        self.lpfcorner_var = tk.StringVar(value="off")
        cb = ttk.Combobox(g, textvariable=self.lpfcorner_var, values=LPF_CORNERS, width=8, state="readonly")
        cb.grid(row=1, column=1, sticky="e")
        cb.bind("<<ComboboxSelected>>", lambda e: self.set_lpf_corner())
        self.lpftx_var = tk.BooleanVar()
        tk.Checkbutton(g, text="transmit through the LPF (needs VDDBG fix)", variable=self.lpftx_var,
                       command=lambda: self.link.write("n1" if self.lpftx_var.get() else "n0"),
                       bg=PANEL, fg=INK, selectcolor=BG).grid(row=2, column=0, columnspan=2, sticky="w")
        self.lpf_lbl = tk.Label(g, text="", bg=PANEL, fg=MUTE)
        self.lpf_lbl.grid(row=3, column=0, columnspan=2, sticky="w")

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
        tk.Label(g, text="I/Q level (Q)", bg=PANEL, fg=MUTE).grid(row=8, column=0, sticky="w")
        self.iqg = tk.Scale(g, from_=0, to=50, resolution=1, orient="horizontal", length=180,
                            bg=PANEL, fg=INK, troughcolor=BG, highlightthickness=0,
                            command=self.set_iqgain)
        self.iqg.set(6)
        self.iqg.grid(row=9, column=0, columnspan=2, sticky="we")
        # --- I/Q imbalance correction (B / Y / U) ---
        tk.Label(g, text="I/Q balance (B)", bg=PANEL, fg=MUTE).grid(row=10, column=0, sticky="w")
        bf = tk.Frame(g, bg=PANEL); bf.grid(row=10, column=1, sticky="e")
        for txt, m in (("off", 0), ("auto", 1), ("man", 2)):
            tk.Button(bf, text=txt, width=4, bg=BG, fg=INK,
                      command=lambda mm=m: self.link.write(f"B{mm}")).pack(side="left", padx=1)
        tk.Label(g, text="phase  c1 (×1000)", bg=PANEL, fg=MUTE).grid(row=11, column=0, sticky="w")
        self.iqc1 = tk.Scale(g, from_=-150, to=150, resolution=1, orient="horizontal", length=180,
                             bg=PANEL, fg=INK, troughcolor=BG, highlightthickness=0, command=self.set_iqc1)
        self.iqc1.set(0); self.iqc1.grid(row=12, column=0, columnspan=2, sticky="we")
        tk.Label(g, text="amplitude  c2 (×1000)", bg=PANEL, fg=MUTE).grid(row=13, column=0, sticky="w")
        self.iqc2 = tk.Scale(g, from_=900, to=1100, resolution=1, orient="horizontal", length=180,
                             bg=PANEL, fg=INK, troughcolor=BG, highlightthickness=0, command=self.set_iqc2)
        self.iqc2.set(1000); self.iqc2.grid(row=14, column=0, columnspan=2, sticky="we")
        self.listen_btn = tk.Button(g, text="Listen on PC speakers (demod)", command=self.toggle_listen,
                                    bg=BG, fg=GOOD)
        self.listen_btn.grid(row=15, column=0, columnspan=2, sticky="we", pady=(4, 2))

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

        # --- clock ---
        g = self._group(col, "Clock (RTC)")
        g.pack(fill="x", pady=3)
        tk.Button(g, text="Set from PC time  (C)", command=self.set_clock_from_pc,
                  bg=BG, fg=GOOD).grid(row=0, column=0, columnspan=2, sticky="we", pady=2)
        self.clock_lbl = tk.Label(g, text="(sends the PC date/time to the RTC)", bg=PANEL, fg=MUTE)
        self.clock_lbl.grid(row=1, column=0, columnspan=2, sticky="w")

        # --- screenshot ---
        g = self._group(col, "Device screen")
        g.pack(fill="x", pady=3)
        self.shot_btn = tk.Button(g, text="Screenshot  (p)", command=self.take_screenshot,
                                  bg=BG, fg=GOOD)
        self.shot_btn.grid(row=0, column=0, columnspan=2, sticky="we", pady=2)
        self.shot_lbl = tk.Label(g, text="(reads the LCD back and shows it)", bg=PANEL, fg=MUTE)
        self.shot_lbl.grid(row=1, column=0, columnspan=2, sticky="w")

        # --- raw console ---
        g = self._group(col, "Raw console")
        g.pack(fill="x", pady=3)
        self.raw_var = tk.StringVar()
        re_ = tk.Entry(g, textvariable=self.raw_var, width=18, bg=BG, fg=INK,
                       insertbackground=INK)
        re_.grid(row=0, column=0, sticky="we")
        re_.bind("<Return>", lambda ev: self.send_raw())
        tk.Button(g, text="Send", command=self.send_raw, bg=BG, fg=INK).grid(row=0, column=1)

    def set_clock_from_pc(self):
        import datetime
        if not self.link.ser:
            self.clock_lbl.config(text="not connected - click Connect first")
            return
        n = datetime.datetime.now()
        self.link.write(f"C {n.year} {n.month} {n.day} {n.hour} {n.minute} {n.second}")
        self.clock_lbl.config(text=n.strftime("sent %H:%M:%S %m/%d/%Y - verifying..."))

    # ---- screenshot ---------------------------------------------------- #
    def take_screenshot(self):
        if not self.link.ser:
            self.shot_lbl.config(text="not connected - click Connect first")
            return
        if self._shot_bands is not None:
            return                               # one already in progress
        self._shot_bands = {}
        self._shot_next = 0
        self.shot_btn.config(state="disabled")
        self._shot_request()

    def _shot_request(self):
        self.shot_lbl.config(text=f"reading band {self._shot_next + 1} of {shot.BANDS}...")
        self._shot_t0 = time.time()
        self.link.write(f"p{self._shot_next}")

    def _shot_band(self, y0, data):
        if self._shot_bands is None:
            return                               # a stray band (typed in the raw console)
        self._shot_bands[y0] = data
        self._shot_next += 1
        if self._shot_next < shot.BANDS:
            self._shot_request()
            return
        self._shot_rgb = shot.stitch(self._shot_bands)
        self._shot_end(time.strftime("captured %H:%M:%S"))
        self._shot_show()

    def _shot_end(self, text):
        self._shot_bands = None
        self.shot_btn.config(state="normal")
        self.shot_lbl.config(text=text)

    def _shot_show(self):
        # Viewer window: the frame at 2x, with Capture again / Save PNG.
        if self._shot_win is None or not self._shot_win.winfo_exists():
            w = self._shot_win = tk.Toplevel(self)
            w.title("PSDR screen")
            w.configure(bg=BG)
            self._shot_view = tk.Label(w, bg=BG)
            self._shot_view.pack(padx=10, pady=(10, 6))
            bar = tk.Frame(w, bg=BG)
            bar.pack(fill="x", padx=10, pady=(0, 10))
            tk.Button(bar, text="Capture again", command=self.take_screenshot,
                      bg=PANEL, fg=INK).pack(side="left")
            tk.Button(bar, text="Save PNG...", command=self.save_screenshot,
                      bg=PANEL, fg=GOOD).pack(side="left", padx=6)
            self._shot_saved = tk.Label(bar, text="", bg=BG, fg=MUTE)
            self._shot_saved.pack(side="left", padx=6)
        self._shot_img = tk.PhotoImage(data=shot.ppm_bytes(self._shot_rgb), format="ppm").zoom(2)
        self._shot_view.config(image=self._shot_img)
        self._shot_saved.config(text="")
        self._shot_win.lift()

    def save_screenshot(self):
        from tkinter import filedialog
        if self._shot_rgb is None:
            return
        path = filedialog.asksaveasfilename(
            parent=self._shot_win, defaultextension=".png",
            initialfile=time.strftime("psdr-%Y%m%d-%H%M%S.png"),
            filetypes=[("PNG image", "*.png")])
        if not path:
            return
        try:
            shot.write_png(path, self._shot_rgb)
            self._shot_saved.config(text="saved " + os.path.basename(path))
        except Exception as e:
            self._shot_saved.config(text=f"save failed: {e}")

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
        self._dots = []
        through = self.mux in (2, 3)          # routed through the HMC1023
        y = 46; h = 46
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
        hy = y + 104
        self.box(mx - 10, hy, w + 20, h + 6,
                 "HMC1023 U287\nRF preamp +10dB/ch\n(≈ 20 dB cascade)",
                 active=through, powered=bool(self.lpf_pwr))
        self.arrow(mx + w / 2 - 14, y + h, mx + w / 2 - 14, hy, active=through)
        self.arrow(mx + w / 2 + 14, hy, mx + w / 2 + 14, y + h, active=through)
        c.create_text(xs[4] + w / 2, y + h + 14, text="↑ quad LO (Si5338 ÷ SY10EP52)",
                      fill=MUTE, font=("Segoe UI", 7))

        # --- MCU / DSP enclosure + stage row ---
        en_top = 232
        c.create_rectangle(14, en_top, 986, 396, outline=WIRE, dash=(3, 3))
        c.create_text(22, en_top - 10, anchor="w", fill=ACCENT, font=("Segoe UI", 8, "bold"),
                      text="inside STM32F746  —  DSP")
        y2 = 252; h2 = 52; w2 = 150
        xs2 = [30, 210, 390, 570, 760]
        dlabels = [
            "ADC → DMA\n15× oversample\navg → 48 kHz",
            "FFT 256-pt",
            f"channel filter\nHPF/LPF  {self.flo}‥{self.fhi}",
            f"demod\n{MODE_NAMES.get(self.mode_i, '?')}",
            "USB audio\n(demod) → PC",
        ]
        for i, (x, lbl) in enumerate(zip(xs2, dlabels)):
            self.box(x, y2, w2, h2, lbl)
            if i < len(xs2) - 1:
                self.arrow(x + w2, y2 + h2 / 2, xs2[i + 1], y2 + h2 / 2)
        # ADC (RF row) -> DSP b0, elbow connector through the gap
        ax = xs[6] + w / 2
        yc = 214
        self.arrow(ax, y + h, ax, yc, active=True)
        c.create_line(ax, yc, xs2[0] + w2 / 2, yc, fill=ACCENT, width=3)
        self.arrow(xs2[0] + w2 / 2, yc, xs2[0] + w2 / 2, y2, active=True)
        # I/Q tap branch: b0 -> I/Q balance -> USB I/Q
        by = 324
        self.box(xs2[0], by, w2, h2, "I/Q balance\n(Gram-Schmidt)\nB / Y / U", active=True)
        self.arrow(xs2[0] + w2 / 2, y2 + h2, xs2[0] + w2 / 2, by, active=True)
        self.box(xs2[1] + 40, by, w2, h2, "USB I/Q\n→ HDSDR  (Q)", active=True)
        self.arrow(xs2[0] + w2, by + h2 / 2, xs2[1] + 40, by + h2 / 2, active=True)

        # legend / route
        c.create_text(20, 12, anchor="w", fill=MUTE, font=("Segoe UI", 8),
                      text=f"RX  •  mux {self.mux}: {MUX_NAMES[self.mux]}  •  "
                           f"{'THROUGH HMC1023' if through else 'straight bypass'}")
        c.create_text(980, 12, anchor="e", fill=GOOD if self.lpf_pwr else MUTE,
                      font=("Segoe UI", 8),
                      text=("HMC1023 powered" if self.lpf_pwr else "HMC1023 unpowered"))

        # signal-flow polyline: antenna -> ADC -> (elbow) -> DSP row -> USB audio
        ry = y + h / 2
        flow = [(xs[i] + w / 2, ry) for i in range(len(xs))]
        flow += [(ax, yc), (xs2[0] + w2 / 2, yc), (xs2[0] + w2 / 2, y2 + h2 / 2)]
        flow += [(xs2[j] + w2 / 2, y2 + h2 / 2) for j in range(1, len(xs2))]
        self._flow = flow

        self.draw_meter()

    def _animate(self):
        """Little dots flowing along the active RX signal path."""
        import math
        try:
            c = self.canvas
            for d in self._dots:
                c.delete(d)
            self._dots = []
            pts = self._flow
            if pts and len(pts) >= 2 and self.link.ser:
                seg, total = [], 0.0
                for i in range(len(pts) - 1):
                    dlen = math.hypot(pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1])
                    seg.append((total, total + dlen, pts[i], pts[i + 1]))
                    total += dlen
                self._dot_phase = (self._dot_phase + 0.006) % 1.0
                ndots = 6
                for k in range(ndots):
                    dist = ((self._dot_phase + k / ndots) % 1.0) * total
                    for a, b, p0, p1 in seg:
                        if a <= dist <= b and b > a:
                            t = (dist - a) / (b - a)
                            x = p0[0] + (p1[0] - p0[0]) * t
                            yy = p0[1] + (p1[1] - p0[1]) * t
                            self._dots.append(
                                c.create_oval(x - 3, yy - 3, x + 3, yy + 3, fill=GOOD, outline=""))
                            break
        except Exception:
            pass
        self.after(40, self._animate)

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

    def set_lpf_corner(self):
        i = LPF_CORNERS.index(self.lpfcorner_var.get())
        self.link.write("ka" if i == 0 else f"k{i - 1}")

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

    def set_iqgain(self, _=None):
        self.link.write(f"Q{int(float(self.iqg.get()))}")

    def set_iqc1(self, _=None):
        self.link.write("B2")                                  # moving a knob = manual mode
        self.link.write(f"Y{int(float(self.iqc1.get()))}")

    def set_iqc2(self, _=None):
        self.link.write("B2")
        self.link.write(f"U{int(float(self.iqc2.get()))}")

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
            self.listen_btn.config(text="Listen on PC speakers (demod)", fg=GOOD)
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
        # Never let an exception here kill the loop - always reschedule (in finally),
        # or the whole UI silently freezes (looks like a crash).
        try:
            # drain serial queue
            try:
                while True:
                    kind, payload = self.link.rx.get_nowait()
                    if kind == "line":
                        try:
                            self._parse(payload)
                        except Exception as e:
                            self._log("  [parse err] " + str(e), "err")
                        if payload:
                            self._log("  " + payload, "rx")
                    elif kind == "img":
                        self._shot_band(*payload)
                    elif kind == "imgerr":
                        self._log("! screenshot: " + payload, "err")
                        if self._shot_bands is not None:
                            self._shot_end("screenshot failed - try again")
                    elif kind == "tx":
                        self._log("> " + payload, "tx")
                    elif kind == "err":
                        self._log("! " + payload, "err")
                        self.status_lbl.config(text=payload, fg=HOT)
            except queue.Empty:
                pass
            # periodic poll (held off while a screenshot is streaming)
            now = time.time()
            if self._shot_bands is not None:
                if now - self._shot_t0 > 8.0:
                    self._shot_end("screenshot timed out - try again")
                return
            if self.link.ser and self.poll_var.get() and now - getattr(self, "_last_poll", 0) > 0.7:
                self._last_poll = now
                self.link.write("s")
            if self.link.ser and now - getattr(self, "_last_cpoll", 0) > 1.5:
                self._last_cpoll = now
                self.link.write("c")          # poll the device RTC for the live clock display
                self.link.write("k")          # and the LPF corner actually in use (auto follows the dial)
            # reflect ffplay exiting on its own
            if self.listen_btn["text"] == "Stop listening" and not self.audio.alive():
                self.listen_btn.config(text="Listen on PC speakers (demod)", fg=GOOD)
        except Exception as e:
            try: self._log("  [pump err] " + str(e), "err")
            except Exception: pass
        finally:
            self.after(120, self._pump)

    def _parse(self, line):
        if line.startswith("rtc:"):          # live device clock (from the 'c' poll)
            self.clock_lbl.config(text="device: " + line[4:].strip())
            return
        m = LPFC_RE.search(line)             # reply to k / the periodic k poll
        if m:
            setting, coarse, ontx = int(m.group(1)), int(m.group(2)), int(m.group(3))
            self.lpfcorner_var.set(LPF_CORNERS[setting + 1] if -1 <= setting <= 9 else "off")
            self.lpftx_var.set(bool(ontx))
            self.lpf_lbl.config(text="in use: " + (LPF_CORNERS[coarse + 1] if 0 <= coarse <= 8 else "no filter"))
            return
        if line.startswith("lpf="):
            try:
                self.lpfmode_var.set(LPF_MODES[int(line[4:].split()[0])])
            except Exception:
                pass
            return
        m = STATUS_RE.search(line)
        if m:
            g = m.groups()
            (rms, bar, s, vfo, filt, mux, amp, pol, trx, txrf, imp, reg2) = g[:12]
            mode, flo, fhi = g[12], g[13], g[14]
            self.bar = int(bar)
            # sync the big readout from the firmware, unless the user just tuned
            if time.time() - self._last_tune > 1.0:
                try:
                    hz = int(vfo)
                    if hz != self.vfo_hz and 2000000 <= hz <= 175000000:
                        self.vfo_hz = hz
                        self._refresh_freq()
                        self.vfo_var.set(f"{hz / 1e6:.6f}")
                except Exception:
                    pass
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
