#pragma once
#include "config.h"
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_TOP_PILL   = {SX(212), 0, SX(56), SY(28)};
constexpr Rect RECT_NAV_CLOCK  = {0,       SY(208), SX(80), SY(32)};
constexpr Rect RECT_NAV_MUSIC  = {SX(80),  SY(208), SX(80), SY(32)};
constexpr Rect RECT_NAV_MARKET = {SX(160), SY(208), SX(80), SY(32)};
constexpr Rect RECT_NAV_MENU   = {SX(240), SY(208), SX(80), SY(32)};

constexpr Rect RECT_SET_WIFI = {SX(28), SY(48),  SX(264), SY(44)};
constexpr Rect RECT_SET_CAL  = {SX(28), SY(104), SX(264), SY(44)};
constexpr Rect RECT_SET_BACK = {SX(28), SY(160), SX(264), SY(38)};

constexpr Rect RECT_MENU_TILES[8] = {
    {SX(9),   SY(34),  SX(71), SY(80)}, // 0: Saat
    {SX(86),  SY(34),  SX(71), SY(80)}, // 1: Hava
    {SX(163), SY(34),  SX(71), SY(80)}, // 2: Piyasa
    {SX(240), SY(34),  SX(71), SY(80)}, // 3: Dünya
    {SX(9),   SY(120), SX(71), SY(80)}, // 4: Alarm
    {SX(86),  SY(120), SX(71), SY(80)}, // 5: Haber
    {SX(163), SY(120), SX(71), SY(80)}, // 6: Odak (Pomodoro)
    {SX(240), SY(120), SX(71), SY(80)}  // 7: Müzik (Spotify & BT)
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
