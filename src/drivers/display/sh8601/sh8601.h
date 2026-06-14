#pragma once

#include <stdint.h>

void sh8601Init();
void sh8601SetDisplayOn(bool on);
void sh8601SetBrightnessPercent(uint8_t percent);
void sh8601Sleep();
void sh8601Wake();
void sh8601PushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                      const uint16_t *data);
