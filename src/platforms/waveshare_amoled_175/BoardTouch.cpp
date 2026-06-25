#include "board/BoardSystem.h"
#include "board/BoardTouch.h"

#include "drivers/touch/cst92xx/cst92xx.h"

namespace Board::Touch {

TwoWire &wire() { return Wire; }

void resetController() {
  Board::System::resetTouchController();
  // After the hardware reset, walk the CST9217 through the factory wake handshake so a cold chip
  // (e.g. after a real power-off) starts scanning and answers on I2C. Without this the controller
  // stays in boot mode and reads as "not found".
  Cst92xxTouch::wake(wire(), Board::Config::TOUCH_I2C_ADDRESS);
}

bool ready() { return Board::Config::PIN_TOUCH_IRQ < 0 || !digitalRead(Board::Config::PIN_TOUCH_IRQ); }

bool configure() { return true; }

size_t packetLength() { return Cst92xxTouch::packetLength(); }

bool readPacket(uint8_t *buffer, size_t len) {
  return Cst92xxTouch::readPacket(wire(), Board::Config::TOUCH_I2C_ADDRESS, buffer, len);
}

bool decodePacket(const uint8_t *data, size_t len, BoardDrivers::Touch::Sample &sample) {
  return Cst92xxTouch::decodePacket(data, len, sample);
}

}  // namespace Board::Touch
