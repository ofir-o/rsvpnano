#include "board/BoardPower.h"

#include "drivers/power/axp2101/Axp2101.h"

namespace Board::Power {

void begin() { BoardDrivers::Axp2101::begin(); }

void prepareDeepSleepPowerHold() {
  // Opt-in battery saver: cut the peripheral (ALDO) rails so touch/IMU/display stop drawing current
  // through deep sleep. Self-gated -- only cuts when armed via setDeepSleepRailCut(). Restored on
  // the wake boot (see Axp2101::begin).
  BoardDrivers::Axp2101::cutAldoRailsForDeepSleep();
}

void resetWakePeripherals() {}

void setDeepSleepRailCut(bool enabled) {
  BoardDrivers::Axp2101::setDeepSleepRailCutEnabled(enabled);
}

bool enableAudioPowerIfAvailable() { return true; }

bool readBatteryStatus(BatteryStatus &status) {
  return BoardDrivers::Axp2101::readBatteryStatus(status);
}

DiagnosticSnapshot diagnosticSnapshot() { return BoardDrivers::Axp2101::diagnosticSnapshot(); }

bool externalPowerPresent() { return BoardDrivers::Axp2101::externalPowerPresent(); }

bool releaseBatteryPowerHold() { return BoardDrivers::Axp2101::releasePower(); }

bool powerOffUsesControllerWake() { return Config::REQUEST_PMU_SHUTDOWN_ON_POWEROFF; }

bool shouldRequestShutdownOnPowerOff() { return Config::REQUEST_PMU_SHUTDOWN_ON_POWEROFF; }

bool shouldReleaseBatteryPowerBeforeDeepSleep() {
  return Config::RELEASE_BATTERY_HOLD_BEFORE_DEEP_SLEEP;
}

}  // namespace Board::Power
