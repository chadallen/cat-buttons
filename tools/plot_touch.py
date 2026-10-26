# /// script
# requires-python = ">=3.10"
# dependencies = ["pyserial>=3.5", "matplotlib>=3.8"]
# ///
"""Live plot of the pad touch stream. Run: uv run tools/plot_touch.py [/dev/cu.usbserial-XXXX]

Resets the board, asks it to stream (works with phase 1, 2 and 3 firmware)
and draws the last WINDOW_S seconds of one pad; on phase 3, n + Enter moves
to the next pad. After each touch it prints how deep the
touch went and whether it triggered. In the terminal, a single character plus
Enter goes to the board (c recalibrates); anything longer is logged as a note,
so each test setup can be labeled. Everything goes to logs/plot-<date>-<time>.log
and logs/latest.log. Close the window or Ctrl-C to quit.
"""
import math
import select
import sys
import time
from collections import deque

import matplotlib.pyplot as plt

from serial_common import BAUD, find_port, open_board, open_log

WINDOW_S = 20.0
FIELDS = ["raw", "smooth", "base", "thresh", "touched"]
DIP_START = 0.95       # a touch episode starts when smooth falls under 95% of base
MIN_EPISODE_S = 0.04   # shorter wiggles are noise, not touches
STREAM_FALLBACK_S = 2.5


class TouchTracker:
    """Turns the sample stream into one summary line per touch."""

    def __init__(self):
        self.ep = None
        self.last = "no touches yet"

    def trigger(self):
        if self.ep:
            self.ep["triggered"] = True

    def sample(self, t, v, dt):
        if self.ep is None:
            if v["smooth"] < DIP_START * v["base"]:
                self.ep = {"start": t, "base": v["base"], "thresh": v["thresh"], "min": v["smooth"],
                           "below_s": 0.0, "triggered": False}
            return None
        ep = self.ep
        ep["min"] = min(ep["min"], v["smooth"])
        if v["smooth"] < ep["thresh"]:
            ep["below_s"] += dt
        if v["smooth"] >= DIP_START * ep["base"]:
            self.ep = None
            dur = t - ep["start"]
            if dur < MIN_EPISODE_S:
                return None
            pct = 100 * ep["min"] / ep["base"]
            thr_pct = 100 * ep["thresh"] / ep["base"]
            if ep["triggered"]:
                verdict = "TRIGGERED"
            elif ep["below_s"] > 0:
                verdict = f"crossed threshold for {ep['below_s'] * 1000:.0f} ms, no trigger (dwell or cooldown)"
            else:
                verdict = "never crossed threshold"
            self.last = f"last touch: {pct:.0f}% of base, {verdict.split(',')[0]}"
            return (f"touch at {ep['start']:.1f}s: lowest {ep['min']:.0f} = {pct:.0f}% of base {ep['base']:.0f} "
                    f"(threshold {thr_pct:.0f}%), {dur * 1000:.0f} ms, {verdict}")
        return None


def main() -> None:
    port = find_port(sys.argv[1] if len(sys.argv) > 1 else None)
    # Keep clear of the pad for the first second: the board calibrates at boot.
    ser = open_board(port)
    log_path, log = open_log("plot")
    print(f"reading {port} at {BAUD}; board reset, keep clear of the pad for a second.")
    print("keys + Enter: c recalibrate, n next pad (phase 3), d/D dwell -/+, t/T threshold -/+ (T = lighter touch), ? settings.")
    print("A longer line + Enter is logged as a note. Logging to", log_path)

    t0 = time.monotonic()
    times: deque = deque()
    series = {f: deque() for f in FIELDS}
    tracker = TouchTracker()
    stream_requested = False
    plotted = None  # pad name the firmware says it is plotting; None means the only pad
    last_sample_t = None

    def stamp():
        return f"{time.monotonic() - t0:.3f}"

    def request_stream():
        nonlocal stream_requested
        ser.write(b"P")
        log.write(f"{stamp()} > P\n")
        stream_requested = True

    # plt.pause() re-shows the figure, which on macOS raises it above every
    # other window each loop. Show once, then only pump the event loop.
    plt.rcParams["figure.raise_window"] = False
    plt.ion()
    fig, ax = plt.subplots(figsize=(10, 5))
    lines = {}
    for f, style in zip(FIELDS, ["-", "-", "--", ":", "-"]):
        (lines[f],) = ax.plot([], [], style, label=f, linewidth=2.5 if f == "touched" else 1.2)
    ax.set_xlabel("seconds")
    ax.set_ylabel("touchRead counts (drops on touch)")
    ax.legend(loc="upper left")
    ax.grid(True, alpha=0.3)
    fig.canvas.manager.set_window_title(f"pad touch: {port}")
    plt.show(block=False)

    last_draw = 0.0
    while plt.fignum_exists(fig.number):
        if select.select([sys.stdin], [], [], 0)[0]:
            text = sys.stdin.readline().strip()
            if len(text) == 1:
                ser.write(text.encode())
                log.write(f"{stamp()} > {text}\n")
            elif text:
                log.write(f"{stamp()} NOTE {text}\n")
                print(f"noted: {text}")

        if not stream_requested and time.monotonic() - t0 > STREAM_FALLBACK_S:
            request_stream()

        raw = ser.readline()
        if raw:
            line = raw.decode(errors="replace").strip()
            if line:
                log.write(f"{stamp()} {line}\n")
            if line.startswith("#"):
                print(line)
                if line.startswith("# plot pad "):
                    plotted = line.split()[-1]
                    # A new pad has its own baseline: start the trace and touch tracking fresh.
                    tracker = TouchTracker()
                    times.clear()
                    for f in FIELDS:
                        series[f].clear()
                    last_sample_t = None
                if "TRIGGER" in line:
                    parts = line.split()
                    if plotted is None or (len(parts) > 3 and parts[3] == plotted):
                        tracker.trigger()
                if not stream_requested and line.startswith("# ready"):
                    request_stream()
            elif ":" in line:
                # A line cut short by the reset or the OS buffer lacks fields; drop it.
                try:
                    vals = {k: float(v) for k, v in (kv.split(":") for kv in line.split(","))}
                except ValueError:
                    vals = {}
                if all(f in vals and math.isfinite(vals[f]) for f in FIELDS):
                    now = time.monotonic() - t0
                    dt = 0.0 if last_sample_t is None else min(now - last_sample_t, 0.1)
                    last_sample_t = now
                    summary = tracker.sample(now, vals, dt)
                    if summary and plotted:
                        summary = f"{plotted} {summary}"
                    if summary:
                        print(summary)
                        log.write(f"{stamp()} ~ {summary}\n")
                    times.append(now)
                    for f in FIELDS:
                        series[f].append(vals[f])
                    while times and now - times[0] > WINDOW_S:
                        times.popleft()
                        for f in FIELDS:
                            series[f].popleft()

        if time.monotonic() - last_draw > 0.1 and times:
            last_draw = time.monotonic()
            for f in FIELDS:
                lines[f].set_data(times, series[f])
            ax.set_xlim(max(0.0, times[-1] - WINDOW_S), times[-1] + 0.5)
            top = max(max(series["raw"]), max(series["base"])) * 1.15
            ax.set_ylim(0, max(top, 10))
            ax.set_title(f"{plotted or 'pad'}   base {series['base'][-1]:.0f}   threshold {series['thresh'][-1]:.0f}   |   {tracker.last}",
                         fontsize=10)
            fig.canvas.draw_idle()
            fig.canvas.flush_events()
        else:
            fig.canvas.start_event_loop(0.001)

    ser.close()
    log.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
