#include "display/BoardDisplay.h"

#include "board/BoardConfig.h"

#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
#include "display/co5300.h"
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
#include "display/sh8601.h"
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
#include "display/rm690b0.h"
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349) || defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42)
#include "display/axs15231b.h"
#endif

bool boardDisplayInit() {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
  co5300Init();
  return true;
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  sh8601Init();
  return true;
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
  rm690b0Init();
  return true;
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349) || defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42)
  axs15231bInit();
  return true;
#endif
  return false;
}

void boardDisplaySetBacklight(bool on) {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
  co5300SetDisplayOn(on);
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  sh8601SetDisplayOn(on);
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
  rm690b0SetDisplayOn(on);
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349) || defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42)
  axs15231bSetBacklight(on);
#endif
}

void boardDisplaySetBrightnessPercent(uint8_t percent) {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
  co5300SetBrightnessPercent(percent);
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  sh8601SetBrightnessPercent(percent);
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
  rm690b0SetBrightnessPercent(percent);
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349) || defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42)
  axs15231bSetBrightnessPercent(percent);
#endif
}

void boardDisplaySleep() {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
  co5300Sleep();
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  sh8601Sleep();
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
  rm690b0Sleep();
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349) || defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42)
  axs15231bSleep();
#endif
}

void boardDisplayWake() {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
  co5300Wake();
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  sh8601Wake();
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
  rm690b0Wake();
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349) || defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42)
  axs15231bWake();
#endif
}

bool boardDisplayPushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                            const uint16_t *data) {
#if defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_216)
  co5300PushColors(x, y, width, height, data);
  return true;
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_18)
  sh8601PushColors(x, y, width, height, data);
  return true;
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_AMOLED_241)
  rm690b0PushColors(x, y, width, height, data);
  return true;
#elif defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349) || defined(RSVP_BOARD_WAVESHARE_ESP32S3_TOUCH_LCD_349_GPIO42)
  axs15231bPushColors(x, y, width, height, data);
  return true;
#endif
  return false;
}
