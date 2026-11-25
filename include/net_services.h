#pragma once
#include "types.h"

void netInit(const DeviceConfig& cfg);
void netStartTask();

void netRequestWeatherRefresh();
void netRequestNewsRefresh(uint8_t topicIdx);
void netRequestTimeSync();

bool netCopyWeather(WeatherData& out);
bool netCopyNews(NewsData& out);
bool netIsFetching();
int netGetRssi();
bool netIsWifiConnected();

const char* wmoToTurkish(int code);
