#pragma once

#include <Arduino.h>

// Driver for the NXP PCF85063A/TP real-time clock used on the Waveshare
// ESP32-S3-Touch-AMOLED-1.75. The RTC shares the system I2C bus (the global
// `Wire` instance) with the AXP2101 PMU; this mirrors the Wire access pattern in
// drivers/power/axp2101/Axp2101.cpp. The PCF85063 sits at I2C address 0x51 and
// keeps time across reboots/power loss from its backup supply.
namespace BoardDrivers::Pcf85063 {

struct DateTime {
  uint16_t year = 2025;  // Full year (the chip only stores 00-99 => 2000-2099).
  uint8_t month = 1;     // 1-12
  uint8_t day = 1;       // 1-31
  uint8_t weekday = 0;   // 0-6 (0 = Sunday). Informational only.
  uint8_t hour = 0;      // 0-23
  uint8_t minute = 0;    // 0-59
  uint8_t second = 0;    // 0-59
};

// Probes the RTC on the shared Wire bus. Returns true when the device ACKs.
bool begin();

// Reads the current date/time. Returns false on a bus error. When the
// oscillator-stop (OS) flag is set the time is unreliable (e.g. backup power was
// lost); `oscillatorStopped` reports that condition but the fields are still
// populated with whatever the chip held.
bool read(DateTime &out, bool *oscillatorStopped = nullptr);

// Writes the date/time and clears the oscillator-stop flag. Returns false on a
// bus error.
bool set(const DateTime &value);

}  // namespace BoardDrivers::Pcf85063
