#include "input/TouchHandler.h"

#include <algorithm>
#include <Wire.h>

#include "board/BoardConfig.h"

namespace {

constexpr uint8_t kReadTouchCommand[] = {
    0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
};
constexpr uint16_t kCst92xxReadCommand = 0xD000;
constexpr uint8_t kCst92xxAck = 0xAB;
constexpr uint8_t kCst92xxMaxTouchPoints = 1;
constexpr size_t kCst92xxPacketLength = kCst92xxMaxTouchPoints * 5 + 5;
constexpr uint32_t kPollIntervalMs = BoardConfig::TOUCH_POLL_INTERVAL_MS;
constexpr uint32_t kFailureBackoffMs = BoardConfig::TOUCH_FAILURE_BACKOFF_MS;
constexpr uint32_t kRecoveryRetryMs = BoardConfig::TOUCH_RECOVERY_RETRY_MS;
constexpr uint8_t kReleaseConfirmSamples = 2;
constexpr uint8_t kFt6336PointsReg = 0x02;

uint16_t clampDisplayX(uint16_t x) {
  return std::min<uint16_t>(x, static_cast<uint16_t>(BoardConfig::DISPLAY_WIDTH - 1));
}

uint16_t clampDisplayY(uint16_t y) {
  return std::min<uint16_t>(y, static_cast<uint16_t>(BoardConfig::DISPLAY_HEIGHT - 1));
}

uint16_t clampPhysicalX(uint16_t x) {
  return std::min<uint16_t>(x, static_cast<uint16_t>(BoardConfig::PANEL_NATIVE_WIDTH - 1));
}

uint16_t clampPhysicalY(uint16_t y) {
  return std::min<uint16_t>(y, static_cast<uint16_t>(BoardConfig::PANEL_NATIVE_HEIGHT - 1));
}

TwoWire &touchWire() { return BoardConfig::TOUCH_USES_WIRE1 ? Wire1 : Wire; }

bool touchInterruptActive() {
  return BoardConfig::PIN_TOUCH_IRQ < 0 || !digitalRead(BoardConfig::PIN_TOUCH_IRQ);
}

bool writeTouchRegister(uint8_t reg, uint8_t value) {
  TwoWire &wire = touchWire();
  wire.beginTransmission(BoardConfig::TOUCH_I2C_ADDRESS);
  wire.write(reg);
  wire.write(value);
  return wire.endTransmission(true) == 0;
}

}  // namespace

bool TouchHandler::begin() {
  lastPollMs_ = 0;
  backoffUntilMs_ = 0;
  ignoreEventsUntilMs_ = 0;
  lastTouchSampleMs_ = 0;
  consecutiveReadFailures_ = 0;
  emptyTouchSamples_ = 0;
  touchActive_ = false;
  lastX_ = 0;
  lastY_ = 0;

  BoardConfig::resetTouchController();

  TwoWire &wire = touchWire();
  wire.beginTransmission(kAddress);
  const uint8_t error = wire.endTransmission();
  initialized_ = (error == 0);

  if (!initialized_) {
    Serial.printf("[touch] Controller not detected at 0x%02X\n", kAddress);
  } else {
    Serial.printf("[touch] Initialized controller=0x%02X\n", kAddress);
    if (BoardConfig::TOUCH_RECOVERY_EVENT_IGNORE_MS > 0) {
      ignoreEventsUntilMs_ = millis() + BoardConfig::TOUCH_RECOVERY_EVENT_IGNORE_MS;
      Serial.printf("[touch] Ignoring events for %lu ms after init\n",
                    static_cast<unsigned long>(BoardConfig::TOUCH_RECOVERY_EVENT_IGNORE_MS));
    }
    if (BoardConfig::TOUCH_REQUIRES_MONITOR_MODE) {
      if (writeTouchRegister(BoardConfig::TOUCH_MONITOR_MODE_REGISTER,
                             BoardConfig::TOUCH_MONITOR_MODE_VALUE)) {
        Serial.printf("[touch] Applied monitor mode reg=0x%02X value=0x%02X\n",
                      BoardConfig::TOUCH_MONITOR_MODE_REGISTER,
                      BoardConfig::TOUCH_MONITOR_MODE_VALUE);
      } else {
        Serial.println("[touch] Failed to apply board-specific monitor mode");
      }
    }
  }

  return initialized_;
}

void TouchHandler::end() {
  cancel();
  initialized_ = false;
}

void TouchHandler::cancel() {
  touchActive_ = false;
  lastPollMs_ = 0;
  backoffUntilMs_ = 0;
  ignoreEventsUntilMs_ = 0;
  lastTouchSampleMs_ = 0;
  consecutiveReadFailures_ = 0;
  emptyTouchSamples_ = 0;
}

