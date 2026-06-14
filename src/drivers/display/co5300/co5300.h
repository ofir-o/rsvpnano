#pragma once

#include <stdint.h>

void co5300Init();
void co5300SetDisplayOn(bool on);
void co5300SetBrightnessPercent(uint8_t percent);
void co5300Sleep();
void co5300Wake();
void co5300PushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                      const uint16_t *data);
