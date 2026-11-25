#pragma once
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_CLOCK_HOUR    = {16, 33, 134, 134};
constexpr Rect RECT_CLOCK_MIN     = {170, 33, 134, 134};
constexpr Rect RECT_CLOCK_DATE_WX = {16, 170, 288, 30};

void uiClockEnter(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx);
void uiClockTick(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx, bool fetching);
void uiClockTriggerDemo(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx);
