#pragma once

#include <Arduino.h>

#include "board/BoardConfig.h"

// AutoLevel: gyro/IMU driven auto-rotation helper.
//
// Reads the accelerometer (gravity vector) from the on-board QMI8658 IMU and
// derives the nearest of the four cardinal UI orientations so the UI can stay
// upright as the device is tilted. Uses atan2 of the X/Y gravity components
// with an angular hysteresis band plus a debounce time to avoid flip-flopping
// near the 45 degree boundaries.
//
// This reuses the existing Board::Imu wrapper (QMI8658 driver) for all I2C
// access; it does not add a new driver. The IMU init/config sequence mirrors
// the one already proven in FocusTimer.
//
// NOTE: True continuous, arbitrary-angle text rotation is out of scope for the
// current renderer, which only supports the four cardinal orientations via
// DisplayManager::setUiOrientation(). The "Continuous" auto-rotate setting is
// therefore treated as 4-way snap for now. TODO: when the renderer gains
// arbitrary-angle glyph rotation, AutoLevel can expose the raw tilt angle so
// the UI can rotate smoothly instead of snapping.
class AutoLevel {
 public:
  // Initialize the IMU. Returns false (and disables the feature) on boards
  // without an IMU or if the QMI8658 fails to respond/configure.
  bool begin();

  bool available() const { return imuAvailable_; }

  // Sample the accelerometer and update the debounced orientation estimate.
  // Should be called periodically from the app loop; sampling is internally
  // throttled so calling it every loop tick is cheap.
  void update(uint32_t nowMs);

  // True once a stable orientation has been observed since boot.
  bool hasStableOrientation() const { return hasStable_; }

  // The current debounced upright orientation. Only meaningful when
  // hasStableOrientation() is true.
  Board::Config::UiOrientation orientation() const { return stableOrientation_; }

  // Returns true and updates `out` if a new stable orientation became
  // available since the last call (edge-triggered convenience for callers
  // that only want to act on changes).
  bool consumeOrientationChange(Board::Config::UiOrientation &out);

  // Forget the current estimate (e.g. when auto-rotate is toggled on) so the
  // next stable reading is reported as a change.
  void reset();

 private:
  bool initImu();
  bool updateRegister(uint8_t reg, uint8_t mask, uint8_t value);
  bool readAccelerometer(float &x, float &y, float &z);
  Board::Config::UiOrientation classify(float x, float y) const;

  bool imuAvailable_ = false;
  uint8_t imuAddress_ = 0;
  float accelScale_ = 0.0f;

  uint32_t lastSampleMs_ = 0;

  bool hasStable_ = false;
  bool changePending_ = false;
  Board::Config::UiOrientation stableOrientation_ = Board::Config::DEFAULT_UI_ORIENTATION;
  Board::Config::UiOrientation candidateOrientation_ = Board::Config::DEFAULT_UI_ORIENTATION;
  bool hasCandidate_ = false;
  uint32_t candidateSinceMs_ = 0;
};
