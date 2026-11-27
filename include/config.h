#pragma once
#include "ui_theme.h"
#include <stdint.h>

// Responsive scaling macros: 320x240 (ILI9341 2.4") vs 480x320 (Waveshare 4" ILI9486)
#ifdef SCREEN_480X320
constexpr int16_t SCREEN_W = 480;
constexpr int16_t SCREEN_H = 320;
#define SX(x) (static_cast<int16_t>(((x) * 3) / 2))
#define SY(y) (static_cast<int16_t>(((y) * 4) / 3))
#define SR(r) (static_cast<int16_t>(((r) * 4) / 3))
#else
constexpr int16_t SCREEN_W = 320;
constexpr int16_t SCREEN_H = 240;
#define SX(x) (static_cast<int16_t>(x))
#define SY(y) (static_cast<int16_t>(y))
#define SR(r) (static_cast<int16_t>(r))
#endif

// Resistive Touch (XPT2046 on shared SPI)
constexpr uint16_t TOUCH_THRESHOLD   = 350;
constexpr uint32_t TOUCH_COOLDOWN_MS = 180;
constexpr int16_t  SWIPE_MIN_PX      = SX(48);

// Layout bars
constexpr int16_t TOP_BAR_H = SY(24);
constexpr int16_t NAV_BAR_Y = SY(210);
constexpr int16_t NAV_BAR_H = SY(26);

// PCM5102A I2S DAC pins (ESP32 DevKit)
constexpr int8_t PIN_I2S_BCK  = 26;
constexpr int8_t PIN_I2S_WS   = 25;
constexpr int8_t PIN_I2S_DOUT = 22;

// Optional passive buzzer GPIO (0 = disabled, I2S chime used on PCM5102A by default)
constexpr uint8_t DEFAULT_BUZZER_PIN = 0;

// Default Wi-Fi, Location & Audio
constexpr char DEFAULT_WIFI_SSID[] = "WIFI_ADINIZ";
constexpr char DEFAULT_WIFI_PASS[] = "WIFI_SIFRENIZ";
constexpr char DEFAULT_CITY_NAME[] = "İstanbul";
constexpr float DEFAULT_CITY_LAT   = 41.0082f;
constexpr float DEFAULT_CITY_LON   = 28.9784f;
constexpr char DEFAULT_TZ[]        = "TRT-3";
constexpr char NTP_SERVER_1[]      = "pool.ntp.org";
constexpr char NTP_SERVER_2[]      = "tr.pool.ntp.org";
constexpr char MDNS_HOSTNAME[]     = "deskhub";
constexpr char DEFAULT_BT_NAME[]   = "ESP32-DeskHub";

// Refresh & UI timings
constexpr uint32_t WEATHER_REFRESH_MS      = 15UL * 60UL * 1000UL;
constexpr uint32_t NEWS_REFRESH_MS         = 10UL * 60UL * 1000UL;
constexpr uint32_t MARKET_REFRESH_MS       = 10UL * 60UL * 1000UL;
constexpr uint32_t SPOTIFY_REFRESH_MS      = 5000UL;
constexpr uint32_t AUTO_CYCLE_MS           = 45000UL;
constexpr uint32_t NEWS_AUTO_CYCLE_MS      = 12000UL;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000UL;

// Dynamic Dark / Light RGB565 theme palette macros
#define COL_BG        (uiPalette().bg)
#define COL_SURFACE   (uiPalette().surface)
#define COL_CARD_TOP  (uiPalette().cardTop)
#define COL_CARD_BOT  (uiPalette().cardBot)
#define COL_BORDER    (uiPalette().border)
#define COL_HINGE     (uiPalette().hinge)
#define COL_ACCENT    (uiPalette().accent)
#define COL_ACCENT_2  (uiPalette().accent2)
#define COL_TEXT      (uiPalette().text)
#define COL_MUTED     (uiPalette().muted)
#define COL_ON_ACCENT (uiPalette().onAccent)
#define COL_OK        (uiPalette().ok)
#define COL_WARN      (uiPalette().warn)
#define COL_ERR       (uiPalette().err)
#define COL_GOLD      (uiPalette().gold)
#define COL_CYAN      (uiPalette().cyan)
#define COL_PURPLE    (uiPalette().purple)
#define COL_NIGHT_RED (uiPalette().nightRed)
#define COL_SPOTIFY   (uiPalette().spotify)
