#!/usr/bin/env python3
"""
Floating status UI for xhisper.
Shows idle / listening / processing based on /tmp/xhisper_status.
"""

import json
import os
import subprocess
import time
import tkinter as tk
from tkinter import font as tkFont

STATUS_FILE = "/tmp/xhisper_status"
LAST_WAV = "/tmp/xhisper_last.wav"
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
POLL_MS = 100
MONITOR_POLL_S = 1.0
INTERACTIVE = os.environ.get("XHISPER_UI_INTERACTIVE", "1").lower() not in ("0", "false", "off", "no")

STYLES = {
    "idle": {"text": "xhisper idle", "bg": "#2b2b2b", "fg": "#d0d0d0"},
    "listening": {"text": "listening...", "bg": "#1b5e20", "fg": "#ffffff"},
    "processing": {"text": "processing...", "bg": "#6d4c41", "fg": "#ffffff"},
    "empty": {"text": "no speech", "bg": "#6a4f1f", "fg": "#ffffff"},
}

def read_status():
    try:
        with open(STATUS_FILE, "r", encoding="utf-8") as f:
            value = f.read().strip().lower()
            return value if value in STYLES else "idle"
    except FileNotFoundError:
        return "idle"

last_geom = ""
last_monitor_check = 0.0


def ensure_hypr_signature():
    if os.environ.get("HYPRLAND_INSTANCE_SIGNATURE"):
        return
    runtime = os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")
    hypr_dir = os.path.join(runtime, "hypr")
    if not os.path.isdir(hypr_dir):
        return
    try:
        entries = sorted(os.listdir(hypr_dir))
    except OSError:
        return
    if entries:
        os.environ["HYPRLAND_INSTANCE_SIGNATURE"] = entries[0]


def get_active_monitor():
    ensure_hypr_signature()
    try:
        output = subprocess.check_output(["hyprctl", "-j", "monitors"], text=True).strip()
    except Exception:
        return None
    if not output:
        return None
    try:
        monitors = json.loads(output)
    except json.JSONDecodeError:
        return None
    if not monitors:
        return None
    for mon in monitors:
        if mon.get("focused"):
            return mon
    return monitors[0]


def update_geometry():
    global last_geom, last_monitor_check
    now = time.time()
    if now - last_monitor_check < MONITOR_POLL_S:
        return
    last_monitor_check = now

    mon = get_active_monitor()
    if mon:
        x = mon.get("x", 0) + mon.get("width", 0) - width - 20
        y = mon.get("y", 0) + 20
    else:
        screen_w = root.winfo_screenwidth()
        x = screen_w - width - 20
        y = 20
    geom = f"{width}x{height}+{x}+{y}"
    if geom != last_geom:
        root.geometry(geom)
        last_geom = geom


def update():
    status = read_status()
    style = STYLES.get(status, STYLES["idle"])
    label.config(text=style["text"], bg=style["bg"], fg=style["fg"])
    file_label.config(text=get_last_info(), bg=style["bg"], fg="#e0e0e0")
    container.config(bg=style["bg"])
    root.config(bg=style["bg"])
    update_geometry()
    root.after(POLL_MS, update)

root = tk.Tk()
root.title("xhisper")
root.overrideredirect(True)
root.attributes("-topmost", True)
# Try to keep the window from stealing focus (WM-specific).
try:
    root.wm_attributes("-type", "dock")
except tk.TclError:
    pass
if not INTERACTIVE:
    try:
        root.attributes("-disabled", True)
    except tk.TclError:
        pass

width = 320
height = 110
root.geometry(f"{width}x{height}+0+0")

def get_last_info():
    if not os.path.exists(LAST_WAV):
        return "last audio: none"
    try:
        size_kb = os.path.getsize(LAST_WAV) / 1024.0
    except OSError:
        size_kb = 0.0
    return f"last audio: {size_kb:.0f} KB"


def retry_transcribe():
    if not os.path.exists(LAST_WAV):
        return
    cmd = ["/bin/bash", "-lc", f"XHISPER_RETRY=1 XHISPER_RETRY_PASTE=0 \"{SCRIPT_DIR}/xhisper.sh\""]
    subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def open_audio():
    if not os.path.exists(LAST_WAV):
        return
    if subprocess.call(["/bin/sh", "-lc", "command -v xdg-open >/dev/null 2>&1"]) == 0:
        subprocess.Popen(["xdg-open", LAST_WAV], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


font = tkFont.Font(family="Arial", size=14, weight="bold")
small_font = tkFont.Font(family="Arial", size=9)
container = tk.Frame(root, bg="#2b2b2b")
container.pack(expand=True, fill=tk.BOTH)

label = tk.Label(container, text="", font=font, bg="#2b2b2b", fg="#d0d0d0")
label.pack(side=tk.TOP, expand=False, fill=tk.X, pady=(8, 0))

file_label = tk.Label(container, text=get_last_info(), font=small_font, bg="#2b2b2b", fg="#e0e0e0")
file_label.pack(side=tk.TOP, expand=False, fill=tk.X, pady=(2, 6))

btn_frame = tk.Frame(container, bg="#2b2b2b")
btn_frame.pack(side=tk.TOP, fill=tk.X, padx=10, pady=(0, 8))

retry_btn = tk.Button(btn_frame, text="Retry", command=retry_transcribe, bg="#3d3d3d", fg="#ffffff")
retry_btn.pack(side=tk.LEFT, expand=True, fill=tk.X, padx=(0, 6))

open_btn = tk.Button(btn_frame, text="Open audio", command=open_audio, bg="#3d3d3d", fg="#ffffff")
open_btn.pack(side=tk.LEFT, expand=True, fill=tk.X)

update()
root.mainloop()
