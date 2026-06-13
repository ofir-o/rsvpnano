#pragma once

#include <Arduino.h>

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349) && \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
#error "Select only one RSVP board target."
#endif

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349) && \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
#error "Select only one RSVP board target."
#endif

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349) && \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42)
#error "Select only one RSVP board target."
#endif

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349) && \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
#error "Select only one RSVP board target."
#endif

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42) && \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
#error "Select only one RSVP board target."
#endif

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42) && \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
#error "Select only one RSVP board target."
#endif

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42) && \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
#error "Select only one RSVP board target."
#endif

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241) && \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
#error "Select only one RSVP board target."
#endif

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241) && \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
#error "Select only one RSVP board target."
#endif

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18) && \
    defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
#error "Select only one RSVP board target."
#endif

#if !defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349) && \
    !defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42) && \
    !defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18) && \
    !defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241) && \
    !defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
#define RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349 1
#endif

namespace BoardConfig {

enum class UiOrientation : uint8_t {
  Landscape = 0,
  LandscapeFlipped,
  Portrait,
  PortraitFlipped,
};

enum class DisplayDriverKind : uint8_t {
  Axs15231b = 0,
  Sh8601,
  Rm690b0,
  Co5300,
};

enum class TouchControllerKind : uint8_t {
  Axs15231b = 0,
  Ft6336,
  Cst92xx,
};

enum class StorageBusKind : uint8_t {
  SdMmc1Bit = 0,
  SdSpi,
};

enum class PowerManagerKind : uint8_t {
  Tca9554 = 0,
  Axp2101,
  DirectGpioBatteryHold,
};

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
#include "board/profiles/WaveshareEsp32S3TouchAmoled216Profile.h"
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
#include "board/profiles/WaveshareEsp32S3TouchAmoled18Profile.h"
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
#include "board/profiles/WaveshareEsp32S3TouchAmoled241Profile.h"
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42)
#include "board/profiles/WaveshareEsp32S3TouchLcd349Gpio42Profile.h"
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349)
#include "board/profiles/WaveshareEsp32S3TouchLcd349Profile.h"
#else
#error "Unsupported RSVP board target."
#endif

struct BatteryStatus {
  bool present = false;
  float voltage = 0.0f;
  uint8_t percent = 0;
};

struct PowerDiagnosticSnapshot {
  bool available = false;
  bool externalPowerPresent = false;
  uint8_t status1 = 0;
  uint8_t status2 = 0;
  uint8_t powerKeyIrqStatus = 0;
};

void begin();
void lightSleepUntilBootButton();
void holdBacklightOffForDeepSleep();
void resetWakePeripherals();
void resetTouchController();
bool readBatteryStatus(BatteryStatus &status);
PowerDiagnosticSnapshot powerDiagnosticSnapshot();
bool externalPowerPresent();
bool releaseBatteryPowerHold();
bool readVirtualBootButtonHeld();
bool readVirtualPowerButtonHeld();
bool consumeVirtualPowerButtonShortPressEvent();
bool consumeVirtualPowerButtonLongPressEvent();

}  // namespace BoardConfig
