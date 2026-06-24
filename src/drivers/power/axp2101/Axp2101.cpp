#include "drivers/power/axp2101/Axp2101.h"

#include <Wire.h>

#include "drivers/power/BatteryCurve.h"

namespace BoardDrivers::Axp2101 {
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
    // The PWR key is the reader / hold-to-read button. The AXP2101 cannot fully ignore a long
    // press (hardware always does power-off OR restart), so pick the least-destructive option:
    // make a long press a recoverable RESTART (resumes reading) instead of a power-off, and push
    // the trigger to its 10 s maximum. Real power-off is the two-button combo (firmware soft-off).
    // Per XPowersLib, PWROFF_EN bit0 = 1 selects restart; reg 0x27 bits[3:2] set the off-level time.
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

}  // namespace BoardDrivers::Axp2101