void TouchHandler::setUiOrientation(BoardConfig::UiOrientation orientation) {
  if (uiOrientation_ == orientation) {
    return;
  }

  uiOrientation_ = orientation;
  cancel();
}

void TouchHandler::setUiRotated180(bool rotated180) {
  setUiOrientation(rotated180 ? BoardConfig::ROTATED_UI_ORIENTATION
                              : BoardConfig::DEFAULT_UI_ORIENTATION);
}

bool TouchHandler::readTouchPacket(uint8_t *buffer, size_t len) {
  TwoWire &wire = touchWire();

  if (BoardConfig::TOUCH_CONTROLLER == BoardConfig::TouchControllerKind::Ft6336) {
    wire.beginTransmission(kAddress);
    wire.write(kFt6336PointsReg);
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
    constexpr bool releaseBusBeforeRead = true;
#else
    constexpr bool releaseBusBeforeRead = false;
#endif
    if (wire.endTransmission(releaseBusBeforeRead) != 0) {
      return false;
    }
    if (releaseBusBeforeRead) {
      delayMicroseconds(50);
    }

    const size_t readLen = wire.requestFrom(static_cast<uint8_t>(kAddress),
                                            static_cast<size_t>(len), true);
    if (readLen != len) {
      return false;
    }

    for (size_t i = 0; i < len; ++i) {
      buffer[i] = wire.read();
    }
    return true;
  }

  if (BoardConfig::TOUCH_CONTROLLER == BoardConfig::TouchControllerKind::Cst92xx) {
    const uint8_t readCommand[] = {highByte(kCst92xxReadCommand), lowByte(kCst92xxReadCommand)};
    constexpr uint8_t kMaxRetries = 5;
    for (uint8_t retry = 0; retry < kMaxRetries; ++retry) {
      wire.beginTransmission(kAddress);
      wire.write(readCommand, sizeof(readCommand));
      if (wire.endTransmission(true) != 0) {
        delay(3);
        continue;
      }

      delay(2);
      const size_t readLen =
          wire.requestFrom(static_cast<uint8_t>(kAddress), static_cast<size_t>(len), true);
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

  wire.beginTransmission(kAddress);
  wire.write(kReadTouchCommand, sizeof(kReadTouchCommand));
  if (wire.endTransmission(false) != 0) {
    return false;
  }

  const size_t readLen =
      wire.requestFrom(static_cast<uint8_t>(kAddress), static_cast<size_t>(len), true);
  if (readLen != len) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    buffer[i] = wire.read();
  }
  return true;
}

bool TouchHandler::poll(TouchEvent &event) {
  event = TouchEvent{};

  const uint32_t now = millis();

  if (!initialized_) {
    if (now < backoffUntilMs_) {
      return false;
    }

    Serial.println("[touch] Attempting controller recovery");
    if (!begin()) {
      backoffUntilMs_ = now + kRecoveryRetryMs;
    }
    return false;
  }

  if (now < backoffUntilMs_) {
    return false;
  }

  if (now - lastPollMs_ < kPollIntervalMs) {
    return false;
  }
  lastPollMs_ = now;

  const bool isFt6336 = BoardConfig::TOUCH_CONTROLLER == BoardConfig::TouchControllerKind::Ft6336;
  const bool isCst92xx =
      BoardConfig::TOUCH_CONTROLLER == BoardConfig::TouchControllerKind::Cst92xx;
  const bool touchUsesInterruptGate = BoardConfig::PIN_TOUCH_IRQ >= 0 && isFt6336;
  auto releaseTouchIfNeeded = [&]() -> bool {
    if (!touchActive_) {
      return false;
    }

    ++emptyTouchSamples_;
    if (emptyTouchSamples_ < kReleaseConfirmSamples) {
      return false;
    }

    touchActive_ = false;
    emptyTouchSamples_ = 0;
    event.touched = false;
    event.x = lastX_;
    event.y = lastY_;
    event.phase = TouchPhase::End;
    return true;
  };

  const size_t packetLength = isFt6336 ? 5 : (isCst92xx ? kCst92xxPacketLength : 8);
  if (touchUsesInterruptGate && !touchInterruptActive()) {
    return releaseTouchIfNeeded();
  }

  uint8_t data[15] = {0};
  if (!readTouchPacket(data, packetLength)) {
    backoffUntilMs_ = now + kFailureBackoffMs;
    if (++consecutiveReadFailures_ >= 5) {
      initialized_ = false;
      touchActive_ = false;
      emptyTouchSamples_ = 0;
      backoffUntilMs_ = now + kRecoveryRetryMs;
      Serial.println("[touch] Read failed repeatedly, scheduling controller recovery");
    }
    return false;
  }

  if (isCst92xx && (data[0] == kCst92xxAck || data[6] != kCst92xxAck)) {
    backoffUntilMs_ = 0;
    consecutiveReadFailures_ = 0;
    return releaseTouchIfNeeded();
  }
  consecutiveReadFailures_ = 0;

  const uint8_t points = isFt6336 ? static_cast<uint8_t>(data[0] & 0x0F)
                                  : (isCst92xx ? static_cast<uint8_t>(data[5] & 0x7F) : data[1]);
  const uint8_t maxPoints = isCst92xx ? kCst92xxMaxTouchPoints : 4;
  const uint8_t cstTouchId = isCst92xx ? static_cast<uint8_t>(data[0] >> 4) : 0;
  const uint8_t cstEvent = isCst92xx ? static_cast<uint8_t>(data[0] & 0x0F) : 0;
  const bool cstPressed = !isCst92xx || (cstEvent == 0x06 && cstTouchId < maxPoints);
  if (points == 0 || points > maxPoints || !cstPressed) {
    return releaseTouchIfNeeded();
  }

  if (now < ignoreEventsUntilMs_) {
    touchActive_ = false;
    emptyTouchSamples_ = 0;
    return false;
  }

  backoffUntilMs_ = 0;
  consecutiveReadFailures_ = 0;
  emptyTouchSamples_ = 0;
  lastTouchSampleMs_ = now;

  event.touched = true;
  event.gesture = 0;
  event.phase = touchActive_ ? TouchPhase::Move : TouchPhase::Start;

  uint16_t physicalX = 0;
  uint16_t physicalY = 0;
  if (isFt6336) {
    physicalX = clampPhysicalX(static_cast<uint16_t>(((data[1] & 0x0F) << 8) | data[2]));
    physicalY = clampPhysicalY(static_cast<uint16_t>(((data[3] & 0x0F) << 8) | data[4]));
  } else if (isCst92xx) {
    physicalX = clampPhysicalX(static_cast<uint16_t>((data[1] << 4) | (data[3] >> 4)));
    physicalY = clampPhysicalY(static_cast<uint16_t>((data[2] << 4) | (data[3] & 0x0F)));
  } else {
    const uint16_t rawLongAxis = static_cast<uint16_t>(((data[2] & 0x0F) << 8) | data[3]);
    const uint16_t rawShortAxis = static_cast<uint16_t>(((data[4] & 0x0F) << 8) | data[5]);
    physicalX = clampPhysicalX(rawShortAxis);
    physicalY =
        clampPhysicalY(rawLongAxis >= BoardConfig::PANEL_NATIVE_HEIGHT
                           ? 0
                           : static_cast<uint16_t>(BoardConfig::PANEL_NATIVE_HEIGHT - 1 -
                                                   rawLongAxis));
  }

  switch (uiOrientation_) {
    case BoardConfig::UiOrientation::Portrait:
      event.x = physicalX;
      event.y = physicalY;
      break;
    case BoardConfig::UiOrientation::PortraitFlipped:
      event.x = static_cast<uint16_t>(BoardConfig::PANEL_NATIVE_WIDTH - 1 - physicalX);
      event.y = static_cast<uint16_t>(BoardConfig::PANEL_NATIVE_HEIGHT - 1 - physicalY);
      break;
    case BoardConfig::UiOrientation::Landscape:
      if (isFt6336 || isCst92xx) {
        event.x = clampDisplayX(
            static_cast<uint16_t>(BoardConfig::PANEL_NATIVE_HEIGHT - 1 - physicalY));
        event.y = clampDisplayY(physicalX);
      } else {
        event.x =
            clampDisplayX(static_cast<uint16_t>(BoardConfig::PANEL_NATIVE_HEIGHT - 1 - physicalY));
        event.y = clampDisplayY(physicalX);
      }
      break;
    case BoardConfig::UiOrientation::LandscapeFlipped:
    default:
      if (isFt6336 || isCst92xx) {
        event.x = clampDisplayX(physicalY);
        event.y =
            clampDisplayY(static_cast<uint16_t>(BoardConfig::PANEL_NATIVE_WIDTH - 1 - physicalX));
      } else {
        event.x = clampDisplayX(physicalY);
        event.y = clampDisplayY(
            static_cast<uint16_t>(BoardConfig::PANEL_NATIVE_WIDTH - 1 - physicalX));
      }
      break;
  }
  touchActive_ = true;
  lastX_ = event.x;
  lastY_ = event.y;

  return true;
}
