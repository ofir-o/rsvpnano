#pragma once

#include <Arduino.h>

#include "board/BoardConfig.h"

// Board-agnostic real-time clock facade. Boards with an onboard RTC (currently
// only the Waveshare AMOLED 1.75, which carries a PCF85063) implement this in
// their platform BoardClock.cpp; every other board links a no-op stub that
// reports the clock as unavailable. The reader clock is additionally gated by
// Board::Config::READER_SHOW_CLOCK.
namespace Board::Clock {

struct DateTime {
  uint16_t year = 2025;
  uint8_t month = 1;
  uint8_t day = 1;
  uint8_t weekday = 0;  // 0-6, 0 = Sunday
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
};

// Initialises the RTC. Returns true when a real-time clock responds.
bool begin();

// True when the board has a working RTC available for reads/writes.
bool available();

// Reads the current time. `oscillatorStopped` (optional) is set true when the
// RTC lost its oscillator (backup power was interrupted) and the time should be
// treated as unset. Returns false when no RTC is present or on a bus error.
bool read(DateTime &out, bool *oscillatorStopped = nullptr);

// Persists the time to the RTC (survives reboot/power loss via backup supply).
// Returns false when no RTC is present or on a bus error.
bool set(const DateTime &value);

}  // namespace Board::Clock
