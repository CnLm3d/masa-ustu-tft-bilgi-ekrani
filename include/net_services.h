#pragma once
#include "types.h"

enum class SpotifyCmd : uint8_t {
    PLAY_PAUSE = 0,
    NEXT,
    PREV,
    SET_VOLUME
};

void netInit(const DeviceConfig& cfg);
void netUpdateConfig(const DeviceConfig& cfg);
void netStartTask();
void netPause(bool paused);

void netRequestWeatherRefresh();
void netRequestNewsRefresh(uint8_t topicIdx);
void netRequestMarketRefresh();
void netRequestSpotifyRefresh();
void netSendSpotifyCmd(SpotifyCmd cmd, int volPct = 70);
void netRequestTimeSync();

bool netCopyWeather(WeatherData& out);
bool netCopyNews(NewsData& out);
bool netCopyMarket(MarketData& out);
bool netCopySpotify(MusicState& out);
bool netIsFetching();
int netGetRssi();
bool netIsWifiConnected();

const char* wmoToTurkish(int code);
