#pragma once
#include <stdint.h>

// Screen dimensions (landscape rotation 1 or 3 on 240x320 ILI9341)
constexpr int16_t SCREEN_W = 320;
constexpr int16_t SCREEN_H = 240;

// Resistive Touch (XPT2046 on shared SPI)
constexpr uint16_t TOUCH_THRESHOLD   = 350;
constexpr uint32_t TOUCH_COOLDOWN_MS = 180;
constexpr int16_t  SWIPE_MIN_PX      = 48;

// Layout bars
constexpr int16_t TOP_BAR_H = 24;
constexpr int16_t NAV_BAR_Y = 210;
constexpr int16_t NAV_BAR_H = 28;

// Optional passive buzzer GPIO (0 = disabled, configurable via Web UI)
constexpr uint8_t DEFAULT_BUZZER_PIN = 0;

// Default Wi-Fi & Location (overwritten via NVS / Setup Portal / Web UI)
constexpr char DEFAULT_WIFI_SSID[] = "WIFI_ADINIZ";
constexpr char DEFAULT_WIFI_PASS[] = "WIFI_SIFRENIZ";
constexpr char DEFAULT_CITY_NAME[] = "İstanbul";
constexpr float DEFAULT_CITY_LAT   = 41.0082f;
constexpr float DEFAULT_CITY_LON   = 28.9784f;
constexpr char DEFAULT_TZ[]        = "TRT-3";
constexpr char NTP_SERVER_1[]      = "pool.ntp.org";
constexpr char NTP_SERVER_2[]      = "tr.pool.ntp.org";
constexpr char MDNS_HOSTNAME[]     = "deskhub";

// Refresh & UI timings
constexpr uint32_t WEATHER_REFRESH_MS      = 15UL * 60UL * 1000UL;
constexpr uint32_t NEWS_REFRESH_MS         = 10UL * 60UL * 1000UL;
constexpr uint32_t MARKET_REFRESH_MS       = 10UL * 60UL * 1000UL;
constexpr uint32_t NEWS_AUTO_CYCLE_MS      = 12000UL;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000UL;

// Dark modern RGB565 color palette
constexpr uint16_t COL_BG        = 0x0842; // #0C0E13 deep slate
constexpr uint16_t COL_SURFACE   = 0x10A3; // #14171D elevated panel
constexpr uint16_t COL_CARD_TOP  = 0x18E4; // #1C2029 flip card upper
constexpr uint16_t COL_CARD_BOT  = 0x18C4; // #181B24 flip card lower
constexpr uint16_t COL_BORDER    = 0x2967; // #2D333F subtle border
constexpr uint16_t COL_HINGE     = 0x0000; // #000000 hinge slot
constexpr uint16_t COL_ACCENT    = 0x3C9F; // #3893FF vivid sky blue
constexpr uint16_t COL_ACCENT_2  = 0xFD20; // #FFA400 warm amber
constexpr uint16_t COL_TEXT      = 0xF7BE; // #F5F7FA crisp white
constexpr uint16_t COL_MUTED     = 0x8C71; // #8E95A2 muted grey
constexpr uint16_t COL_OK        = 0x2E8C; // #2DD264 emerald green
constexpr uint16_t COL_WARN      = 0xFD04; // #FFA020 orange
constexpr uint16_t COL_ERR       = 0xF9E7; // #FA3E3E coral red
constexpr uint16_t COL_GOLD      = 0xF629; // #F2C448 gold
constexpr uint16_t COL_CYAN      = 0x273F; // #24E6FF neon cyan
constexpr uint16_t COL_PURPLE    = 0xB27E; // #B24CF5 purple
constexpr uint16_t COL_NIGHT_RED = 0x9924; // #9B2620 dim night crimson
