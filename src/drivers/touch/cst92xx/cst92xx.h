#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "drivers/touch/TouchDriver.h"

namespace Cst92xxTouch {

size_t packetLength();
bool readPacket(TwoWire &wire, uint8_t address, uint8_t *buffer, size_t len);
bool decodePacket(const uint8_t *data, size_t len, TouchDriver::Sample &sample);

}  // namespace Cst92xxTouch
