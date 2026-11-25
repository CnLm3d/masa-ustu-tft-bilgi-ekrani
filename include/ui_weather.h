#pragma once
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_WEATHER_REFRESH = {12, 32, 296, 88};

void uiWeatherDraw(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx, bool fetching);
