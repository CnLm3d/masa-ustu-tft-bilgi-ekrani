#include "led_strip.h"
#include "config.h"
#include <Arduino.h>
#include <esp32-hal-rmt.h>
#include <math.h>
#include <string.h>

namespace {
constexpr uint8_t NUM_LEDS = DEFAULT_LED_COUNT;
constexpr size_t SYMBOLS_LEN = NUM_LEDS * 24;
constexpr float TWO_PI_F = 6.2831853f;

bool g_rmtReady = false;
uint8_t g_pixels[NUM_LEDS][3]{}; // R, G, B
uint8_t g_lastPixels[NUM_LEDS][3]{};
bool g_hasSentOnce = false;
rmt_data_t g_rmtSymbols[SYMBOLS_LEN]{};

uint32_t g_lastTickMs = 0;
uint32_t g_testUntilMs = 0;

void clearPixels() {
    memset(g_pixels, 0, sizeof(g_pixels));
}

void setPixel(uint8_t idx, uint8_t r, uint8_t g, uint8_t b, uint8_t briPct) {
    if (idx >= NUM_LEDS) return;
    const uint16_t scale = constrain(briPct, 0, 80);
    g_pixels[idx][0] = static_cast<uint8_t>((static_cast<uint16_t>(r) * scale) / 100U);
    g_pixels[idx][1] = static_cast<uint8_t>((static_cast<uint16_t>(g) * scale) / 100U);
    g_pixels[idx][2] = static_cast<uint8_t>((static_cast<uint16_t>(b) * scale) / 100U);
}

void fillAll(uint8_t r, uint8_t g, uint8_t b, uint8_t briPct) {
    for (uint8_t i = 0; i < NUM_LEDS; ++i) {
        setPixel(i, r, g, b, briPct);
    }
}

void hsvToRgb(uint8_t h, uint8_t& r, uint8_t& g, uint8_t& b) {
    const uint8_t region = h / 43;
    const uint8_t rem = (h - region * 43) * 6;
    const uint8_t q = 255 - rem;
    const uint8_t t = rem;
    switch (region) {
        case 0:  r = 255; g = t;   b = 0;   break;
        case 1:  r = q;   g = 255; b = 0;   break;
        case 2:  r = 0;   g = 255; b = t;   break;
        case 3:  r = 0;   g = q;   b = 255; break;
        case 4:  r = t;   g = 0;   b = 255; break;
        default: r = 255; g = 0;   b = q;   break;
    }
}

void flushPixels(bool force = false) {
    if (!g_rmtReady) return;
    if (!force && g_hasSentOnce && memcmp(g_pixels, g_lastPixels, sizeof(g_pixels)) == 0) {
        return;
    }
    memcpy(g_lastPixels, g_pixels, sizeof(g_pixels));
    g_hasSentOnce = true;

    // WS2812B uses GRB byte order, MSB first, 100ns RMT tick (10 MHz)
    size_t s = 0;
    for (uint8_t i = 0; i < NUM_LEDS; ++i) {
        const uint8_t grb[3] = {g_pixels[i][1], g_pixels[i][0], g_pixels[i][2]};
        for (uint8_t c = 0; c < 3; ++c) {
            const uint8_t byteVal = grb[c];
            for (int8_t bit = 7; bit >= 0; --bit) {
                if ((byteVal >> bit) & 0x01U) {
                    g_rmtSymbols[s].duration0 = 8;
                    g_rmtSymbols[s].level0 = 1;
                    g_rmtSymbols[s].duration1 = 4;
                    g_rmtSymbols[s].level1 = 0;
                } else {
                    g_rmtSymbols[s].duration0 = 4;
                    g_rmtSymbols[s].level0 = 1;
                    g_rmtSymbols[s].duration1 = 8;
                    g_rmtSymbols[s].level1 = 0;
                }
                ++s;
            }
        }
    }
    rmtWrite(DEFAULT_LED_PIN, g_rmtSymbols, SYMBOLS_LEN, 40);
}

void renderRadioVu(const RadioState& radio, uint8_t bri) {
    const uint8_t peak = max(radio.vuLeft, radio.vuRight);
    const uint8_t lit = static_cast<uint8_t>(min<int>(NUM_LEDS, (peak * NUM_LEDS + 45) / 100));
    clearPixels();
    for (uint8_t i = 0; i < lit; ++i) {
        if (i < 2) setPixel(i, 30, 235, 90, bri);
        else if (i < 4) setPixel(i, 255, 160, 0, bri);
        else setPixel(i, 255, 30, 30, bri);
    }
}

void renderNightAmberBreath(uint32_t nowMs) {
    // Calm simultaneous breathing warm amber on all 5 LEDs (4s period)
    const float wave = 0.5f * (1.0f - cosf((static_cast<float>(nowMs % 4000UL) / 4000.0f) * TWO_PI_F));
    const uint8_t r = static_cast<uint8_t>(10 + wave * 58.0f);
    const uint8_t g = static_cast<uint8_t>(3 + wave * 20.0f);
    for (uint8_t i = 0; i < NUM_LEDS; ++i) {
        g_pixels[i][0] = r;
        g_pixels[i][1] = g;
        g_pixels[i][2] = 0;
    }
}

void renderAlarmLightning(uint32_t nowMs, uint8_t bri) {
    const uint32_t t = nowMs % 1100UL;
    const uint8_t alertBri = max<uint8_t>(bri, 65);
    clearPixels();

    if (t < 65) {
        setPixel(0, 255, 255, 255, alertBri);
        setPixel(1, 255, 255, 255, alertBri);
        setPixel(2, 255, 255, 255, alertBri);
        setPixel(3, 140, 200, 255, alertBri);
        setPixel(4, 140, 200, 255, alertBri);
    } else if (t < 125) {
        fillAll(18, 8, 45, alertBri);
    } else if (t < 205) {
        fillAll(255, 255, 255, alertBri);
    } else if (t < 350) {
        const uint8_t phase = static_cast<uint8_t>((t / 35UL) & 1U);
        for (uint8_t i = 0; i < NUM_LEDS; ++i) {
            if (((i + phase) & 1U) == 0) setPixel(i, 210, 235, 255, alertBri);
            else setPixel(i, 255, 35, 35, alertBri);
        }
    } else if (t >= 420 && t < 485) {
        setPixel(2, 190, 230, 255, alertBri);
        setPixel(3, 190, 230, 255, alertBri);
        setPixel(4, 190, 230, 255, alertBri);
    } else if (t >= 485) {
        const float decay = 1.0f - static_cast<float>(t - 485UL) / 615.0f;
        const uint8_t rBri = static_cast<uint8_t>(alertBri * decay);
        fillAll(70, 10, 40, rBri);
    }
}

void renderWeatherEffect(uint32_t nowMs, const WeatherData& wx, uint8_t bri) {
    const uint8_t baseBri = max<uint8_t>(12, bri);
    if (!wx.valid) {
        const float w = 0.25f + 0.75f * 0.5f * (1.0f - cosf((static_cast<float>(nowMs % 2000UL) / 2000.0f) * TWO_PI_F));
        fillAll(45, 175, 255, max<uint8_t>(6, static_cast<uint8_t>(baseBri * w)));
        return;
    }

    if (wx.wmoCode <= 1) {
        if (wx.isDay) {
            // Clear sunny day: warm golden sun shimmer radiating from center LED 2
            for (uint8_t i = 0; i < NUM_LEDS; ++i) {
                const float dist = fabsf(static_cast<float>(i) - 2.0f);
                const float shimmer = 0.45f + 0.55f * (0.5f + 0.5f * cosf(nowMs * 0.0045f - dist * 0.9f));
                setPixel(i, 255, 185, 22, max<uint8_t>(8, static_cast<uint8_t>(baseBri * shimmer)));
            }
        } else {
            // Clear night: calm moonlight silver-blue slow breath
            const float w = 0.25f + 0.75f * 0.5f * (1.0f - cosf((static_cast<float>(nowMs % 3200UL) / 3200.0f) * TWO_PI_F));
            fillAll(100, 145, 255, max<uint8_t>(6, static_cast<uint8_t>(baseBri * w)));
        }
        return;
    }

    if (wx.wmoCode <= 48) {
        // Cloudy / Fog: drifting ice-blue cloud wave
        for (uint8_t i = 0; i < NUM_LEDS; ++i) {
            const float wave = 0.30f + 0.70f * (0.5f + 0.5f * sinf(nowMs * 0.0025f - i * 0.75f));
            setPixel(i, 130, 195, 255, max<uint8_t>(6, static_cast<uint8_t>(baseBri * wave)));
        }
        return;
    }

    if ((wx.wmoCode >= 71 && wx.wmoCode <= 77) || wx.wmoCode == 85 || wx.wmoCode == 86) {
        // Snow: icy-white base + pure-white twinkling sparkles every ~180ms
        const uint32_t step = nowMs / 180UL;
        const uint8_t sparkA = static_cast<uint8_t>((step * 3UL + 1UL) % NUM_LEDS);
        const uint8_t sparkB = static_cast<uint8_t>((step * 7UL + 3UL) % NUM_LEDS);
        for (uint8_t i = 0; i < NUM_LEDS; ++i) {
            if (i == sparkA || i == sparkB) setPixel(i, 255, 255, 255, baseBri);
            else setPixel(i, 140, 175, 220, max<uint8_t>(6, baseBri / 3));
        }
        return;
    }

    if (wx.wmoCode >= 95) {
        // Thunderstorm: deep purple-indigo cloud + periodic lightning flashes every 1.8s
        const uint32_t cyc = nowMs % 1800UL;
        if (cyc < 60 || (cyc >= 120 && cyc < 185)) {
            fillAll(240, 245, 255, max<uint8_t>(baseBri, 60));
        } else {
            fillAll(55, 18, 110, max<uint8_t>(8, baseBri / 2));
        }
        return;
    }

    // Rain / Showers: deep rain-blue base + falling cyan raindrop sparkles every ~150ms
    const uint32_t rStep = nowMs / 150UL;
    const uint8_t dropA = static_cast<uint8_t>(rStep % NUM_LEDS);
    const uint8_t dropB = static_cast<uint8_t>((rStep + 2UL) % NUM_LEDS);
    for (uint8_t i = 0; i < NUM_LEDS; ++i) {
        if (i == dropA || i == dropB) setPixel(i, 60, 210, 255, baseBri);
        else setPixel(i, 12, 42, 145, max<uint8_t>(6, baseBri / 3));
    }
}

void renderMarketEffect(uint32_t nowMs, const MarketData& mk, uint8_t focusRow, uint8_t bri) {
    const uint8_t active = focusRow % 5;
    const float chgs[5] = {mk.usdChg, mk.eurChg, mk.gbpChg, mk.gramChg, mk.onsChg};
    const float chg = chgs[active];
    const uint8_t mBri = max<uint8_t>(12, bri);

    if (chg > 0.005f) {
        // Rising trend: green comet sweeping LED 0 -> LED 4
        const float pos = fmodf(static_cast<float>(nowMs) * 0.0055f, static_cast<float>(NUM_LEDS));
        for (uint8_t i = 0; i < NUM_LEDS; ++i) {
            float d = fabsf(static_cast<float>(i) - pos);
            if (d > 2.5f) d = static_cast<float>(NUM_LEDS) - d;
            const float w = max(0.18f, 1.0f - d * 0.45f);
            setPixel(i, 20, 240, 75, max<uint8_t>(5, static_cast<uint8_t>(mBri * w)));
        }
    } else if (chg < -0.005f) {
        // Falling trend: red comet sweeping LED 4 -> LED 0
        const float pos = static_cast<float>(NUM_LEDS - 1) - fmodf(static_cast<float>(nowMs) * 0.0055f, static_cast<float>(NUM_LEDS));
        for (uint8_t i = 0; i < NUM_LEDS; ++i) {
            float d = fabsf(static_cast<float>(i) - pos);
            if (d > 2.5f) d = static_cast<float>(NUM_LEDS) - d;
            const float w = max(0.18f, 1.0f - d * 0.45f);
            setPixel(i, 255, 25, 25, max<uint8_t>(5, static_cast<uint8_t>(mBri * w)));
        }
    } else {
        // Neutral / TCMB static: soft gold strip with breathing active row index LED
        const float pulse = 0.35f + 0.65f * 0.5f * (1.0f - cosf((static_cast<float>(nowMs % 1000UL) / 1000.0f) * TWO_PI_F));
        for (uint8_t i = 0; i < NUM_LEDS; ++i) {
            if (i == active) setPixel(i, 255, 195, 30, max<uint8_t>(8, static_cast<uint8_t>(mBri * pulse)));
            else setPixel(i, 180, 130, 15, max<uint8_t>(5, mBri / 5));
        }
    }
}

void renderClockBreath(uint32_t nowMs, uint32_t ledColor, uint8_t bri) {
    // Exact 1-second smooth breathing wave across all 5 LEDs
    const float phase = static_cast<float>(nowMs % 1000UL) / 1000.0f;
    const float breath = 0.14f + 0.86f * 0.5f * (1.0f - cosf(phase * TWO_PI_F));
    const uint8_t r = static_cast<uint8_t>((ledColor >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((ledColor >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(ledColor & 0xFF);
    fillAll(r, g, b, max<uint8_t>(4, static_cast<uint8_t>(bri * breath)));
}
} // namespace

void ledInit(const DeviceConfig& cfg) {
    (void)cfg;
    g_rmtReady = rmtInit(DEFAULT_LED_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000UL);
    if (g_rmtReady) {
        rmtSetEOT(DEFAULT_LED_PIN, 0);
        clearPixels();
        flushPixels(true);
    }
}

void ledUpdateConfig(const DeviceConfig& cfg) {
    (void)cfg;
    g_hasSentOnce = false;
}

void ledTriggerTest() {
    g_testUntilMs = millis() + 2600UL;
}

void ledShowOtaProgress(uint8_t pct) {
    clearPixels();
    const uint8_t lit = static_cast<uint8_t>(min<int>(NUM_LEDS, (static_cast<int>(pct) * NUM_LEDS + 19) / 100));
    for (uint8_t i = 0; i < lit; ++i) {
        setPixel(i, 40, 140, 255, 45);
    }
    flushPixels(true);
}

void ledTick(uint32_t nowMs, const DeviceConfig& cfg, PageId page, bool alarmRinging,
             const PomodoroState& pomo, const RadioState& radio,
             const WeatherData& wx, const MarketData& mk, uint8_t marketFocusRow, bool wifiOk) {
    if ((nowMs - g_lastTickMs) < 25) return;
    g_lastTickMs = nowMs;

    // 1) Interactive rainbow test (2.6s)
    if (nowMs < g_testUntilMs) {
        const uint8_t baseHue = static_cast<uint8_t>((nowMs / 10) & 0xFF);
        for (uint8_t i = 0; i < NUM_LEDS; ++i) {
            uint8_t r, g, b;
            hsvToRgb(static_cast<uint8_t>(baseHue + i * 42), r, g, b);
            setPixel(i, r, g, b, max<uint8_t>(cfg.ledBrightness, 35));
        }
        flushPixels();
        return;
    }

    const uint8_t bri = constrain(cfg.ledBrightness, 10, 80);

    // 2) Disabled or explicit OFF
    if (!cfg.ledEnabled || cfg.ledMode == static_cast<uint8_t>(LedMode::OFF)) {
        clearPixels();
        flushPixels();
        return;
    }

    // 3) Alarm ringing -> Lightning bolt flash
    if (alarmRinging) {
        renderAlarmLightning(nowMs, bri);
        flushPixels();
        return;
    }

    // 4) Manual fixed modes
    const LedMode mode = static_cast<LedMode>(cfg.ledMode);
    if (mode == LedMode::SOLID) {
        const uint8_t r = static_cast<uint8_t>((cfg.ledColor >> 16) & 0xFF);
        const uint8_t g = static_cast<uint8_t>((cfg.ledColor >> 8) & 0xFF);
        const uint8_t b = static_cast<uint8_t>(cfg.ledColor & 0xFF);
        fillAll(r, g, b, bri);
        flushPixels();
        return;
    }
    if (mode == LedMode::NIGHT || page == PageId::NIGHT) {
        renderNightAmberBreath(nowMs);
        flushPixels();
        return;
    }
    if (mode == LedMode::RADIO_VU) {
        if (radio.isPlaying) renderRadioVu(radio, bri);
        else fillAll(20, 140, 180, max<uint8_t>(10, bri / 2));
        flushPixels();
        return;
    }

    // 5) AUTO smart state machine
    // 5a) Pomodoro Countdown / Focus active
    if (pomo.running && pomo.totalSec > 0) {
        clearPixels();
        if (pomo.onBreak) {
            const float wave = 0.45f + 0.55f * (0.5f + 0.5f * sinf(nowMs * 0.004f));
            const uint8_t bBri = static_cast<uint8_t>(bri * wave);
            fillAll(32, 180, 255, max<uint8_t>(8, bBri));
        } else {
            const uint32_t pct = (static_cast<uint32_t>(pomo.remainingSec) * 100UL) / pomo.totalSec;
            const uint8_t lit = static_cast<uint8_t>(max<uint32_t>(1, (pct * NUM_LEDS + 99) / 100));
            const bool critical = (pomo.remainingSec <= 10) && (((nowMs / 250) & 1U) == 0);
            for (uint8_t i = 0; i < min<uint8_t>(lit, NUM_LEDS); ++i) {
                if (pct > 50) setPixel(i, 35, 220, 95, bri);
                else if (pct > 20) setPixel(i, 255, 165, 0, bri);
                else if (!critical) setPixel(i, 255, 30, 30, bri);
            }
        }
        flushPixels();
        return;
    }

    // 5b) Internet Radio VU Meter
    if (radio.isPlaying) {
        renderRadioVu(radio, bri);
        flushPixels();
        return;
    }

    // 5c) Wi-Fi Disconnected warning sweep
    if (!wifiOk) {
        clearPixels();
        const uint8_t pos = static_cast<uint8_t>((nowMs / 180) % (NUM_LEDS * 2 - 2));
        const uint8_t idx = (pos < NUM_LEDS) ? pos : static_cast<uint8_t>(NUM_LEDS * 2 - 2 - pos);
        setPixel(idx, 255, 170, 0, bri);
        flushPixels();
        return;
    }

    // 5d) Weather page -> Weather-reactive effects
    if (page == PageId::WEATHER) {
        renderWeatherEffect(nowMs, wx, bri);
        flushPixels();
        return;
    }

    // 5e) Market page -> Row-by-row rise/fall effect
    if (page == PageId::MARKET && mk.valid) {
        renderMarketEffect(nowMs, mk, marketFocusRow, bri);
        flushPixels();
        return;
    }

    // 5f) Clock screen & other idle screens -> 1-second breathing wave
    renderClockBreath(nowMs, cfg.ledColor, bri);
    flushPixels();
}
