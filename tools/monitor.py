# /// script
# requires-python = ">=3.10"
# dependencies = ["pyserial>=3.5"]
# ///
"""Serial monitor that logs. Run: uv run tools/monitor.py [/dev/cu.usbserial-XXXX]

Resets the board, prints every line it sends, and logs each one with seconds
since start to logs/serial-<date>-<time>.log (logs/latest.log points at it).
Type keys and Enter to send them to the board. Ctrl-C quits.
"""
import select
import sys
import time

from serial_common import BAUD, find_port, open_board, open_log


def main() -> None:
    port = find_port(sys.argv[1] if len(sys.argv) > 1 else None)
    ser = open_board(port)
    log_path, log = open_log("serial")
    print(f"reading {port} at {BAUD}; board reset. Type keys + Enter to send. Logging to {log_path}")
    t0 = time.monotonic()
    try:
        while True:
            if select.select([sys.stdin], [], [], 0)[0]:
                keys = sys.stdin.readline().strip()
                if keys:
                    ser.write(keys.encode())
                    log.write(f"{time.monotonic() - t0:.3f} > {keys}\n")
            raw = ser.readline()
            if raw:
                line = raw.decode(errors="replace").rstrip()
                if line:
                    print(line)
                    log.write(f"{time.monotonic() - t0:.3f} {line}\n")
    finally:
        ser.close()
        log.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
