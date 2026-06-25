#include "drivers/touch/cst92xx/cst92xx.h"

#include <algorithm>

#include "board/BoardConfig.h"

namespace {

constexpr uint16_t kReadCommand = 0xD000;
constexpr uint8_t kAck = 0xAB;
constexpr uint8_t kMaxTouchPoints = 1;
constexpr size_t kPacketLength = kMaxTouchPoints * 5 + 5;

uint16_t clampPhysicalX(uint16_t x) {
  return std::min<uint16_t>(x, static_cast<uint16_t>(Board::Config::PANEL_NATIVE_WIDTH - 1));
}

uint16_t clampPhysicalY(uint16_t y) {
  return std::min<uint16_t>(y, static_cast<uint16_t>(Board::Config::PANEL_NATIVE_HEIGHT - 1));
}

}  // namespace

namespace {

bool writeRegister16(TwoWire &wire, uint8_t address, uint8_t regHigh, uint8_t regLow) {
  wire.beginTransmission(address);
  wire.write(regHigh);
  wire.write(regLow);
  return wire.endTransmission(true) == 0;
}

}  // namespace

namespace Cst92xxTouch {

size_t packetLength() { return kPacketLength; }

bool wake(TwoWire &wire, uint8_t address) {
  // Mirror the factory SensorLib TouchDrvCST92xx bring-up: after the hardware reset the controller
  // sits in boot mode, so step it into command mode (0xD101) and then back into normal scanning
  // mode (0xD109). Without this a cold chip never starts reporting and won't ACK normal reads.
  const bool enteredCommandMode = writeRegister16(wire, address, 0xD1, 0x01);
  delay(10);
  const bool enteredNormalMode = writeRegister16(wire, address, 0xD1, 0x09);
  delay(10);
  // Either ACK is enough to conclude the controller is alive on the bus.
  return enteredCommandMode || enteredNormalMode;
}

bool readPacket(TwoWire &wire, uint8_t address, uint8_t *buffer, size_t len) {
  const uint8_t readCommand[] = {highByte(kReadCommand), lowByte(kReadCommand)};
  constexpr uint8_t kMaxRetries = 5;

  for (uint8_t retry = 0; retry < kMaxRetries; ++retry) {
    wire.beginTransmission(address);
    wire.write(readCommand, sizeof(readCommand));
    if (wire.endTransmission(true) != 0) {
      delay(3);
      continue;
    }

    delay(2);
    const size_t readLen =
        wire.requestFrom(static_cast<uint8_t>(address), static_cast<size_t>(len), true);
    if (readLen == len) {
      for (size_t i = 0; i < len; ++i) {
        buffer[i] = wire.read();
      }
      return true;
    }
    while (wire.available() > 0) {
      wire.read();
    }
    delay(3);
  }

  return false;
}

bool decodePacket(const uint8_t *data, size_t len, BoardDrivers::Touch::Sample &sample) {
  if (data == nullptr || len < packetLength()) {
    return false;
  }

  if (data[0] == kAck || data[6] != kAck) {
    sample.touched = false;
    return true;
  }

  const uint8_t points = static_cast<uint8_t>(data[5] & 0x7F);
  const uint8_t touchId = static_cast<uint8_t>(data[0] >> 4);
  const uint8_t event = static_cast<uint8_t>(data[0] & 0x0F);
  if (points == 0 || points > kMaxTouchPoints || event != 0x06 || touchId >= kMaxTouchPoints) {
    sample.touched = false;
    return true;
  }

  sample.touched = true;
  sample.physicalX = clampPhysicalX(static_cast<uint16_t>((data[1] << 4) | (data[3] >> 4)));
  sample.physicalY = clampPhysicalY(static_cast<uint16_t>((data[2] << 4) | (data[3] & 0x0F)));
  return true;
}

}  // namespace Cst92xxTouch
