#include "WavPlayer.h"

#include <LittleFS.h>

// Samples read from flash per I2S write: 1 KB in, 2 KB out.
static const size_t CHUNK_SAMPLES = 512;

// The amp plays (L+R)/2. Sending stereo with each sample copied to both
// slots avoids the core's 16-bit mono workaround for the original ESP32.
bool WavPlayer::begin(int8_t bclk, int8_t ws, int8_t dout, uint32_t sampleRate) {
  bclk_ = bclk;
  ws_ = ws;
  dout_ = dout;
  i2s_.setPins(bclk_, ws_, dout_);
  return setRate(sampleRate);
}

bool WavPlayer::setRate(uint32_t rate) {
  if (rate == rate_) return true;
  if (rate_ != 0) i2s_.end();
  i2s_.setPins(bclk_, ws_, dout_);
  if (!i2s_.begin(I2S_MODE_STD, rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    rate_ = 0;
    return false;
  }
  rate_ = rate;
  return true;
}

static uint32_t readU32(File& f) {
  uint8_t b[4] = {};
  f.read(b, 4);
  return b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint16_t readU16(File& f) {
  uint8_t b[2] = {};
  f.read(b, 2);
  return b[0] | (b[1] << 8);
}

PlayResult WavPlayer::play(const char* path, float volume) {
  PlayResult r = {false, nullptr, 0, 0, 0};
  uint32_t t0 = millis();

  File f = LittleFS.open(path, "r");
  if (!f) {
    r.error = "file not found";
    return r;
  }

  char id[5] = {};
  f.read((uint8_t*)id, 4);
  readU32(f);
  char wave[5] = {};
  f.read((uint8_t*)wave, 4);
  if (strcmp(id, "RIFF") != 0 || strcmp(wave, "WAVE") != 0) {
    r.error = "not a WAV file";
    return r;
  }

  // Walk the chunks: ffmpeg and voice-memo exports can put LIST or other
  // chunks between fmt and data.
  uint16_t format = 0, channels = 0, bits = 0;
  uint32_t dataBytes = 0;
  bool haveData = false;
  while (f.available() >= 8) {
    char cid[5] = {};
    f.read((uint8_t*)cid, 4);
    uint32_t size = readU32(f);
    if (strcmp(cid, "fmt ") == 0) {
      format = readU16(f);
      channels = readU16(f);
      r.sampleRate = readU32(f);
      readU32(f);  // byte rate
      readU16(f);  // block align
      bits = readU16(f);
      f.seek(f.position() + size - 16 + (size & 1));
    } else if (strcmp(cid, "data") == 0) {
      dataBytes = size;
      haveData = true;
      break;
    } else {
      f.seek(f.position() + size + (size & 1));
    }
  }
  if (!haveData) {
    r.error = "no data chunk";
    return r;
  }
  if (format != 1 || channels != 1 || bits != 16) {
    r.error = "need 16-bit mono PCM (run tools/make_clips.sh)";
    return r;
  }
  if (!setRate(r.sampleRate)) {
    r.error = "I2S init failed";
    return r;
  }

  int32_t gain = (int32_t)(constrain(volume, 0.0f, 1.0f) * 32768.0f);
  static int16_t in[CHUNK_SAMPLES];
  static int16_t out[CHUNK_SAMPLES * 2];
  uint32_t remaining = dataBytes;
  while (remaining > 0) {
    size_t want = min((uint32_t)sizeof(in), remaining);
    size_t got = f.read((uint8_t*)in, want);
    if (got == 0) break;
    size_t n = got / 2;
    for (size_t i = 0; i < n; i++) {
      int16_t s = (int16_t)(((int32_t)in[i] * gain) >> 15);
      out[2 * i] = s;
      out[2 * i + 1] = s;
    }
    i2s_.write((uint8_t*)out, n * 4);
    remaining -= got;
    r.samples += n;
  }
  f.close();

  r.ok = true;
  r.elapsedMs = millis() - t0;
  return r;
}
