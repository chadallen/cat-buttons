# Firmware spec

Build in phases. Each phase ends with a physical test the builder runs; don't start the next phase until it passes.

## Phase 0: Toolchain

- Set up a PlatformIO project (`esp32dev`, Arduino framework) that builds and uploads a blink or serial hello over USB.
- **Pass:** the serial monitor shows output from the board.

## Phase 1: One-pad paw test (go/no-go for the whole design)

A single sketch for one pad on GPIO4:
- Stream the raw `touchRead()` value, a smoothed value and the baseline to serial as `label:value,...` lines. The Arduino IDE serial plotter graphs that format; PlatformIO has no plotter, so `tools/plot_touch.py` draws it instead.
- Light the on-board LED (GPIO2) while the pad registers as touched and log one `# TRIGGER` line per trigger, using the same detection logic as phase 3 (threshold and dwell).
- Take thresholds from a config header. Calibrate the baseline at boot (pad untouched) as an average of N samples.

On the original ESP32, `touchRead()` **drops** on touch. Detect touch as `value < baseline * TOUCH_RATIO`, with `TOUCH_RATIO` starting around 0.7. Use hysteresis on release.

**Pass:**
- A deliberate paw step on the covered pad triggers reliably.
- The cat standing, sitting or lying next to the puck doesn't trigger it.
- If paws don't separate cleanly from a finger at any threshold, stop and report it. The fallback is an FSR (see DECISIONS.md).

**Test protocol:** 10 deliberate paw steps on the covered pad, at least 9 trigger. Then 5 minutes with the cat sitting or lying within 5 cm of the puck, 0 triggers. Count `# TRIGGER` lines in the log.

## Phase 2: Audio

- Play a 16-bit mono WAV from LittleFS through the MAX98357A on I2S pins 26, 25 and 22. **Chosen: ESP_I2S** from the 3.x core. It ships with the core, and plain PCM WAV needs no decoder, which is the main thing ESP8266Audio adds. `lib/WavPlayer` parses the WAV chunks and streams them.
- Add a software volume setting in the config header.
- Write a small script, run on a Mac, that converts recorded clips (m4a, wav, whatever the voice memo app makes) to 16 kHz mono 16-bit WAV with loudness normalized. Use ffmpeg. Upload them with `pio run -t uploadfs`. Done as `tools/make_clips.sh`; `--say` makes spoken placeholders with the Mac's voice.
- **Test sketch** (`src/phase2_audio`): serial keys 1–6 play a pad's clip, `r` runs the ten-play brownout test, `+`/`-` adjust volume, `p`/`P` toggle or start the pad plot stream. The GPIO4 pad also plays its clip.
- **Live touch tuning** (phase 2 sketch): `d`/`D` step the dwell by 20 ms, `t`/`T` step the touch threshold by 5% of baseline with the release point following, `?` prints the current values. Changes last until reset; copy the printed values into `include/config.h` to keep them. Use this during the cat test instead of reflashing. Boot prints the reset reason, so a brownout reset shows as `BROWNOUT`.
- **Pass:**
  - A clip plays cleanly at room volume.
  - Ten plays in a row at max intended volume cause no reset (check for brownouts in the serial log).

## Phase 3: Full v0 firmware

Built as `src/phase3_board`. Keys: 1–6 play a clip, `c` recalibrates every pad, `n` moves the plot to the next pad, `a` streams every pad's raw value and baseline for the Arduino plotter, and the phase 2 volume, repeat-test and tuning keys work here too. The lockout counts pads under threshold before dwell, so a cat lying down is caught before any single pad's dwell ends.

- **Pads:** 6, mapped to clips by a table in the config, e.g. `{4, "TWINE", "/twine.wav"}`. Adding, removing or renaming a pad must be a config edit only. `tools/make_clips.sh` reads the same table.
- **Per-pad detection:**
  - Smoothing.
  - A threshold ratio against the baseline, with hysteresis.
  - **Dwell:** about 120 ms of continuous touch before triggering.
  - **Cooldown:** about 2.5 s per pad before it can trigger again.
- **Lockout:** if 3 or more pads read as touched at once (the cat lying across the board), ignore all of them until they clear.
- **Baseline drift:** update the baseline slowly only while a pad is untouched, so humidity and temperature changes don't cause false triggers or dead pads. A pad held "touched" for more than about 30 s should recalibrate rather than stay stuck.
- **One clip at a time:** a new trigger during playback is dropped, not queued.
- **Serial log:** one line per press, `millis, pad name, raw value, baseline`.
- **Debug mode,** toggled by a config flag or a serial command: stream all pads' raw values and baselines for plotting.
- **Pass:**
  - Each pad plays its word when touched.
  - Brushing a puck or lying across the board doesn't trigger.
  - It runs overnight with no false triggers in the log.

## Phase 4 (after v0 is stable, optional): Press logging

- Persist presses with a timestamp (NTP over WiFi) to a LittleFS CSV, served by a tiny web page on the LAN listing recent presses and a per-pad count.
- WiFi credentials live in a git-ignored secrets header.
- **Purpose:** after two weeks, answer whether presses correlate with anything, before deciding what v1 needs.

## Config header (single source of tuning)

`TOUCH_RATIO`, `RELEASE_RATIO`, `DWELL_MS`, `COOLDOWN_MS`, `LOCKOUT_PAD_COUNT`, `BASELINE_ALPHA`, `STUCK_RECAL_MS`, `VOLUME`, `DEBUG_STREAM`, and the pad-to-pin-to-clip table.
