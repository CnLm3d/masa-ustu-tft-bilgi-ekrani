#pragma once
#include <stdint.h>

// Screen dimensions (landscape rotation=1 on 240x320 ILI9341)
constexpr int16_t SCREEN_W = 320;
constexpr int16_t SCREEN_H = 240;

// Rotary Encoder (KY-040 / EC11) pins
constexpr uint8_t PIN_ENC_CLK = 32;
constexpr uint8_t PIN_ENC_DT  = 33;
constexpr uint8_t PIN_ENC_SW  = 25;

// Default Wi-Fi & Location (overwritten via NVS / Web Setup Portal)
constexpr char DEFAULT_WIFI_SSID[] = "WIFI_ADINIZ";
constexpr char DEFAULT_WIFI_PASS[] = "WIFI_SIFRENIZ";
constexpr char DEFAULT_CITY_NAME[] = "İstanbul";
constexpr float DEFAULT_CITY_LAT   = 41.0082f;
constexpr float DEFAULT_CITY_LON   = 28.9784f;
constexpr char DEFAULT_TZ[]        = "TRT-3";
constexpr char NTP_SERVER_1[]      = "pool.ntp.org";
constexpr char NTP_SERVER_2[]      = "tr.pool.ntp.org";

// Refresh & UI timings
constexpr uint32_t WEATHER_REFRESH_MS  = 15UL * 60UL * 1000UL;
constexpr uint32_t NEWS_REFRESH_MS     = 10UL * 60UL * 1000UL;
constexpr uint32_t NEWS_AUTO_CYCLE_MS  = 12000UL;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000UL;

// Dark modern RGB565 color palette
constexpr uint16_t COL_BG       = 0x0842; // #0C0E13 deep slate
constexpr uint16_t COL_SURFACE  = 0x10A3; // #14171D elevated panel
constexpr uint16_t COL_CARD_TOP = 0x18E4; // #1C2029 flip card upper
constexpr uint16_t COL_CARD_BOT = 0x18C4; // #181B24 flip card lower
constexpr uint16_t COL_BORDER   = 0x2967; // #2D333F subtle border
constexpr uint16_t COL_HINGE    = 0x0000; // #000000 hinge slot
constexpr uint16_t COL_ACCENT   = 0x3C9F; // #3893FF vivid sky blue
constexpr uint16_t COL_ACCENT_2 = 0xFD20; // #FFA400 warm amber
constexpr uint16_t COL_TEXT     = 0xF7BE; // #F5F7FA crisp white
constexpr uint16_t COL_MUTED    = 0x8C71; // #8E95A2 muted grey
constexpr uint16_t COL_OK       = 0x2E8C; // #2DD264 emerald green
constexpr uint16_t COL_WARN     = 0xFD04; // #FFA020 orange
constexpr uint16_t COL_ERR      = 0xF9E7; // #FA3E3E coral red
