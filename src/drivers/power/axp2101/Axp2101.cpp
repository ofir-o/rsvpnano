#include "drivers/power/axp2101/Axp2101.h"

#include <Wire.h>
#include <esp_attr.h>

#include "drivers/power/BatteryCurve.h"

// ALDO rail state saved before a deep sleep that cut the peripheral rails. RTC_DATA_ATTR survives
// deep sleep but is re-initialized to the 0xFF "nothing cut" sentinel on a true power-on, so begin()
// only restores rails after a deep-sleep wake that actually cut them. (Mask is 4 bits, so 0xFF is a
// safe sentinel.)
RTC_DATA_ATTR static uint8_t gSavedAldoMaskForWake = 0xFF;

namespace BoardDrivers::Axp2101 {

// Runtime opt-in (a Settings toggle, off by default). Set each boot by the app from the saved
// preference. When false, cutAldoRailsForDeepSleep() does nothing.
static bool gDeepSleepRailCutEnabled = false;

namespace {

constexpr uint8_t kAddress = 0x34;
constexpr uint8_t kStatus1Reg = 0x00;
constexpr uint8_t kStatus2Reg = 0x01;
constexpr uint8_t kCommonConfigReg = 0x10;
constexpr uint8_t kAdcChannelCtrlReg = 0x30;
constexpr uint8_t kPowerOffEnableReg = 0x22;
constexpr uint8_t kIrqOffOnLevelCtrlReg = 0x27;
constexpr uint8_t kIrqEnable2Reg = 0x41;
constexpr uint8_t kIrqStatus2Reg = 0x49;
constexpr uint8_t kBatteryVoltageHighReg = 0x34;
constexpr uint8_t kBatteryVoltageLowReg = 0x35;
constexpr uint8_t kBatteryDetectCtrlReg = 0x68;
constexpr uint8_t kBatteryPercentReg = 0xA4;
// Charger configuration registers (verified against XPowersLib AXP2101 constants).
constexpr uint8_t kTsPinCtrlReg = 0x50;          // TS (battery temp) pin control
constexpr uint8_t kChargerCtrlReg = 0x18;        // charge/gauge/watchdog control; bit1 = charge enable
constexpr uint8_t kChargeVoltageReg = 0x64;      // constant-voltage target [2:0]
constexpr uint8_t kChargeCurrentReg = 0x62;      // constant charge current [4:0]
constexpr uint8_t kPrechargeCurrentReg = 0x61;   // precharge current [3:0]
constexpr uint8_t kTerminationCurrentReg = 0x63; // termination current [3:0]
// LDO on/off control 0 (verified against XPowersLib AXP2101 constants): one enable bit per rail.
constexpr uint8_t kLdoOnOffCtrl0Reg = 0x90;
constexpr uint8_t kAldoRailMask = 0x0F; // ALDO1=bit0, ALDO2=bit1, ALDO3=bit2, ALDO4=bit3
constexpr uint8_t kPowerKeyIrqMask = 0x0F;
constexpr uint8_t kPowerKeyPositiveIrqMask = 0x01;
constexpr uint8_t kPowerKeyNegativeIrqMask = 0x02;
constexpr uint8_t kPowerKeyLongIrqMask = 0x04;
constexpr uint8_t kPowerKeyShortIrqMask = 0x08;
constexpr uint8_t kLongPressShutdownMask = 0x02;
constexpr uint8_t kLongPressRestartMask = 0x01;
constexpr uint8_t kPowerKeyTimingMask = 0x0F;
constexpr uint32_t kPowerKeyPollIntervalMs = 20;

struct Context {
  bool ready = false;
  bool touchRailRecovered = false;
  bool powerButtonHeld = false;
  bool shortPressPending = false;
  bool longPressPending = false;
  uint32_t lastPowerKeyPollMs = 0;
  Board::Power::DiagnosticSnapshot diagnosticSnapshot;
};

Context gContext;

bool readRegister(uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  if (Wire.endTransmission(Board::Config::AXP2101_RELEASE_BUS_BEFORE_READ) != 0) {
    return false;
  }
  if (Board::Config::AXP2101_RELEASE_BUS_BEFORE_READ) {
    delayMicroseconds(50);
  }
  if (Wire.requestFrom(kAddress, static_cast<uint8_t>(1)) != 1) {
    return false;
  }

  value = Wire.read();
  return true;
}

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission(true) == 0;
}

bool updateRegisterBits(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current = 0;
  if (!readRegister(reg, current)) {
    return false;
  }

  current = static_cast<uint8_t>((current & static_cast<uint8_t>(~mask)) | (value & mask));
  return writeRegister(reg, current);
}

uint16_t readH5L8(uint8_t highReg, uint8_t lowReg) {
  uint8_t high = 0;
  uint8_t low = 0;
  if (!readRegister(highReg, high) || !readRegister(lowReg, low)) {
    return 0;
  }

  return static_cast<uint16_t>(((high & 0x1F) << 8) | low);
}

void captureDiagnostics(uint8_t status1) {
  if (gContext.diagnosticSnapshot.available) {
    return;
  }

  Board::Power::DiagnosticSnapshot snapshot;
  snapshot.available = true;
  snapshot.status1 = status1;

  uint8_t status2 = 0;
  if (readRegister(kStatus2Reg, status2)) {
    snapshot.status2 = status2;
    snapshot.externalPowerPresent = (status1 & 0x20) != 0 && (status2 & 0x08) == 0;
  }

  uint8_t irqStatus2 = 0;
  if (readRegister(kIrqStatus2Reg, irqStatus2)) {
    snapshot.powerKeyIrqStatus = irqStatus2;
  }

  gContext.diagnosticSnapshot = snapshot;
}

bool batteryConnected() {
  uint8_t status1 = 0;
  return readRegister(kStatus1Reg, status1) && (status1 & 0x08) != 0;
}

}  // namespace

