#pragma once
#include "types.h"
#include <TFT_eSPI.h>

void uiThemesReset();
void uiThemesTick(TFT_eSPI& tft, ClockTheme theme, const DeviceConfig& cfg,
                  const WeatherData& wx, bool fetching, bool forceFull);
void uiThemesDemo(TFT_eSPI& tft, ClockTheme theme, const DeviceConfig& cfg,
                  const WeatherData& wx);
