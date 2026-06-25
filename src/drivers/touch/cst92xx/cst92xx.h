#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "drivers/touch/TouchTypes.h"

namespace Cst92xxTouch {

size_t packetLength();
// Wake a freshly-reset controller into normal scanning mode. A cold CST92xx (e.g. after a real
// power-off) comes up in boot/command mode and will not report touches -- or even reliably ACK its
// I2C address -- until it is taken through the command-mode -> normal-mode handshake the factory
// driver performs. Call this right after pulsing the reset line. Returns true if the chip answered.
bool wake(TwoWire &wire, uint8_t address);
bool readPacket(TwoWire &wire, uint8_t address, uint8_t *buffer, size_t len);
bool decodePacket(const uint8_t *data, size_t len, BoardDrivers::Touch::Sample &sample);

}  // namespace Cst92xxTouch