bool begin() {
  uint8_t value = 0;
  if (!readRegister(kStatus1Reg, value)) {
    if (gContext.ready) {
      Serial.println("[board] AXP2101 no longer responding");
    }
    gContext.ready = false;
    return false;
  }

  gContext.ready = true;
  captureDiagnostics(value);

  if (gSavedAldoMaskForWake != 0xFF) {
    // We cut the ALDO peripheral rails before the deep sleep that this boot is waking from. Restore
    // them now -- before any display/touch init -- so those controllers have power again.
    uint8_t ldoState = 0;
    if (readRegister(kLdoOnOffCtrl0Reg, ldoState)) {
      const uint8_t restored = static_cast<uint8_t>((ldoState & ~kAldoRailMask) |
                                                    (gSavedAldoMaskForWake & kAldoRailMask));
      writeRegister(kLdoOnOffCtrl0Reg, restored);
      Serial.printf("[board] AXP2101 deep-sleep wake: restored ALDO rails 0x%02X\n",
                    gSavedAldoMaskForWake & kAldoRailMask);
      delay(50);  // let the peripheral rails settle before they are initialized
    }
    gSavedAldoMaskForWake = 0xFF;
  }

  if (!updateRegisterBits(kAdcChannelCtrlReg, 0x01, 0x01)) {
    Serial.println("[board] AXP2101 battery voltage ADC enable failed");
  }
  if (!updateRegisterBits(kBatteryDetectCtrlReg, 0x01, 0x01)) {
    Serial.println("[board] AXP2101 battery detect enable failed");
  }

  if (Board::Config::AXP2101_CONFIGURE_CHARGER) {
    // The board has a LiPo but no NTC thermistor on the TS pin. Detach the TS pin from the charger
    // (matches XPowersLib disableTSPinMeasure: reg 0x50 = (val & 0xF0) | 0x10, and clear ADC bit1),
    // otherwise the PMU reads an out-of-range temperature and refuses to charge.
    updateRegisterBits(kTsPinCtrlReg, 0x1F, 0x10);
    updateRegisterBits(kAdcChannelCtrlReg, 0x02, 0x00);
    // Single-cell 4.2 V LiPo charge profile (CV 4.2 V, CC 200 mA, precharge 50 mA, term 25 mA).
    updateRegisterBits(kChargeVoltageReg, 0x07, 0x03);
    updateRegisterBits(kChargeCurrentReg, 0x1F, 0x08);
    updateRegisterBits(kPrechargeCurrentReg, 0x0F, 0x02);
    updateRegisterBits(kTerminationCurrentReg, 0x0F, 0x01);
    if (!updateRegisterBits(kChargerCtrlReg, 0x02, 0x02)) {
      Serial.println("[board] AXP2101 charge enable failed");
    }
  }

  if (Board::Config::AXP2101_RECOVER_TOUCH_POWER_RAIL && !gContext.touchRailRecovered) {
    // Run exactly once per boot: a later begin() re-init must not blip the display rail mid-use.
    gContext.touchRailRecovered = true;
    // Cold-boot the peripheral 3.x V rails so a brown-out-latched touch controller restarts from
    // zero. We only ever toggle the ALDO rails (ALDO1..ALDO4); DCDC1 (the ESP32 system rail) is
    // never touched, so there is no brick risk, and the display/RTC/IMU re-initialize afterwards.
    // This runs before any display or touch init, so the brief power blip on those rails is
    // indistinguishable from a normal cold boot.
    uint8_t ldoState = 0;
    if (readRegister(kLdoOnOffCtrl0Reg, ldoState)) {
      const uint8_t aldoOn = static_cast<uint8_t>(ldoState & kAldoRailMask);
      if (aldoOn != 0) {
        Serial.printf("[board] AXP2101 cold-booting touch rail: 0x90=0x%02X aldo=0x%02X\n", ldoState,
                      aldoOn);
        writeRegister(kLdoOnOffCtrl0Reg, static_cast<uint8_t>(ldoState & ~aldoOn));
        delay(200);
        writeRegister(kLdoOnOffCtrl0Reg, ldoState);
        delay(200);
      } else {
        Serial.println("[board] AXP2101 touch-rail recovery skipped (no ALDO rails enabled)");
      }
    } else {
      Serial.println("[board] AXP2101 touch-rail recovery failed to read 0x90");
    }
  }

  const uint8_t irqValue = Board::Config::AXP2101_ENABLE_POWER_KEY_IRQS ? kPowerKeyIrqMask : 0x00;
  if (!updateRegisterBits(kIrqEnable2Reg, kPowerKeyIrqMask, irqValue)) {
    Serial.println("[board] AXP2101 power-key IRQ setup failed");
  }

  if (Board::Config::PMU_REQUIRES_POWER_KEY_CONFIG) {
    const uint8_t powerKeyTiming =
        static_cast<uint8_t>((Board::Config::PMU_POWER_KEY_ON_TIME_VALUE & 0x03) |
                             ((Board::Config::PMU_POWER_KEY_OFF_TIME_VALUE & 0x03) << 2));
    if (!updateRegisterBits(kIrqOffOnLevelCtrlReg, kPowerKeyTimingMask, powerKeyTiming)) {
      Serial.println("[board] AXP2101 power-key timing config failed");
    }
    if (!updateRegisterBits(
            kPowerOffEnableReg, static_cast<uint8_t>(kLongPressShutdownMask | kLongPressRestartMask),
            kLongPressShutdownMask)) {
      Serial.println("[board] AXP2101 long-press shutdown enable failed");
    }
  }

  if (Board::Config::AXP2101_DISABLE_LONG_PRESS_POWEROFF) {
    // The big PWR key is the reader / hold-to-read button, so a long hold must NOT power the device
    // off or restart it. The AXP2101 power-key long-press has a MASTER ENABLE in PWROFF_EN bit1
    // (XPowersLib disableLongPressShutdown): clear it so a long hold does nothing at all. Real
    // power-off is the two-button firmware combo (clean AXP shutdown). Also select the
    // non-destructive restart mode (bit0=1) and the 10 s maximum trigger time as belt-and-suspenders
    // in case any boot path re-enables the long-press. Powering the device ON via the PWR key is a
    // separate on-key function and is unaffected.
    updateRegisterBits(kPowerOffEnableReg, kLongPressShutdownMask, 0x00);
    updateRegisterBits(kPowerOffEnableReg, kLongPressRestartMask, kLongPressRestartMask);
    updateRegisterBits(kIrqOffOnLevelCtrlReg, 0x0C, 0x0C);
  }

  gContext.powerButtonHeld = false;
  gContext.shortPressPending = false;
  gContext.longPressPending = false;
  gContext.lastPowerKeyPollMs = 0;
  writeRegister(kIrqStatus2Reg, 0xFF);
  return true;
}

