#pragma once

#include <Arduino.h>

#include "board/BoardPower.h"

namespace BoardDrivers::Axp2101 {

bool begin();
bool readBatteryStatus(Board::Power::BatteryStatus &status);
Board::Power::DiagnosticSnapshot diagnosticSnapshot();
bool externalPowerPresent();
bool releasePower();
void pollPowerKeyIfDue(bool force = false);
bool isPowerButtonHeld();
bool consumeShortPress();
bool consumeLongPress();
// Battery saver (runtime opt-in). setDeepSleepRailCutEnabled() arms it; when armed,
// cutAldoRailsForDeepSleep() cuts the ALDO peripheral rails (touch/IMU/display) before deep sleep,
// stashing the previous state in RTC memory. begin() restores it on the next (deep-sleep wake) boot.
void setDeepSleepRailCutEnabled(bool enabled);
void cutAldoRailsForDeepSleep();

}  // namespace BoardDrivers::Axp2101
