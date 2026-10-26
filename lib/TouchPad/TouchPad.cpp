#include "TouchPad.h"

TouchPad::TouchPad(uint8_t pin, const char* name) : pin_(pin), name_(name) {}

void TouchPad::begin(const TouchTuning& tuning) {
  tuning_ = tuning;
  recalibrate();
}

void TouchPad::recalibrate() {
  // Throw away the first read: the peripheral settles on first use.
  touchRead(pin_);
  delay(5);
  uint32_t sum = 0;
  for (uint16_t i = 0; i < tuning_.baselineSamples; i++) {
    sum += touchRead(pin_);
    delay(5);
  }
  baseline_ = (float)sum / tuning_.baselineSamples;
  smooth_ = baseline_;
  raw_ = (uint32_t)baseline_;
  touched_ = false;
  below_ = false;
}

void TouchPad::printPlot(Print& out) const {
  out.printf("raw:%lu,smooth:%.1f,base:%.1f,thresh:%.1f,touched:%.0f\n", (unsigned long)raw_, smooth_, baseline_,
             touchThreshold(), touched_ ? baseline_ : 0.0f);
}

bool TouchPad::update(uint32_t nowMs) {
  raw_ = touchRead(pin_);
  smooth_ += tuning_.smoothAlpha * ((float)raw_ - smooth_);

  bool trigger = false;

  if (!touched_) {
    bool nowBelow = smooth_ < touchThreshold();
    if (nowBelow && !below_) {
      belowSinceMs_ = nowMs;
    }
    below_ = nowBelow;

    if (below_) {
      if (nowMs - belowSinceMs_ >= tuning_.dwellMs) {
        touched_ = true;
        touchedSinceMs_ = nowMs;
        bool cooled = !everTriggered_ || (nowMs - lastTriggerMs_ >= tuning_.cooldownMs);
        if (cooled) {
          trigger = true;
          lastTriggerMs_ = nowMs;
          everTriggered_ = true;
        }
      }
    } else {
      // Clearly untouched: let the baseline follow slow environmental drift.
      baseline_ += tuning_.baselineAlpha * (smooth_ - baseline_);
    }
  } else {
    if (smooth_ > releaseThreshold()) {
      touched_ = false;
      below_ = false;
    } else if (nowMs - touchedSinceMs_ >= tuning_.stuckRecalMs) {
      // Held this long it's a moved cover, a wet pad or a sleeping cat, not a
      // press. Adopt the current reading as normal so the pad isn't dead.
      baseline_ = smooth_;
      touched_ = false;
      below_ = false;
    }
  }
  return trigger;
}
