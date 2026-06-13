#include "input/ButtonHandler.h"

ButtonHandler::ButtonHandler(int pin) : pin_(pin) {}

void ButtonHandler::resetState(bool held, uint32_t nowMs) {
  held_ = held;
  pressedEvent_ = false;
  releasedEvent_ = false;
  lastEdgeMs_ = nowMs;
  pressStartedMs_ = held ? nowMs : 0;
  lastHoldDurationMs_ = 0;
}

void ButtonHandler::begin() {
  if (pin_ < 0) {
    resetState(false, millis());
    return;
  }

  pinMode(pin_, INPUT_PULLUP);
  resetState(!digitalRead(pin_), millis());
}

void ButtonHandler::beginWithState(bool held) { resetState(held, millis()); }

void ButtonHandler::updateFromState(bool currentHeld, uint32_t nowMs) {
  pressedEvent_ = false;
  releasedEvent_ = false;

  if (currentHeld != held_) {
    held_ = currentHeld;
    lastEdgeMs_ = nowMs;
    if (held_) {
      pressStartedMs_ = nowMs;
      pressedEvent_ = true;
    } else {
      lastHoldDurationMs_ = nowMs - pressStartedMs_;
      releasedEvent_ = true;
    }
  }
}

void ButtonHandler::update(uint32_t nowMs) {
  if (pin_ < 0) {
    pressedEvent_ = false;
    releasedEvent_ = false;
    return;
  }

  updateFromState(!digitalRead(pin_), nowMs);  // Board buttons are active-low.
}

bool ButtonHandler::isHeld() const { return held_; }

bool ButtonHandler::wasPressedEvent() const { return pressedEvent_; }

bool ButtonHandler::wasReleasedEvent() const { return releasedEvent_; }

uint32_t ButtonHandler::lastEdgeMs() const { return lastEdgeMs_; }

uint32_t ButtonHandler::heldDurationMs(uint32_t nowMs) const {
  return held_ ? nowMs - pressStartedMs_ : 0;
}

uint32_t ButtonHandler::lastHoldDurationMs() const { return lastHoldDurationMs_; }
