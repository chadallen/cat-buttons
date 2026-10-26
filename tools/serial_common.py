"""Serial port and session-log helpers shared by monitor.py and plot_touch.py.

open_board() releases the CP2102's DTR/RTS lines and pulses EN once so the
board boots fresh; open_log() writes logs/<prefix>-<date>-<time>.log and points
logs/latest.log at it.
"""
import os
import sys
import time
from pathlib import Path

import serial
import serial.tools.list_ports

BAUD = 115200
LOG_DIR = Path(__file__).resolve().parent.parent / "logs"


def find_port(arg: str | None) -> str:
    if arg:
        return arg
    candidates = [p.device for p in serial.tools.list_ports.comports()
                  if "usbserial" in p.device or "SLAB" in p.device or "wchusbserial" in p.device]
    if not candidates:
        sys.exit("no USB serial port found; pass it as an argument, e.g. /dev/cu.usbserial-0001")
    return candidates[0]


def open_board(port: str) -> serial.Serial:
    ser = serial.Serial(port, BAUD, timeout=0.05)
    # The CP2102 wires DTR and RTS to IO0 and EN. Release both, then pulse EN.
    ser.dtr = False
    ser.rts = False
    time.sleep(0.2)
    ser.reset_input_buffer()
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    return ser


def open_log(prefix: str):
    LOG_DIR.mkdir(exist_ok=True)
    path = LOG_DIR / time.strftime(f"{prefix}-%Y%m%d-%H%M%S.log")
    latest = LOG_DIR / "latest.log"
    if latest.is_symlink() or latest.exists():
        latest.unlink()
    os.symlink(path.name, latest)
    # Line-buffered so the file can be read while the session runs.
    return path, open(path, "w", buffering=1)
