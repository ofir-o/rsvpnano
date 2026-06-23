#include "board/BoardClock.h"

// This board has no onboard real-time clock. The reader clock stays disabled via
// Board::Config::READER_SHOW_CLOCK == false; these stubs simply report that no
// RTC is available so the shared App/Display code links and runs unchanged.
namespace Board::Clock {

bool begin() { return false; }

bool available() { return false; }

bool read(DateTime &out, bool *oscillatorStopped) {
  (void)out;
  if (oscillatorStopped != nullptr) {
    *oscillatorStopped = true;
  }
  return false;
}

bool set(const DateTime &value) {
  (void)value;
  return false;
}

}  // namespace Board::Clock
