#pragma once
#include "types.h"
#include <TFT_eSPI.h>

void drawTopBar(TFT_eSPI& tft, PageId page, const char* title, int rssi, bool wifiOk, bool fetching);
void drawBottomHint(TFT_eSPI& tft, const char* leftHint, const char* rightHint);
uint8_t drawWrappedUtf8(TFT_eSPI& tft, const String& text, int16_t x, int16_t y,
                        int16_t maxW, int16_t lineH, uint8_t maxLines,
                        uint16_t fg, uint16_t bg);
void drawWeatherIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, int wmoCode, bool isDay);
