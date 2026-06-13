#pragma once

#include <Arduino.h>

bool boardDisplayInit();
void boardDisplaySetBacklight(bool on);
void boardDisplaySetBrightnessPercent(uint8_t percent);
void boardDisplaySleep();
void boardDisplayWake();
bool boardDisplayPushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                            const uint16_t *data);
