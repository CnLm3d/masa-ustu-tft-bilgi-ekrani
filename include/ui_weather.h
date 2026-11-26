#pragma once
#include "config.h"
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_WEATHER_REFRESH = {SX(12), SY(32), SX(296), SY(88)};

void uiWeatherDraw(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx, bool fetching);
