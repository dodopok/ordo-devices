#include "audio.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "board_pins.h"

#if BOARD_HAS_SPEAKER
#include <ESP_I2S.h>
#endif

namespace board {

#if BOARD_HAS_SPEAKER
namespace {

constexpr uint32_t kSampleRate = 16000;

bool writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(I2C_ADDR_CODEC);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// The ES8311 as a playback-only slave at 16 kHz with MCLK = 256 x fs from
// the MCLK pin. The register values are the ones Espressif's es8311 driver
// (used by Waveshare's 07_ES8311 example for this board) arrives at for that
// clocking.
bool codecStart(uint8_t volumePercent) {
  bool ok = writeReg(0x00, 0x1F);  // reset
  delay(10);
  ok &= writeReg(0x00, 0x00);
  ok &= writeReg(0x00, 0x80);  // power-on command, slave mode
  ok &= writeReg(0x01, 0x3F);  // all clocks on, MCLK from the pin
  ok &= writeReg(0x02, 0x00);  // pre-divider 1, multiplier 1
  ok &= writeReg(0x03, 0x10);  // single speed, ADC OSR
  ok &= writeReg(0x04, 0x10);  // DAC OSR
  ok &= writeReg(0x05, 0x00);  // ADC and DAC clock dividers 1
  ok &= writeReg(0x06, 0x03);  // BCLK divider 4
  ok &= writeReg(0x07, 0x00);  // LRCK divider high
  ok &= writeReg(0x08, 0xFF);  // LRCK divider low
  ok &= writeReg(0x09, 0x0C);  // serial port in: I2S, 16 bit
  ok &= writeReg(0x0A, 0x0C);  // serial port out: I2S, 16 bit
  ok &= writeReg(0x0D, 0x01);  // analog circuitry up
  ok &= writeReg(0x0E, 0x02);  // PGA and modulator
  ok &= writeReg(0x12, 0x00);  // DAC up
  ok &= writeReg(0x13, 0x10);  // output to the headphone drive
  ok &= writeReg(0x1C, 0x6A);  // ADC equaliser bypass
  ok &= writeReg(0x37, 0x08);  // DAC equaliser bypass
  const int reg = volumePercent == 0 ? 0 : (volumePercent * 256 / 100) - 1;
  ok &= writeReg(0x32, static_cast<uint8_t>(reg));
  return ok;
}

// Back to the codec's low-power state (es8311_suspend in Espressif's ADF).
void codecSleep() {
  writeReg(0x32, 0x00);
  writeReg(0x17, 0x00);
  writeReg(0x0E, 0xFF);
  writeReg(0x12, 0x02);
  writeReg(0x14, 0x00);
  writeReg(0x0D, 0xFA);
  writeReg(0x15, 0x00);
  writeReg(0x37, 0x08);
  writeReg(0x45, 0x01);
}

// One stroke of a small bell: a fundamental and two overtones that die away
// at different rates, with a soft attack so the speaker does not click.
void writeStroke(I2SClass& i2s, float seconds) {
  constexpr float kFundamental = 987.77f;  // B5
  constexpr int kChunk = 256;
  int16_t buffer[kChunk * 2];
  const int total = static_cast<int>(seconds * kSampleRate);
  for (int n = 0; n < total; n += kChunk) {
    for (int i = 0; i < kChunk; i++) {
      const float t = static_cast<float>(n + i) / kSampleRate;
      const float attack = t < 0.004f ? t / 0.004f : 1.0f;
      const float w = 2.0f * static_cast<float>(M_PI) * kFundamental * t;
      float v = 0.60f * sinf(w) * expf(-t / 0.55f);
      v += 0.25f * sinf(2.0f * w) * expf(-t / 0.32f);
      v += 0.15f * sinf(2.98f * w) * expf(-t / 0.18f);
      const int16_t sample = static_cast<int16_t>(v * attack * 26000.0f);
      buffer[i * 2] = sample;
      buffer[i * 2 + 1] = sample;
    }
    i2s.write(reinterpret_cast<const uint8_t*>(buffer), sizeof(buffer));
  }
}

}  // namespace

bool audioAvailable() { return true; }

void audioBell(int strikes, uint8_t volume) {
  static const uint8_t percent[3] = {62, 76, 90};
  I2SClass i2s;
  i2s.setPins(PIN_I2S_BCLK, PIN_I2S_LRCK, PIN_I2S_DOUT, -1, PIN_I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, kSampleRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) return;
  if (codecStart(percent[volume > 2 ? 2 : volume])) {
    digitalWrite(PIN_PA_CTRL, HIGH);
    delay(30);
    for (int i = 0; i < strikes; i++) {
      writeStroke(i2s, i + 1 < strikes ? 0.9f : 1.5f);
    }
    delay(40);
    digitalWrite(PIN_PA_CTRL, LOW);
  }
  codecSleep();
  i2s.end();
}

#else

bool audioAvailable() { return false; }

void audioBell(int, uint8_t) {}

#endif

}  // namespace board
