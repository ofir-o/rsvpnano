#include "board/BoardClock.h"

#include "drivers/rtc/pcf85063/Pcf85063.h"

namespace Board::Clock {
namespace {
bool gAvailable = false;
}  // namespace

bool begin() {
  gAvailable = BoardDrivers::Pcf85063::begin();
  return gAvailable;
}

bool available() { return gAvailable; }

bool read(DateTime &out, bool *oscillatorStopped) {
  if (!gAvailable && !begin()) {
    return false;
  }

  BoardDrivers::Pcf85063::DateTime raw;
  if (!BoardDrivers::Pcf85063::read(raw, oscillatorStopped)) {
    gAvailable = false;
    return false;
  }

  out.year = raw.year;
  out.month = raw.month;
  out.day = raw.day;
  out.weekday = raw.weekday;
  out.hour = raw.hour;
  out.minute = raw.minute;
  out.second = raw.second;
  return true;
}

bool set(const DateTime &value) {
  if (!gAvailable && !begin()) {
    return false;
  }

  BoardDrivers::Pcf85063::DateTime raw;
  raw.year = value.year;
  raw.month = value.month;
  raw.day = value.day;
  raw.weekday = value.weekday;
  raw.hour = value.hour;
  raw.minute = value.minute;
  raw.second = value.second;
  return BoardDrivers::Pcf85063::set(raw);
}

}  // namespace Board::Clock
