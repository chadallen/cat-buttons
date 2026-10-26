// Phase 3: the full board. Every pad in PADS plays its clip on a touch, with
// per-pad dwell, cooldown, baseline drift and stuck recalibration, a board
// lockout when LOCKOUT_PAD_COUNT pads read touched at once, and one clip at a
// time. Logs one # TRIGGER line per press and the reset reason at boot.
//
// Serial keys:  1-6  play that pad's clip          r    repeat test: first clip, REPEAT_TEST_COUNT times
//               +/-  volume up/down                l    list files on LittleFS
//               c    recalibrate every pad         p/P  toggle / turn on the plot stream for one pad
//               n    plot the next pad             a    toggle the all-pads stream
//               d/D  dwell shorter / longer        t/T  threshold lower / higher (higher = lighter touch)
//               ?    print touch settings
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_system.h>

#include "TouchPad.h"
#include "WavPlayer.h"
#include "config.h"

static const TouchTuning TUNING = {
    TOUCH_RATIO, RELEASE_RATIO, SMOOTH_ALPHA, BASELINE_ALPHA,
    DWELL_MS,    COOLDOWN_MS,   STUCK_RECAL_MS, BASELINE_SAMPLES,
};

static TouchPad* pads[PAD_COUNT];
static WavPlayer player;
static TouchTuning tuning = TUNING;
static float volume = VOLUME;
static uint32_t playCount = 0;
static bool plotting = false;
static bool allStream = DEBUG_STREAM;
static size_t plotPad = 0;
static bool lockout = false;

static const char* resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WATCHDOG";
    case ESP_RST_TASK_WDT: return "TASK_WATCHDOG";
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    default: return "OTHER";
  }
}

static void play(const char* path) {
  digitalWrite(LED_PIN, HIGH);
  PlayResult r = player.play(path, volume);
  digitalWrite(LED_PIN, LOW);
  playCount++;
  if (r.ok) {
    Serial.printf("# PLAY %lu %s ok ms=%lu vol=%.2f count=%lu\n", (unsigned long)millis(), path,
                  (unsigned long)r.elapsedMs, volume, (unsigned long)playCount);
  } else {
    Serial.printf("# PLAY %lu %s FAILED: %s\n", (unsigned long)millis(), path, r.error);
  }
}

static void printPads() {
  for (size_t i = 0; i < PAD_COUNT; i++) {
    Serial.printf("# pad %u %s GPIO%u baseline %.1f touch<%.1f clip %s%s\n", (unsigned)(i + 1), pads[i]->name(),
                  pads[i]->pin(), pads[i]->baseline(), pads[i]->touchThreshold(), PADS[i].clip,
                  LittleFS.exists(PADS[i].clip) ? "" : " (MISSING)");
  }
}

static void printTuning() {
  Serial.printf("# TUNE touch<%.0f%% of base  release>%.0f%%  dwell %lu ms  cooldown %lu ms  lockout %u pads  "
                "(copy to TOUCH_RATIO/RELEASE_RATIO/DWELL_MS in config.h to keep)\n",
                tuning.touchRatio * 100, tuning.releaseRatio * 100, (unsigned long)tuning.dwellMs,
                (unsigned long)tuning.cooldownMs, LOCKOUT_PAD_COUNT);
}

static void applyTuning() {
  for (size_t i = 0; i < PAD_COUNT; i++) pads[i]->setTuning(tuning);
  printTuning();
}

static void stepRatio(float delta) {
  float gap = RELEASE_RATIO - TOUCH_RATIO;
  tuning.touchRatio = constrain(tuning.touchRatio + delta, RATIO_MIN, RATIO_MAX);
  tuning.releaseRatio = min(tuning.touchRatio + gap, 0.97f);
  applyTuning();
}

static void stepDwell(int32_t delta) {
  int32_t d = (int32_t)tuning.dwellMs + delta;
  tuning.dwellMs = (uint32_t)constrain(d, 0, (int32_t)DWELL_MAX_MS);
  applyTuning();
}

