#pragma once
#include <Arduino.h>
#include <stdint.h>

enum class PageId : uint8_t {
    CLOCK = 0,
    WEATHER,
    NEWS,
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

struct DeviceConfig {
    char ssid[33];
    char pass[65];
    char city[32];
    float lat;
    float lon;
    uint8_t newsTopic;
};

struct ForecastDay {
    char dateIso[12]; // YYYY-MM-DD
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
    ForecastDay daily[4]; // [0]=today, [1..3]=next 3 days
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
