#pragma once

#include <Arduino.h>

#include "board/BoardConfig.h"

namespace Input::Touch {

enum class Phase {
  Start,
  Move,
  End,
};

struct Event {
  bool touched = false;
  uint16_t x = 0;
  uint16_t y = 0;
  uint8_t gesture = 0;
  Phase phase = Phase::Move;
};

bool begin();
void end();
void cancel();
// True when the controller last answered on the I2C bus. Surfaced in Settings as a touch self-test
// so a dead/recovered touch chip can be diagnosed without a serial console.
bool isInitialized();
bool readEvent(Event &event);
void setUiOrientation(Board::Config::UiOrientation orientation);
void setUiRotated180(bool rotated180);

}  // namespace Input::Touch
