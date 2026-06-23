#include "sensors/AutoLevel.h"

#include <math.h>

#include "board/BoardImu.h"

namespace {

// QMI8658 register map (matches the proven sequence in FocusTimer).
constexpr uint8_t kImuWhoAmIReg = 0x00;
constexpr uint8_t kImuCtrl1Reg = 0x02;
constexpr uint8_t kImuCtrl2Reg = 0x03;
constexpr uint8_t kImuCtrl5Reg = 0x06;
constexpr uint8_t kImuCtrl7Reg = 0x08;
constexpr uint8_t kImuCtrl8Reg = 0x09;
constexpr uint8_t kImuAccelStartReg = 0x35;
constexpr uint8_t kImuResetReg = 0x60;
constexpr uint8_t kImuResetValue = 0xB0;
constexpr uint8_t kImuResetResultReg = 0x4D;
constexpr uint8_t kImuResetResultValue = 0x80;
constexpr uint8_t kImuWhoAmIValue = 0x05;

// How often to actually read the accelerometer. The UI does not need a fast
// update rate to feel responsive for rotation, and a slower cadence keeps the
// I2C bus and CPU cost negligible.
constexpr uint32_t kSampleIntervalMs = 120;

// Debounce: a candidate orientation must persist this long before it is
// applied. Long enough to ride out a brief 45 degree pass-through while the
// user reorients the device, short enough to feel responsive.
constexpr uint32_t kOrientationDebounceMs = 450;

// Angular hysteresis (degrees). The screen is divided into four 90 degree
// sectors centred on each cardinal orientation. To switch *away* from the
// currently held orientation the tilt angle must cross past the 45 degree
// sector boundary by this much; staying within the boundary keeps the current
// orientation. This prevents flip-flopping when the device hovers near 45.
constexpr float kHysteresisDeg = 12.0f;

// Minimum in-plane gravity magnitude (g) required to trust the angle. When the
// device lies flat (face up/down) the X/Y components are tiny and the angle is
// meaningless, so we hold the last orientation instead of jittering.
constexpr float kMinInPlaneG = 0.40f;

constexpr float kDegPerRad = 57.2957795f;

float orientationCenterDeg(Board::Config::UiOrientation orientation) {
  switch (orientation) {
    case Board::Config::UiOrientation::Landscape:
      return 0.0f;
    case Board::Config::UiOrientation::Portrait:
      return 90.0f;
    case Board::Config::UiOrientation::LandscapeFlipped:
      return 180.0f;
    case Board::Config::UiOrientation::PortraitFlipped:
    default:
      return 270.0f;
  }
}

// Smallest absolute difference between two angles in [0,360), result in [0,180].
float angularDistanceDeg(float a, float b) {
  float diff = fabsf(a - b);
  while (diff > 360.0f) {
    diff -= 360.0f;
  }
  if (diff > 180.0f) {
    diff = 360.0f - diff;
  }
  return diff;
}

}  // namespace

bool AutoLevel::begin() { return initImu(); }

void AutoLevel::reset() {
  hasStable_ = false;
  changePending_ = false;
  hasCandidate_ = false;
  candidateSinceMs_ = 0;
}

void AutoLevel::update(uint32_t nowMs) {
  if (!imuAvailable_) {
    return;
  }

  if (lastSampleMs_ != 0 && (nowMs - lastSampleMs_) < kSampleIntervalMs) {
    return;
  }
  lastSampleMs_ = nowMs;

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  if (!readAccelerometer(x, y, z)) {
    return;
  }

  // Hold the last orientation when the device is too flat to give a reliable
  // in-plane angle.
  if (sqrtf((x * x) + (y * y)) < kMinInPlaneG) {
    return;
  }

  const Board::Config::UiOrientation candidate = classify(x, y);

  if (!hasCandidate_ || candidate != candidateOrientation_) {
    candidateOrientation_ = candidate;
    candidateSinceMs_ = nowMs;
    hasCandidate_ = true;
    return;
  }

  if ((nowMs - candidateSinceMs_) >= kOrientationDebounceMs) {
    if (!hasStable_ || candidateOrientation_ != stableOrientation_) {
      stableOrientation_ = candidateOrientation_;
      hasStable_ = true;
      changePending_ = true;
    }
  }
}

bool AutoLevel::consumeOrientationChange(Board::Config::UiOrientation &out) {
  if (!changePending_) {
    return false;
  }
  changePending_ = false;
  out = stableOrientation_;
  return true;
}