bool readBatteryStatus(Board::Power::BatteryStatus &status) {
  if (!gContext.ready && !begin()) {
    return false;
  }

  status = Board::Power::BatteryStatus{};
  status.present = batteryConnected();
  if (!status.present) {
    return false;
  }

  const uint16_t millivolts = readH5L8(kBatteryVoltageHighReg, kBatteryVoltageLowReg);
  if (millivolts == 0) {
    return false;
  }

  status.voltage = static_cast<float>(millivolts) / 1000.0f;
  uint8_t percent = 0;
  if (readRegister(kBatteryPercentReg, percent) && percent <= 100) {
    status.percent = percent;
  } else {
    status.percent = BatteryCurve::percentForVoltage(status.voltage);
  }
  return true;
}

Board::Power::DiagnosticSnapshot diagnosticSnapshot() { return gContext.diagnosticSnapshot; }

bool externalPowerPresent() {
  if (!gContext.ready && !begin()) {
    return false;
  }

  uint8_t status1 = 0;
  uint8_t status2 = 0;
  if (!readRegister(kStatus1Reg, status1) || !readRegister(kStatus2Reg, status2)) {
    return false;
  }

  return (status1 & 0x20) != 0 && (status2 & 0x08) == 0;
}

bool releasePower() {
  if (!gContext.ready && !begin()) {
    return false;
  }

  if (!updateRegisterBits(kCommonConfigReg, 0x01, 0x01)) {
    Serial.println("[board] AXP2101 shutdown request failed");
    return false;
  }

  Serial.println("[board] AXP2101 shutdown requested");
  return true;
}

