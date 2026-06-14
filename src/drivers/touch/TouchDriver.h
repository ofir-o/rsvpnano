#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace TouchDriver {

struct Sample {
  bool touched = false;
  uint16_t physicalX = 0;
  uint16_t physicalY = 0;
};

TwoWire &wire();
bool configure(uint8_t address);
bool usesInterruptGate();
size_t packetLength();
bool readPacket(uint8_t address, uint8_t *buffer, size_t len);
bool decodePacket(const uint8_t *data, size_t len, Sample &sample);

}  // namespace TouchDriver
