// Phase 1: one-pad paw test. Streams raw, smoothed, baseline and threshold
// for one pad in serial-plotter format, lights the on-board LED while the
// pad reads touched and logs each trigger, using the same TouchPad logic
// phase 3 will use.
//
// Plot lines:    raw:62,smooth:61.4,base:63.0,thresh:44.1,touched:0
// Event lines:   # TRIGGER 12345 TWINE raw=31 smooth=33.2 base=63.0
// Serial keys:   c  recalibrate baseline (pad untouched)
//                p  pause/resume the plot stream   P  plot stream on
#include <Arduino.h>

#include "TouchPad.h"
#include "config.h"

static const TouchTuning TUNING = {
    TOUCH_RATIO, RELEASE_RATIO, SMOOTH_ALPHA, BASELINE_ALPHA,
    DWELL_MS,    COOLDOWN_MS,   STUCK_RECAL_MS, BASELINE_SAMPLES,
};

static TouchPad pad(PADS[PHASE1_PAD_INDEX].pin, PADS[PHASE1_PAD_INDEX].name);

static bool plotting = true;
static uint32_t triggerCount = 0;

static void printCalibration() {
  Serial.printf("# baseline %.1f  touch<%.1f  release>%.1f  (ratio %.2f/%.2f, dwell %lu ms, cooldown %lu ms)\n",
                pad.baseline(), pad.touchThreshold(), pad.releaseThreshold(), TOUCH_RATIO, RELEASE_RATIO,
                (unsigned long)DWELL_MS, (unsigned long)COOLDOWN_MS);
}

static void calibrate() {
  Serial.println("# calibrating: keep everything away from the pad");
  pad.recalibrate();
  printCalibration();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  delay(300);
  Serial.println();
  Serial.printf("# cat-buttons phase1 paw test: pad %s on GPIO%u, LED on GPIO%u\n", pad.name(), pad.pin(), LED_PIN);

  touchSetConfig(TOUCH_MEASURE_MS, TOUCH_VOLT_LOW, TOUCH_VOLT_HIGH);
  pad.begin(TUNING);
  printCalibration();

  // Two blinks: calibrated and listening.
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
}

static void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    switch (c) {
      case 'c':
        calibrate();
        break;
      case 'P':
        plotting = true;
        break;
      case 'p':
        plotting = !plotting;
        Serial.printf("# plot %s\n", plotting ? "on" : "paused");
        break;
      default:
        break;
    }
  }
}

void loop() {
  static uint32_t nextSampleMs = 0;
  static uint32_t nextPlotMs = 0;
  uint32_t now = millis();

  handleSerial();

  if ((int32_t)(now - nextSampleMs) >= 0) {
    nextSampleMs = now + SAMPLE_INTERVAL_MS;
    if (pad.update(now)) {
      triggerCount++;
      Serial.printf("# TRIGGER %lu %s raw=%lu smooth=%.1f base=%.1f count=%lu\n", (unsigned long)now, pad.name(),
                    (unsigned long)pad.raw(), pad.smooth(), pad.baseline(), (unsigned long)triggerCount);
    }
  }

  digitalWrite(LED_PIN, pad.touched() ? HIGH : LOW);

  if (plotting && (int32_t)(now - nextPlotMs) >= 0) {
    nextPlotMs = now + PLOT_INTERVAL_MS;
    pad.printPlot(Serial);
  }
}
