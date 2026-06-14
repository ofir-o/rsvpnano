#pragma once

#include <Arduino.h>

void rm690b0Init();
void rm690b0SetDisplayOn(bool on);
void rm690b0SetBrightnessPercent(uint8_t percent);
void rm690b0Sleep();
void rm690b0Wake();
void rm690b0PushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                       const uint16_t *data);
