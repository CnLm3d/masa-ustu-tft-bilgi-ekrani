#pragma once
#include "types.h"
#include <TFT_eSPI.h>

void uiClockEnter(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx);
void uiClockTick(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx, bool fetching);
void uiClockTriggerDemoFlip(TFT_eSPI& tft);
