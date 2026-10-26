// Streams 16-bit mono PCM WAV files from LittleFS to an I2S amp, scaling by a
// volume factor. Playback blocks until the clip ends, so one clip plays at a
// time and anything that happens meanwhile is dropped, not queued.
#pragma once

#include <Arduino.h>
#include <ESP_I2S.h>

struct PlayResult {
  bool ok;
  const char* error;    // set when ok is false
  uint32_t sampleRate;
  uint32_t samples;
  uint32_t elapsedMs;
};

class WavPlayer {
 public:
  bool begin(int8_t bclk, int8_t ws, int8_t dout, uint32_t sampleRate);
  PlayResult play(const char* path, float volume);

 private:
  bool setRate(uint32_t rate);

  I2SClass i2s_;
  int8_t bclk_ = -1, ws_ = -1, dout_ = -1;
  uint32_t rate_ = 0;
};
