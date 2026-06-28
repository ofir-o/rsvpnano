#pragma once

#include <Arduino.h>

#include "board/BoardConfig.h"

namespace Board::Power {

using BatteryStatus = Board::Config::BatteryStatus;
using DiagnosticSnapshot = Board::Config::PowerDiagnosticSnapshot;

void begin();
void prepareDeepSleepPowerHold();
void resetWakePeripherals();
// Runtime opt-in battery saver: arm/disarm cutting the peripheral power rails during deep sleep.
// No-op on boards without a controllable PMU. Set from the app's saved preference.
void setDeepSleepRailCut(bool enabled);
bool enableAudioPowerIfAvailable();
bool readBatteryStatus(BatteryStatus &status);
DiagnosticSnapshot diagnosticSnapshot();
bool externalPowerPresent();
bool releaseBatteryPowerHold();
bool powerOffUsesControllerWake();
bool shouldRequestShutdownOnPowerOff();
bool shouldReleaseBatteryPowerBeforeDeepSleep();

}  // namespace Board::Power