Board::Config::UiOrientation AutoLevel::classify(float x, float y) const {
  // Tilt angle of the gravity vector in the screen plane. atan2(x, y) yields
  // 0 deg when gravity points along +Y (device upright in landscape) and
  // increases toward +X.
  float angle = atan2f(x, y) * kDegPerRad;
  if (angle < 0.0f) {
    angle += 360.0f;
  }

  // Apply hysteresis: keep the current orientation unless the angle has moved
  // clearly out of its 45 degree sector.
  if (hasStable_) {
    const float distToCurrent = angularDistanceDeg(angle, orientationCenterDeg(stableOrientation_));
    if (distToCurrent <= (45.0f + kHysteresisDeg)) {
      return stableOrientation_;
    }
  }

  // Otherwise snap to the nearest cardinal orientation.
  const Board::Config::UiOrientation candidates[] = {
      Board::Config::UiOrientation::Landscape,
      Board::Config::UiOrientation::Portrait,
      Board::Config::UiOrientation::LandscapeFlipped,
      Board::Config::UiOrientation::PortraitFlipped,
  };

  Board::Config::UiOrientation best = Board::Config::UiOrientation::Landscape;
  float bestDist = 1000.0f;
  for (const Board::Config::UiOrientation candidate : candidates) {
    const float dist = angularDistanceDeg(angle, orientationCenterDeg(candidate));
    if (dist < bestDist) {
      bestDist = dist;
      best = candidate;
    }
  }
  return best;
}

bool AutoLevel::initImu() {
  if (!Board::Imu::available()) {
    imuAvailable_ = false;
    Serial.println("[autolevel] IMU unavailable for this board profile");
    return false;
  }

  if (imuAvailable_) {
    return true;
  }

  const uint8_t candidateAddresses[] = {
      Board::Imu::address(),
      0x6B,
      0x6A,
  };

  for (uint8_t i = 0; i < sizeof(candidateAddresses); ++i) {
    const uint8_t candidateAddress = candidateAddresses[i];
    bool alreadyTried = false;
    for (uint8_t j = 0; j < i; ++j) {
      if (candidateAddresses[j] == candidateAddress) {
        alreadyTried = true;
        break;
      }
    }
    if (alreadyTried) {
      continue;
    }

    if (!Board::Imu::probeAddress(candidateAddress)) {
      continue;
    }
    imuAddress_ = candidateAddress;

    uint8_t whoAmI = 0;
    if (!Board::Imu::readRegister(imuAddress_, kImuWhoAmIReg, whoAmI) ||
        whoAmI != kImuWhoAmIValue) {
      continue;
    }

    if (!Board::Imu::writeRegister(imuAddress_, kImuResetReg, kImuResetValue)) {
      continue;
    }

    const uint32_t waitStartedMs = millis();
    uint8_t resetResult = 0;
    bool resetReady = false;
    while (millis() - waitStartedMs < 500) {
      if (Board::Imu::readRegister(imuAddress_, kImuResetResultReg, resetResult) &&
          resetResult == kImuResetResultValue) {
        resetReady = true;
        break;
      }
      delay(10);
    }
    if (!resetReady) {
      continue;
    }

    whoAmI = 0;
    if (!Board::Imu::readRegister(imuAddress_, kImuWhoAmIReg, whoAmI) ||
        whoAmI != kImuWhoAmIValue) {
      continue;
    }

    // Enable accelerometer, +/-4g full scale, ~125 Hz ODR (same config the
    // FocusTimer uses successfully).
    if (!updateRegister(kImuCtrl1Reg, 0x40, 0x40) ||
        !Board::Imu::writeRegister(imuAddress_, kImuCtrl8Reg, 0x80) ||
        !Board::Imu::writeRegister(imuAddress_, kImuCtrl2Reg, 0x16) ||
        !updateRegister(kImuCtrl5Reg, 0x07, 0x07) ||
        !updateRegister(kImuCtrl7Reg, 0x01, 0x01)) {
      continue;
    }

    accelScale_ = 4.0f / 32768.0f;
    reset();
    imuAvailable_ = true;
    Serial.printf("[autolevel] QMI8658 ready addr=0x%02X bus=%s\n", imuAddress_,
                  Board::Imu::wireName());
    return true;
  }

  imuAvailable_ = false;
  Serial.println("[autolevel] QMI8658 init failed; auto-rotate disabled");
  return false;
}

bool AutoLevel::updateRegister(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current = 0;
  if (!Board::Imu::readRegister(imuAddress_, reg, current)) {
    return false;
  }
  current = static_cast<uint8_t>((current & static_cast<uint8_t>(~mask)) | (value & mask));
  return Board::Imu::writeRegister(imuAddress_, reg, current);
}

bool AutoLevel::readAccelerometer(float &x, float &y, float &z) {
  uint8_t buffer[6] = {0};
  if (!Board::Imu::readRegisters(imuAddress_, kImuAccelStartReg, buffer, sizeof(buffer))) {
    return false;
  }

  const int16_t rawX = static_cast<int16_t>((buffer[1] << 8) | buffer[0]);
  const int16_t rawY = static_cast<int16_t>((buffer[3] << 8) | buffer[2]);
  const int16_t rawZ = static_cast<int16_t>((buffer[5] << 8) | buffer[4]);

  x = rawX * accelScale_;
  y = rawY * accelScale_;
  z = rawZ * accelScale_;
  return true;
}
