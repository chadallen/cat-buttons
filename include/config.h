// Single source of tuning for the Cat Button Board. Logic files include this
// and never hard-code a threshold, time or pin of their own.
#pragma once

#include <stdint.h>
#include <stddef.h>

// ---------------------------------------------------------------- Serial
#define SERIAL_BAUD 115200

// ---------------------------------------------------------------- Touch sampling
// One sample per pad every SAMPLE_INTERVAL_MS. Dwell and cooldown are counted
// in milliseconds, so this only sets resolution, not behavior.
constexpr uint32_t SAMPLE_INTERVAL_MS = 10;

// Touch peripheral setup, passed to touchSetConfig() before the first read.
// The core's driver (arduino-esp32 3.3 on IDF 5.5) scans continuously and
// touchRead() returns its software-filtered value, refreshed every 10 ms.
// A longer measurement window gives a larger count and finer resolution,
// which matters once a cover shrinks the touch delta. 5 ms is the driver
// default. Charge range 0.5 V to 2.7 V is the widest the ESP32 offers and
// gives the biggest touch delta; the driver default tops out at 1.7 V.
constexpr float TOUCH_MEASURE_MS = 5.0f;
#define TOUCH_VOLT_LOW TOUCH_VOLT_LIM_L_0V5
#define TOUCH_VOLT_HIGH TOUCH_VOLT_LIM_H_2V7

// ---------------------------------------------------------------- Detection
// On the original ESP32 touchRead() DROPS on touch.
// touched  when smooth < baseline * TOUCH_RATIO
// released when smooth > baseline * RELEASE_RATIO   (hysteresis)
constexpr float TOUCH_RATIO = 0.70f;
constexpr float RELEASE_RATIO = 0.80f;

// EMA on top of the driver's own filter. 0.5 at 100 Hz adds about 20 ms.
constexpr float SMOOTH_ALPHA = 0.5f;

// Baseline drift, applied per sample only while the pad is clearly untouched.
// 0.0005 at 100 Hz is a time constant of about 20 s: fast enough to follow
// humidity, slow enough that a paw resting lightly nearby doesn't become the
// new normal.
constexpr float BASELINE_ALPHA = 0.0005f;

// Continuous touch required before a trigger. A brush is shorter than this.
constexpr uint32_t DWELL_MS = 120;

// Live tuning keys in the phase 2 sketch: d/D and t/T step these. The gap
// between TOUCH_RATIO and RELEASE_RATIO is kept as the threshold moves.
constexpr uint32_t DWELL_STEP_MS = 20;
constexpr uint32_t DWELL_MAX_MS = 400;
constexpr float RATIO_STEP = 0.05f;
constexpr float RATIO_MIN = 0.40f;
constexpr float RATIO_MAX = 0.90f;

// Per-pad minimum gap between triggers.
constexpr uint32_t COOLDOWN_MS = 2500;

// A pad that reads touched this long is recalibrated, not left stuck.
constexpr uint32_t STUCK_RECAL_MS = 30000;

// Boot calibration: baseline = mean of this many reads, pad untouched.
constexpr uint16_t BASELINE_SAMPLES = 64;

// Board-level lockout: this many pads touched at once means a cat lying
// across the board. Ignore everything until they clear.
constexpr uint8_t LOCKOUT_PAD_COUNT = 3;

// ---------------------------------------------------------------- Pads
// Order is the button number: serial keys 1-6 play them in this order.
// tools/make_clips.sh reads this table for clip names and spoken placeholders
// (the name with _ as a space), so keep one entry per line in this format.
struct PadConfig {
  uint8_t pin;       // must be a touch-capable GPIO; never 0, 2, 12 or 15
  const char* name;  // serial log label
  const char* clip;  // LittleFS path, phase 2+
};

constexpr PadConfig PADS[] = {
    {4, "TWINE", "/twine.wav"},          // 1, T0
    {13, "TREAT", "/treat.wav"},         // 2, T4
    {14, "CAT_DANCER", "/catdancer.wav"}, // 3, T6
    {27, "HOOPS", "/hoops.wav"},         // 4, T7
    {32, "SNUGGLES", "/snuggles.wav"},   // 5, T9
    {33, "CATNIPS", "/catnips.wav"},     // 6, T8
};
constexpr size_t PAD_COUNT = sizeof(PADS) / sizeof(PADS[0]);

// Which entry of PADS the phase 1 single-pad test uses.
constexpr size_t PHASE1_PAD_INDEX = 0;

// ---------------------------------------------------------------- Audio (phase 2+)
constexpr uint8_t I2S_BCLK_PIN = 26;
constexpr uint8_t I2S_LRC_PIN = 25;
constexpr uint8_t I2S_DIN_PIN = 22;

// Software volume, 0.0 to 1.0, applied to samples before I2S. The amp's
// GAIN pin isn't on the NULLLAB cable, so it runs at its fixed 9 dB.
constexpr float VOLUME = 0.75f;
constexpr float VOLUME_STEP = 0.05f;  // per +/- key in the phase 2 sketch

// Clips are converted to this rate by tools/make_clips.sh. A clip at another
// rate still plays; the player reconfigures I2S for it.
constexpr uint32_t AUDIO_SAMPLE_RATE = 16000;

// Phase 2 brownout test: plays the first clip this many times back to back.
constexpr uint8_t REPEAT_TEST_COUNT = 10;
constexpr uint32_t REPEAT_TEST_GAP_MS = 300;

// ---------------------------------------------------------------- Feedback
// The 30-pin board's blue LED. Phase 1 lights it while the pad reads touched.
// GPIO2 is a boot-strapping pin, which is fine for an output the board
// itself wires to an LED; it is never a pad.
constexpr uint8_t LED_PIN = 2;

// ---------------------------------------------------------------- Debug
// 1: stream pad readings for plotting from boot. 0: only when the serial
// key P asks, which tools/plot_touch.py does. Phase 1 always streams.
#define DEBUG_STREAM 0
constexpr uint32_t PLOT_INTERVAL_MS = 20;
