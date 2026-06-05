#pragma once

#include <Arduino.h>

#include "board/BoardConfig.h"

enum class TouchPhase {
  Start,
  Move,
  End,
};

struct TouchEvent {
  bool touched = false;
  uint16_t x = 0;
  uint16_t y = 0;
  uint8_t gesture = 0;
  TouchPhase phase = TouchPhase::Move;
};

class TouchHandler {
 public:
  bool begin();
  void end();
  bool poll(TouchEvent &event);
  void cancel();
  void setUiOrientation(BoardConfig::UiOrientation orientation);
  void setUiRotated180(bool rotated180);

 private:
  static constexpr uint8_t kAddress = BoardConfig::TOUCH_I2C_ADDRESS;
  bool initialized_ = false;
  uint32_t lastPollMs_ = 0;
  uint32_t backoffUntilMs_ = 0;
  uint32_t ignoreEventsUntilMs_ = 0;
  uint32_t lastTouchSampleMs_ = 0;
  uint8_t consecutiveReadFailures_ = 0;
  uint8_t emptyTouchSamples_ = 0;
  bool touchActive_ = false;
  BoardConfig::UiOrientation uiOrientation_ = BoardConfig::DEFAULT_UI_ORIENTATION;
  uint16_t lastX_ = 0;
  uint16_t lastY_ = 0;

  bool readTouchPacket(uint8_t *buffer, size_t len);
};