void pollPowerKeyIfDue(bool force) {
  if (!Board::Config::AXP2101_ENABLE_POWER_KEY_IRQS) {
    return;
  }

  const uint32_t nowMs = millis();
  if (!force && (nowMs - gContext.lastPowerKeyPollMs) < kPowerKeyPollIntervalMs) {
    return;
  }
  gContext.lastPowerKeyPollMs = nowMs;

  if (!gContext.ready && !begin()) {
    return;
  }

  uint8_t status2 = 0;
  if (!readRegister(kIrqStatus2Reg, status2)) {
    return;
  }

  if ((status2 & kPowerKeyNegativeIrqMask) != 0) {
    gContext.powerButtonHeld = true;
  }
  if ((status2 & kPowerKeyPositiveIrqMask) != 0) {
    gContext.powerButtonHeld = false;
  }
  if ((status2 & kPowerKeyLongIrqMask) != 0) {
    gContext.longPressPending = true;
  }
  if ((status2 & kPowerKeyShortIrqMask) != 0) {
    gContext.shortPressPending = true;
  }
  if (status2 != 0) {
    writeRegister(kIrqStatus2Reg, status2);
  }
}

bool isPowerButtonHeld() {
  pollPowerKeyIfDue();
  return gContext.powerButtonHeld;
}

bool consumeShortPress() {
  pollPowerKeyIfDue();
  const bool pending = gContext.shortPressPending;
  gContext.shortPressPending = false;
  return pending;
}

bool consumeLongPress() {
  pollPowerKeyIfDue();
  const bool pending = gContext.longPressPending;
  gContext.longPressPending = false;
  return pending;
}

void setDeepSleepRailCutEnabled(bool enabled) { gDeepSleepRailCutEnabled = enabled; }

void cutAldoRailsForDeepSleep() {
  if (!gDeepSleepRailCutEnabled) {
    return;
  }
  if (!gContext.ready && !begin()) {
    return;
  }
  uint8_t ldoState = 0;
  if (!readRegister(kLdoOnOffCtrl0Reg, ldoState)) {
    return;
  }
  // Remember which ALDO rails were on so the wake boot can restore exactly those, then cut them.
  // DCDC1 (the ESP32 system rail) lives in a different register and is never touched here.
  gSavedAldoMaskForWake = static_cast<uint8_t>(ldoState & kAldoRailMask);
  writeRegister(kLdoOnOffCtrl0Reg, static_cast<uint8_t>(ldoState & ~kAldoRailMask));
  Serial.printf("[board] AXP2101 deep-sleep: cut ALDO rails (saved 0x%02X)\n",
                gSavedAldoMaskForWake);
}

}  // namespace BoardDrivers::Axp2101
