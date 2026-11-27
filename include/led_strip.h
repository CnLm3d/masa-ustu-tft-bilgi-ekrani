#pragma once
#include "types.h"
#include "ui_pomodoro.h"
#include <stdint.h>

void ledInit(const DeviceConfig& cfg);
void ledUpdateConfig(const DeviceConfig& cfg);
void ledTriggerTest();
void ledShowOtaProgress(uint8_t pct);
void ledTick(uint32_t nowMs, const DeviceConfig& cfg, PageId page, bool alarmRinging,
             const PomodoroState& pomo, const RadioState& radio,
             const WeatherData& wx, const MarketData& mk, bool wifiOk);
