#pragma once
#include "config.h"
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_TOP_SETTINGS = {210, 0, 62, 30};
constexpr Rect RECT_TAB_CLOCK    = {0, 204, 107, 36};
constexpr Rect RECT_TAB_WEATHER  = {107, 204, 106, 36};
constexpr Rect RECT_TAB_NEWS     = {213, 204, 107, 36};

constexpr Rect RECT_SET_WIFI = {28, 48, 264, 44};
constexpr Rect RECT_SET_CAL  = {28, 104, 264, 44};
constexpr Rect RECT_SET_BACK = {28, 160, 264, 38};

void drawTopBar(TFT_eSPI& tft, const char* title, int rssi, bool wifiOk, bool fetching);
void drawBottomTabs(TFT_eSPI& tft, PageId activePage);
void uiSettingsDraw(TFT_eSPI& tft, const DeviceConfig& cfg);

uint8_t drawWrappedUtf8(TFT_eSPI& tft, const String& text, int16_t x, int16_t y,
                        int16_t maxW, int16_t lineH, uint8_t maxLines,
                        uint16_t fg, uint16_t bg);
void drawWeatherIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, int wmoCode, bool isDay);
