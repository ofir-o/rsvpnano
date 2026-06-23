#include "drivers/rtc/pcf85063/Pcf85063.h"

#include <Wire.h>

namespace BoardDrivers::Pcf85063 {
namespace {

constexpr uint8_t kAddress = 0x51;
// Time/date registers are contiguous starting at 0x04:
//   0x04 seconds (bit7 = OS oscillator-stop flag)
//   0x05 minutes
//   0x06 hours   (24h mode; bit5 = AMPM only in 12h mode, unused here)
//   0x07 days
//   0x08 weekdays
//   0x09 months
//   0x0A years
constexpr uint8_t kSecondsReg = 0x04;
constexpr uint8_t kOscillatorStopMask = 0x80;

uint8_t bcdToDecimal(uint8_t value) {
  return static_cast<uint8_t>(((value >> 4) * 10) + (value & 0x0F));
}

uint8_t decimalToBcd(uint8_t value) {
  return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
}

bool readRegisters(uint8_t reg, uint8_t *buffer, uint8_t count) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  if (Wire.endTransmission(true) != 0) {
    return false;
  }
  if (Wire.requestFrom(kAddress, count) != count) {
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) {
    buffer[i] = Wire.read();
  }
  return true;
}

bool writeRegisters(uint8_t reg, const uint8_t *buffer, uint8_t count) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  for (uint8_t i = 0; i < count; ++i) {
    Wire.write(buffer[i]);
  }
  return Wire.endTransmission(true) == 0;
}

}  // namespace

bool begin() {
  Wire.beginTransmission(kAddress);
  return Wire.endTransmission(true) == 0;
}

bool read(DateTime &out, bool *oscillatorStopped) {
  uint8_t raw[7] = {0};
  if (!readRegisters(kSecondsReg, raw, sizeof(raw))) {
    return false;
  }

  if (oscillatorStopped != nullptr) {
    *oscillatorStopped = (raw[0] & kOscillatorStopMask) != 0;
  }

  out.second = bcdToDecimal(static_cast<uint8_t>(raw[0] & 0x7F));
  out.minute = bcdToDecimal(static_cast<uint8_t>(raw[1] & 0x7F));
  out.hour = bcdToDecimal(static_cast<uint8_t>(raw[2] & 0x3F));
  out.day = bcdToDecimal(static_cast<uint8_t>(raw[3] & 0x3F));
  out.weekday = bcdToDecimal(static_cast<uint8_t>(raw[4] & 0x07));
  out.month = bcdToDecimal(static_cast<uint8_t>(raw[5] & 0x1F));
  out.year = static_cast<uint16_t>(2000 + bcdToDecimal(raw[6]));
  return true;
}

bool set(const DateTime &value) {
  uint8_t raw[7];
  // Writing the seconds register with bit7 cleared also clears the OS flag.
  raw[0] = static_cast<uint8_t>(decimalToBcd(value.second) & 0x7F);
  raw[1] = decimalToBcd(value.minute);
  raw[2] = decimalToBcd(value.hour);
  raw[3] = decimalToBcd(value.day);
  raw[4] = decimalToBcd(static_cast<uint8_t>(value.weekday & 0x07));
  raw[5] = decimalToBcd(value.month);
  raw[6] = decimalToBcd(static_cast<uint8_t>(value.year % 100));
  return writeRegisters(kSecondsReg, raw, sizeof(raw));
}

}  // namespace BoardDrivers::Pcf85063
