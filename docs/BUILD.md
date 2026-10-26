# Building and flashing

## Install once

```
uv tool install --python 3.13 platformio
```

Not Homebrew: its PlatformIO runs on Python 3.15, which the pioarduino platform refuses (it accepts 3.10 to 3.14). `uv` is already installed through Homebrew.

macOS 10.15 and later ships a driver for the board's CP2102 USB-serial chip.
Plug the board in and check for a port before installing anything:

```
ls /dev/cu.usbserial*
```

No port means a charge-only cable, or a driver problem. Try another cable
first; then install the Silicon Labs CP210x VCP driver.

## Commands

One PlatformIO environment per firmware phase. `-e` picks it; `phase3`, the
full board, is the default when `-e` is omitted. Uploading firmware leaves the LittleFS clips
in place, and `uploadfs` leaves the firmware in place.

| Task | Command |
|---|---|
| Build | `pio run -e phase1` |
| Build and flash | `pio run -e phase1 -t upload` |
| Serial monitor | `pio device monitor` |
| Flash then monitor | `pio run -e phase1 -t upload && pio device monitor` |
| Upload WAV clips in `data/` to LittleFS (phase 2+) | `pio run -t uploadfs` |
| Live touch plot with a summary per touch (phase 1 or 2), logged to `logs/latest.log` | `uv run tools/plot_touch.py` |
| Serial monitor that logs to `logs/latest.log` | `uv run tools/monitor.py` |
| Convert recordings in `clips/` to `data/*.wav` | `tools/make_clips.sh` (add `--say` for spoken placeholders) |
| Flash the full board firmware and clips | `pio run -t uploadfs && pio run -t upload` |
| List serial ports | `pio device list` |

In the plotter's terminal, one character plus Enter goes to the board (`c`
recalibrates); a longer line is logged as a note, to label a test setup.

`pio device monitor`, `tools/monitor.py` and the plotter can't share the port:
quit one before starting another. The two `tools/` programs reset the board when
they open it. Ctrl-C quits the monitor.

If upload fails with "Failed to connect", hold the board's BOOT button while
`Connecting...` prints, then release it.

## Platform and core

`platformio.ini` points at the pioarduino fork of the espressif32 platform,
which ships arduino-esp32 core 3.x. PlatformIO's own espressif32 platform
stopped at core 2.0.17. Phase 2's `ESP_I2S` API only exists in 3.x.

The installed core is 3.3.12 on ESP-IDF 5.5, which uses the new touch
driver: `touchRead()` returns a value the driver has already filtered,
refreshed every 10 ms, and `touchSetConfig()` (not `touchSetCycles()`) sets
the measurement window and charge voltages. Readings still drop on touch.

## Partition table

`partitions.csv` is a custom table for the 4 MB WROOM-32: no OTA, a 2 MB app
slot and a 1.9 MB LittleFS partition. At 16 kHz mono 16-bit a clip is 32 KB
per second, so the filesystem holds about 60 s of clips. `huge_app` was
rejected because its 960 KB filesystem is tighter for no gain: the app will
not approach 3 MB.

## Layout

```
include/config.h          every tuning constant, pin and the pad table
include/secrets.h         WiFi credentials, git-ignored (copy from secrets.h.example)
lib/TouchPad/             shared touch detection used by phase 1 and phase 3
lib/WavPlayer/            WAV streaming from LittleFS to the I2S amp
src/phase0_hello/         phase 0 serial hello
src/phase1_pawtest/       phase 1 one-pad paw test
src/phase2_audio/         phase 2 playback and brownout test
src/phase3_board/         phase 3 full board, all pads
clips/                    source recordings, any format, git-ignored
tools/plot_touch.py       live plot of the pad, one summary line per touch
tools/serial_common.py    port reset and session-log helpers for the tools
tools/monitor.py          serial monitor that logs
tools/make_clips.sh       clips/ to 16 kHz mono WAV in data/
logs/                     plotter session logs, git-ignored
data/                     converted clips for LittleFS, generated, git-ignored
```
