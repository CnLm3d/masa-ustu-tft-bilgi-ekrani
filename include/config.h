#pragma once
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
constexpr uint16_t COL_SPOTIFY   = 0x1DC9; // #1DB954 Spotify green
