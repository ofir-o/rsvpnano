#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <driver/i2s.h>

namespace BoardDrivers::Es8311 {

struct Config {
  TwoWire *wire = nullptr;
  uint8_t address = 0;
  i2s_port_t i2sPort = I2S_NUM_0;
  int mclkPin = I2S_PIN_NO_CHANGE;
  int bclkPin = I2S_PIN_NO_CHANGE;
  int wsPin = I2S_PIN_NO_CHANGE;
  int dataOutPin = I2S_PIN_NO_CHANGE;
  uint32_t sampleRateHz = 16000;
};

bool begin(const Config &config);
bool prepareOutput();
bool recoverOutputPath();
bool writeSamples(const int16_t *samples, size_t sampleCount, uint32_t timeoutMs);
bool available();

}  // namespace BoardDrivers::Es8311