static void listFiles() {
  File root = LittleFS.open("/");
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    Serial.printf("# file /%s %u bytes\n", f.name(), (unsigned)f.size());
  }
  Serial.printf("# %u of %u bytes used\n", (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
}

static void calibrateAll() {
  Serial.println("# calibrating: keep everything away from the pads");
  for (size_t i = 0; i < PAD_COUNT; i++) pads[i]->recalibrate();
  printPads();
}

static void announcePlotPad() {
  Serial.printf("# plot pad %s\n", pads[plotPad]->name());
}

static void repeatTest() {
  Serial.printf("# REPEAT start: %s x%u at vol=%.2f\n", PADS[0].clip, REPEAT_TEST_COUNT, volume);
  for (uint8_t i = 0; i < REPEAT_TEST_COUNT; i++) {
    play(PADS[0].clip);
    delay(REPEAT_TEST_GAP_MS);
  }
  Serial.println("# REPEAT done: no reset");
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  delay(300);
  Serial.println();
  esp_reset_reason_t why = esp_reset_reason();
  Serial.printf("# cat-buttons phase3 board: %u pads, reset reason %s\n", (unsigned)PAD_COUNT, resetReasonName(why));
  if (why == ESP_RST_BROWNOUT) {
    Serial.println("# WARNING: last reset was a BROWNOUT. Lower VOLUME or see HARDWARE.md Power.");
  }

  if (!LittleFS.begin(false)) {
    Serial.println("# LittleFS mount FAILED: run `pio run -t uploadfs`");
  }
  if (!player.begin(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN, AUDIO_SAMPLE_RATE)) {
    Serial.println("# I2S init FAILED");
  }

  touchSetConfig(TOUCH_MEASURE_MS, TOUCH_VOLT_LOW, TOUCH_VOLT_HIGH);
  for (size_t i = 0; i < PAD_COUNT; i++) {
    pads[i] = new TouchPad(PADS[i].pin, PADS[i].name);
    pads[i]->begin(tuning);
  }
  printPads();
  printTuning();
  Serial.println("# ready. keys: 1-6 play, c recalibrate, n next plot pad, d/D dwell, t/T threshold, ? settings");
}

static void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c >= '1' && c < '1' + (char)PAD_COUNT) {
      play(PADS[c - '1'].clip);
    } else if (c == 'r') {
      repeatTest();
    } else if (c == '+' || c == '-') {
      volume = constrain(volume + (c == '+' ? VOLUME_STEP : -VOLUME_STEP), 0.0f, 1.0f);
      Serial.printf("# volume %.2f (set VOLUME in config.h to keep it)\n", volume);
    } else if (c == 'l') {
      listFiles();
    } else if (c == 'c') {
      calibrateAll();
    } else if (c == 'P') {
      plotting = true;
      announcePlotPad();
    } else if (c == 'p') {
      plotting = !plotting;
      Serial.printf("# plot %s\n", plotting ? "on" : "off");
      if (plotting) announcePlotPad();
    } else if (c == 'n') {
      plotPad = (plotPad + 1) % PAD_COUNT;
      announcePlotPad();
    } else if (c == 'a') {
      allStream = !allStream;
    } else if (c == 'd' || c == 'D') {
      stepDwell(c == 'd' ? -(int32_t)DWELL_STEP_MS : (int32_t)DWELL_STEP_MS);
    } else if (c == 't' || c == 'T') {
      stepRatio(c == 't' ? -RATIO_STEP : RATIO_STEP);
    } else if (c == '?') {
      printTuning();
      printPads();
    }
  }
}

static void printAllPads() {
  // Arduino serial-plotter format: one label per pad for raw and baseline.
  for (size_t i = 0; i < PAD_COUNT; i++) {
    Serial.printf("%s%s:%lu,%s_base:%.0f", i ? "," : "", pads[i]->name(), (unsigned long)pads[i]->raw(),
                  pads[i]->name(), pads[i]->baseline());
  }
  Serial.println();
}

void loop() {
  static uint32_t nextSampleMs = 0;
  static uint32_t nextPlotMs = 0;
  handleSerial();
  uint32_t now = millis();

  if ((int32_t)(now - nextPlotMs) >= 0 && (plotting || allStream)) {
    nextPlotMs = now + PLOT_INTERVAL_MS;
    if (allStream) {
      printAllPads();
    } else {
      pads[plotPad]->printPlot(Serial);
    }
  }

  if ((int32_t)(now - nextSampleMs) < 0) return;
  nextSampleMs = now + SAMPLE_INTERVAL_MS;

  int fired = -1;
  uint8_t down = 0;
  for (size_t i = 0; i < PAD_COUNT; i++) {
    bool trig = pads[i]->update(now);
    if (trig && fired < 0) fired = (int)i;
    if (pads[i]->belowThreshold() || pads[i]->touched()) down++;
  }

  if (!lockout && down >= LOCKOUT_PAD_COUNT) {
    lockout = true;
    Serial.printf("# LOCKOUT %lu: %u pads touched, ignoring all until they clear\n", (unsigned long)now, down);
  } else if (lockout && down == 0) {
    lockout = false;
    Serial.printf("# LOCKOUT %lu cleared\n", (unsigned long)now);
  }

  // One clip at a time: playback blocks, so touches during it are dropped.
  if (fired >= 0 && !lockout) {
    TouchPad* p = pads[fired];
    Serial.printf("# TRIGGER %lu %s raw=%lu base=%.1f\n", (unsigned long)now, p->name(), (unsigned long)p->raw(),
                  p->baseline());
    play(PADS[fired].clip);
  }
}
