// Touch detection for one capacitive pad on the original ESP32: smoothing,
// a ratio threshold against a drifting baseline with hysteresis, dwell,
// cooldown and stuck-pad recalibration. Phase 1 and phase 3 share this
// class so the paw test exercises the same logic the board ships with.
#pragma once

#include <Arduino.h>

struct TouchTuning {
  float touchRatio;
  float releaseRatio;
  float smoothAlpha;
  float baselineAlpha;
  uint32_t dwellMs;
  uint32_t cooldownMs;
  uint32_t stuckRecalMs;
  uint16_t baselineSamples;
};

class TouchPad {
 public:
  TouchPad(uint8_t pin, const char* name);

  // Blocks for about baselineSamples * 5 ms while it averages the untouched
  // reading. Call with nothing near the pad.
  void begin(const TouchTuning& tuning);

  // Take one sample. Returns true on the sample where a trigger fires: the
  // pad has just crossed from untouched to touched after dwell, and the
  // cooldown since the previous trigger has passed.
  bool update(uint32_t nowMs);

  // Re-average the baseline from the current reading. Blocks like begin().
  void recalibrate();

  // One serial-plotter line: raw, smooth, base, thresh, touched. touched is
  // scaled to the baseline so it shows on the same axis. tools/plot_touch.py
  // reads this format.
  void printPlot(Print& out) const;

  // Swap thresholds and timings without recalibrating. Takes effect on the
  // next sample, so a live tuning key can change sensitivity mid-session.
  void setTuning(const TouchTuning& tuning) { tuning_ = tuning; }
  const TouchTuning& tuning() const { return tuning_; }

  bool touched() const { return touched_; }
  // Under the touch threshold right now, before dwell. The board lockout
  // counts these so a cat lying down is caught before any pad's dwell ends.
  bool belowThreshold() const { return smooth_ < touchThreshold(); }
  uint32_t raw() const { return raw_; }
  float smooth() const { return smooth_; }
  float baseline() const { return baseline_; }
  float touchThreshold() const { return baseline_ * tuning_.touchRatio; }
  float releaseThreshold() const { return baseline_ * tuning_.releaseRatio; }
  const char* name() const { return name_; }
  uint8_t pin() const { return pin_; }

 private:
  uint8_t pin_;
  const char* name_;
  TouchTuning tuning_{};

  uint32_t raw_ = 0;
  float smooth_ = 0;
  float baseline_ = 0;

  bool touched_ = false;
  bool below_ = false;        // smooth is under the touch threshold
  uint32_t belowSinceMs_ = 0; // when it went under, for dwell
  uint32_t touchedSinceMs_ = 0;
  uint32_t lastTriggerMs_ = 0;
  bool everTriggered_ = false;
};
