#pragma once
#include <Arduino.h>
#include <stdint.h>

enum class PageId : uint8_t {
    CLOCK = 0,
    WEATHER,
    MARKET,
    WORLD,
    ALARM,
    NEWS,
    POMODORO,
    MUSIC,
    MENU,
    SETTINGS,
    NIGHT,
    COUNT
};

enum class ClockTheme : uint8_t {
    FLIP = 0,
    NIXIE,
    DIGITAL,
    ROLEX,
    PATEK,
    MARIO,
    TETRIS,
    COUNT
};

enum class NewsTopic : uint8_t {
    GUNDEM = 0,
    TEKNOLOJI,
    EKONOMI,
    BILIM,
    SPOR,
    COUNT
};

enum class LedMode : uint8_t {
    AUTO = 0,
    RADIO_VU = 1,
    SOLID = 2,
    NIGHT = 3,
    OFF = 4,
    COUNT
};

constexpr size_t MAX_RADIO_STATIONS = 16;

struct RadioStation {
    char name[28];
    char url[128];
};

struct AlarmItem {
    uint8_t hour;
    uint8_t minute;
    bool enabled;
    uint8_t repeatMode; // 0=Her Gün, 1=Hafta İçi, 2=Hafta Sonu
    char label[24];
};

struct WorldCity {
    char name[20];
    int16_t utcOffsetMin;
};

struct DeviceConfig {
    char ssid[33];
    char pass[65];
    char city[32];
    float lat;
    float lon;
    uint8_t newsTopic;
    uint8_t clockTheme;
    uint8_t rotation; // 1 (landscape) or 3 (180 deg)
    bool nightEnabled;
    bool lightTheme;
    bool autoCycle;
    uint8_t nightStartHour;
    uint8_t nightEndHour;
    uint8_t buzzerPin;
    uint8_t volume;
    bool ledEnabled;
    uint8_t ledBrightness; // 10..80 (%)
    uint8_t ledMode;       // LedMode
    uint32_t ledColor;     // 0xRRGGBB for SOLID mode
    uint8_t radioStationCount;
    uint8_t radioActiveIdx;
    RadioStation radioStations[MAX_RADIO_STATIONS];
    AlarmItem alarms[3];
    WorldCity worldCities[4];
};

struct RadioState {
    bool isPlaying;
    bool connecting;
    bool hasError;
    uint8_t stationIdx;
    uint8_t stationCount;
    uint8_t volume;
    uint8_t vuLeft;
    uint8_t vuRight;
    uint32_t sampleRate;
    char stationName[32];
    char streamTitle[96];
    char statusMsg[48];
    uint32_t updatedAtMs;
};

struct ForecastDay {
    char dateIso[12];
    int wmoCode;
    float tempMax;
    float tempMin;
};

struct WeatherData {
    bool valid;
    bool hasError;
    char errorMsg[48];
    float tempC;
    float feelsLikeC;
    float windKmh;
    int humidity;
    int wmoCode;
    bool isDay;
    ForecastDay daily[4];
    uint32_t updatedAtMs;
};

struct MarketData {
    bool valid;
    bool hasError;
    char errorMsg[48];
    float usdTry;
    float eurTry;
    float gbpTry;
    float gramAltinTry;
    float onsAltinUsd;
    float usdChg;
    float eurChg;
    float gbpChg;
    float gramChg;
    float onsChg;
    uint32_t updatedAtMs;
};

constexpr size_t MAX_NEWS_ITEMS = 12;

struct NewsItem {
    char title[180];
    char source[40];
    char timeStr[16];
};

struct NewsData {
    bool valid;
    bool hasError;
    char errorMsg[48];
    uint8_t topic;
    uint8_t count;
    NewsItem items[MAX_NEWS_ITEMS];
    uint32_t updatedAtMs;
};

inline const char* const TR_DAYS_SHORT[] = {
    "Paz", "Pzt", "Sal", "Çar", "Per", "Cum", "Cmt"
};

inline const char* const TR_DAYS_LONG[] = {
    "Pazar", "Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi"
};

inline const char* const TR_MONTHS_SHORT[] = {
    "Oca", "Şub", "Mar", "Nis", "May", "Haz",
    "Tem", "Ağu", "Eyl", "Eki", "Kas", "Ara"
};

inline const char* const TR_TOPIC_NAMES[] = {
    "Gündem", "Teknoloji", "Ekonomi", "Bilim", "Spor"
};

inline const char* const TR_THEME_NAMES[] = {
    "Yaprak Saat",
    "Nixie Tüp",
    "Dijital LED",
    "Rolex Submariner",
    "Patek Nautilus",
    "Mario Saat",
    "Tetris Saat"
};
