#pragma once
#include "types.h"
#include <TFT_eSPI.h>

void uiWeatherDraw(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx, bool fetching);
