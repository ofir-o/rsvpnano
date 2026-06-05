#include <Arduino.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include "app/App.h"
#include "board/BoardConfig.h"

App app;

namespace {

const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "poweron";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "interrupt_watchdog";
    case ESP_RST_TASK_WDT:
      return "task_watchdog";
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_DEEPSLEEP:
      return "deep_sleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
    case ESP_RST_UNKNOWN:
    default:
      return "unknown";
  }
}

const char *wakeupCauseName(esp_sleep_wakeup_cause_t cause) {
  switch (cause) {
    case ESP_SLEEP_WAKEUP_EXT0:
      return "ext0";
    case ESP_SLEEP_WAKEUP_EXT1:
      return "ext1";
    case ESP_SLEEP_WAKEUP_TIMER:
      return "timer";
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      return "touchpad";
    case ESP_SLEEP_WAKEUP_ULP:
      return "ulp";
    case ESP_SLEEP_WAKEUP_GPIO:
      return "gpio";
    case ESP_SLEEP_WAKEUP_UART:
      return "uart";
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      return "undefined";
  }
}

void logStartupDiagnostics() {
  const esp_reset_reason_t resetReason = esp_reset_reason();
  const esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
  Serial.printf("[diag] reset=%s(%d) sleep_wake=%s(%d)\n", resetReasonName(resetReason),
                static_cast<int>(resetReason), wakeupCauseName(wakeupCause),
                static_cast<int>(wakeupCause));

  const BoardConfig::PowerDiagnosticSnapshot power = BoardConfig::powerDiagnosticSnapshot();
  if (!power.available) {
    Serial.println("[diag] power_snapshot=unavailable");
    return;
  }

  Serial.printf("[diag] power_snapshot=vbus:%u axp_status1:0x%02X axp_status2:0x%02X "
                "axp_pwr_irq:0x%02X\n",
                power.externalPowerPresent ? 1 : 0, power.status1, power.status2,
                power.powerKeyIrqStatus);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);
  delay(50);
  BoardConfig::begin();
  const uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 2000) {
    delay(10);
  }
  logStartupDiagnostics();
  Serial.println("[main] app setup");
  app.begin();
}

void loop() {
  const uint32_t now = millis();
  app.update(now);
}
