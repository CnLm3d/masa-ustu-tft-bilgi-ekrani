#pragma once
#include "types.h"

void netInit(const DeviceConfig& cfg);
void netUpdateConfig(const DeviceConfig& cfg);
void netStartTask();
void netPause(bool paused);

void netRequestWeatherRefresh();
void netRequestNewsRefresh(uint8_t topicIdx);
void netRequestMarketRefresh();
void netRequestTimeSync();

bool netCopyWeather(WeatherData& out);
bool netCopyNews(NewsData& out);
bool netCopyMarket(MarketData& out);
bool netIsFetching();
int netGetRssi();
bool netIsWifiConnected();

const char* wmoToTurkish(int code);
