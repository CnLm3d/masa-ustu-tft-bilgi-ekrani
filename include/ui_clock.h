#pragma once
#include "config.h"
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_CLOCK_HOUR    = {SX(16),  SY(33),  SX(134), SY(134)};
constexpr Rect RECT_CLOCK_MIN     = {SX(170), SY(33),  SX(134), SY(134)};
constexpr Rect RECT_CLOCK_DATE_WX = {SX(16),  SY(170), SX(288), SY(30)};

void uiClockEnter(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx);
void uiClockTick(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx, bool fetching);
void uiClockTriggerDemo(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx);
