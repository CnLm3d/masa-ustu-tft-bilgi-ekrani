#pragma once
#include "config.h"
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr uint8_t SLIDER_PAGE_COUNT = 9;

constexpr Rect RECT_TOP_PILL   = {SX(212), 0, SX(56), SY(28)};
constexpr Rect RECT_NAV_SLIDER = {SX(4), SY(206), SCREEN_W - SX(8), SCREEN_H - SY(206)};

constexpr Rect RECT_SET_WIFI  = {SX(24), SY(36),  SX(272), SY(36)};
constexpr Rect RECT_SET_CAL   = {SX(24), SY(78),  SX(272), SY(36)};
constexpr Rect RECT_SET_THEME = {SX(24), SY(120), SX(272), SY(36)};
constexpr Rect RECT_SET_BACK  = {SX(24), SY(162), SX(272), SY(34)};

constexpr Rect RECT_MENU_TILES[8] = {
    {SX(9),   SY(30),  SX(71), SY(68)}, // 0: Saat
    {SX(86),  SY(30),  SX(71), SY(68)}, // 1: Hava
    {SX(163), SY(30),  SX(71), SY(68)}, // 2: Piyasa
    {SX(240), SY(30),  SX(71), SY(68)}, // 3: Dünya
    {SX(9),   SY(104), SX(71), SY(68)}, // 4: Alarm
    {SX(86),  SY(104), SX(71), SY(68)}, // 5: Haber
    {SX(163), SY(104), SX(71), SY(68)}, // 6: Odak (Pomodoro)
    {SX(240), SY(104), SX(71), SY(68)}  // 7: Radyo (İnternet Radyosu)
};

constexpr Rect RECT_MENU_CYCLE = {SX(9),   SY(178), SX(148), SY(26)};
constexpr Rect RECT_MENU_THEME = {SX(163), SY(178), SX(148), SY(26)};

void drawTopBar(TFT_eSPI& tft, const char* title, const char* rightPill,
                int rssi, bool wifiOk, bool fetching);
void drawBottomNav(TFT_eSPI& tft, uint8_t activePageIdx, bool dragging = false);
uint8_t uiSliderIndexFromX(int16_t px);
PageId uiSliderPageAt(uint8_t idx);

void uiMenuDraw(TFT_eSPI& tft, const DeviceConfig& cfg);
void uiSettingsDraw(TFT_eSPI& tft, const DeviceConfig& cfg);
void uiNightDraw(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx);
void uiDrawOtaScreen(TFT_eSPI& tft, uint8_t pct, const char* msg);

uint8_t drawWrappedUtf8(TFT_eSPI& tft, const String& text, int16_t x, int16_t y,
                        int16_t maxW, int16_t lineH, uint8_t maxLines,
                        uint16_t fg, uint16_t bg);
void drawWeatherIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, int wmoCode, bool isDay);
