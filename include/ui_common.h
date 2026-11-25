#pragma once
#include "config.h"
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_TOP_PILL   = {212, 0, 56, 28};
constexpr Rect RECT_NAV_CLOCK  = {0, 208, 80, 32};
constexpr Rect RECT_NAV_MARKET = {80, 208, 80, 32};
constexpr Rect RECT_NAV_NEWS   = {160, 208, 80, 32};
constexpr Rect RECT_NAV_MENU   = {240, 208, 80, 32};

constexpr Rect RECT_SET_WIFI = {28, 48, 264, 44};
constexpr Rect RECT_SET_CAL  = {28, 104, 264, 44};
constexpr Rect RECT_SET_BACK = {28, 160, 264, 38};

constexpr Rect RECT_MENU_TILES[8] = {
    {9,   34, 71, 80}, // 0: Saat
    {86,  34, 71, 80}, // 1: Hava
    {163, 34, 71, 80}, // 2: Piyasa
    {240, 34, 71, 80}, // 3: Dünya
    {9,  120, 71, 80}, // 4: Alarm
    {86, 120, 71, 80}, // 5: Haber
    {163,120, 71, 80}, // 6: Odak (Pomodoro)
    {240,120, 71, 80}  // 7: Ayar
};

void drawTopBar(TFT_eSPI& tft, const char* title, const char* rightPill,
                int rssi, bool wifiOk, bool fetching);
void drawBottomNav(TFT_eSPI& tft, uint8_t activeTab);
void uiMenuDraw(TFT_eSPI& tft);
void uiSettingsDraw(TFT_eSPI& tft, const DeviceConfig& cfg);
void uiNightDraw(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx);

uint8_t drawWrappedUtf8(TFT_eSPI& tft, const String& text, int16_t x, int16_t y,
                        int16_t maxW, int16_t lineH, uint8_t maxLines,
                        uint16_t fg, uint16_t bg);
void drawWeatherIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, int wmoCode, bool isDay);
