// Phase 2: audio. Plays WAV clips from LittleFS through the MAX98357A, both
// on serial command and when the phase 1 pad on GPIO4 triggers. Logs the
// reset reason at boot so a brownout during playback shows in the log.
//
// Event lines:  # PLAY 12345 /twine.wav ok rate=16000 samples=9000 ms=571 vol=0.50
// Serial keys:  1-6  play that pad's clip      r  repeat test: first clip, REPEAT_TEST_COUNT times
//               +/-  volume up/down            l  list files on LittleFS
//               c    recalibrate the pad           p/P  toggle / turn on the plot stream
//               d/D  dwell shorter / longer        t/T  threshold lower / higher (higher = lighter touch)
//               ?    print touch settings#include <Arduino.h>
#include <LittleFS.h>
#include <esp_system.h>

#include "TouchPad.h"
#include "WavPlayer.h"
#include "config.h"

static const TouchTuning TUNING = {
    TOUCH_RATIO, RELEASE_RATIO, SMOOTH_ALPHA, BASELINE_ALPHA,
    DWELL_MS,    COOLDOWN_MS,   STUCK_RECAL_MS, BASELINE_SAMPLES,
};

static TouchPad pad(PADS[0].pin, PADS[0].name);
static WavPlayer player;
static float volume = VOLUME;
static uint32_t playCount = 0;
static bool plotting = DEBUG_STREAM;
static TouchTuning tuning = TUNING;

static const char* resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WATCHDOG";
    case ESP_RST_TASK_WDT: return "TASK_WATCHDOG";
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

static void play(const char* path) {
  digitalWrite(LED_PIN, HIGH);
  PlayResult r = player.play(path, volume);
  digitalWrite(LED_PIN, LOW);
  playCount++;
  if (r.ok) {
    Serial.printf("# PLAY %lu %s ok rate=%lu samples=%lu ms=%lu vol=%.2f count=%lu\n", (unsigned long)millis(), path,
                  (unsigned long)r.sampleRate, (unsigned long)r.samples, (unsigned long)r.elapsedMs, volume,
                  (unsigned long)playCount);
  } else {
    Serial.printf("# PLAY %lu %s FAILED: %s\n", (unsigned long)millis(), path, r.error);
  }
}

static void printTuning() {
  Serial.printf("# TUNE touch<%.0f%% of base (%.1f)  release>%.0f%%  dwell %lu ms  cooldown %lu ms  "
                "(copy to TOUCH_RATIO/RELEASE_RATIO/DWELL_MS in config.h to keep)\n",
                tuning.touchRatio * 100, pad.touchThreshold(), tuning.releaseRatio * 100,
                (unsigned long)tuning.dwellMs, (unsigned long)tuning.cooldownMs);
}

static void stepRatio(float delta) {
  float gap = RELEASE_RATIO - TOUCH_RATIO;
  tuning.touchRatio = constrain(tuning.touchRatio + delta, RATIO_MIN, RATIO_MAX);
  tuning.releaseRatio = min(tuning.touchRatio + gap, 0.97f);
  pad.setTuning(tuning);
  printTuning();
}

static void stepDwell(int32_t delta) {
  int32_t d = (int32_t)tuning.dwellMs + delta;
  tuning.dwellMs = (uint32_t)constrain(d, 0, (int32_t)DWELL_MAX_MS);
  pad.setTuning(tuning);
  printTuning();
}

static void listFiles() {
  File root = LittleFS.open("/");
  size_t n = 0;
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    Serial.printf("# file /%s %u bytes\n", f.name(), (unsigned)f.size());
    n++;
  }
  Serial.printf("# %u files, %u of %u bytes used\n", (unsigned)n, (unsigned)LittleFS.usedBytes(),
                (unsigned)LittleFS.totalBytes());
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
  Serial.printf("# cat-buttons phase2 audio: reset reason %s\n", resetReasonName(why));
  if (why == ESP_RST_BROWNOUT) {
    Serial.println("# WARNING: last reset was a BROWNOUT. Lower VOLUME or see HARDWARE.md Power.");
  }

  if (!LittleFS.begin(false)) {
    Serial.println("# LittleFS mount FAILED: run `pio run -e phase2 -t uploadfs`");
  } else {
    listFiles();
  }

  if (!player.begin(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN, AUDIO_SAMPLE_RATE)) {
    Serial.println("# I2S init FAILED");
  }
  Serial.printf("# I2S BCLK=%u LRC=%u DIN=%u, volume %.2f\n", I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN, volume);

  touchSetConfig(TOUCH_MEASURE_MS, TOUCH_VOLT_LOW, TOUCH_VOLT_HIGH);
  pad.begin(TUNING);
  Serial.printf("# pad %s baseline %.1f touch<%.1f\n", pad.name(), pad.baseline(), pad.touchThreshold());
  printTuning();
  Serial.println("# ready. keys: 1-6 play, r repeat, +/- volume, l list, c recalibrate, p plot, d/D dwell, t/T threshold, ? settings");
}

static void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c >= '1' && c <= '0' + (char)PAD_COUNT) {
      play(PADS[c - '1'].clip);
    } else if (c == 'r') {
      repeatTest();
    } else if (c == '+' || c == '-') {
      volume = constrain(volume + (c == '+' ? VOLUME_STEP : -VOLUME_STEP), 0.0f, 1.0f);
      Serial.printf("# volume %.2f (set VOLUME in config.h to keep it)\n", volume);
    } else if (c == 'l') {
      listFiles();
    } else if (c == 'P') {
      plotting = true;
    } else if (c == 'p') {
      plotting = !plotting;
      Serial.printf("# plot %s\n", plotting ? "on" : "off");
    } else if (c == 'd' || c == 'D') {
      stepDwell(c == 'd' ? -(int32_t)DWELL_STEP_MS : (int32_t)DWELL_STEP_MS);
    } else if (c == 't' || c == 'T') {
      stepRatio(c == 't' ? -RATIO_STEP : RATIO_STEP);
    } else if (c == '?') {
      printTuning();
    } else if (c == 'c') {
      pad.recalibrate();
      Serial.printf("# pad %s baseline %.1f touch<%.1f\n", pad.name(), pad.baseline(), pad.touchThreshold());
    }
  }
}

void loop() {
  static uint32_t nextSampleMs = 0;
  static uint32_t nextPlotMs = 0;
  handleSerial();
  uint32_t now = millis();
  if (plotting && (int32_t)(now - nextPlotMs) >= 0) {
    nextPlotMs = now + PLOT_INTERVAL_MS;
    pad.printPlot(Serial);
  }
  if ((int32_t)(now - nextSampleMs) >= 0) {
    nextSampleMs = now + SAMPLE_INTERVAL_MS;
    if (pad.update(now)) {
      Serial.printf("# TRIGGER %lu %s raw=%lu base=%.1f\n", (unsigned long)now, pad.name(), (unsigned long)pad.raw(),
                    pad.baseline());
      play(PADS[0].clip);
    }
  }
}
