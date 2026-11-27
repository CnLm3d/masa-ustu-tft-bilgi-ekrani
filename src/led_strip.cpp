#include "led_strip.h"
#include "config.h"
#include <Arduino.h>
#include <esp32-hal-rmt.h>
#include <math.h>
#include <string.h>

namespace {
constexpr uint8_t NUM_LEDS = DEFAULT_LED_COUNT;
constexpr size_t SYMBOLS_LEN = NUM_LEDS * 24;

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

    // WS2812B uses GRB byte order, MSB first, 100ns RMT tick (10 MHz):
    // 1 bit: 0.8us HIGH (8 ticks) + 0.45us LOW (4 ticks)
    // 0 bit: 0.4us HIGH (4 ticks) + 0.85us LOW (8 ticks)
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

void renderNightAmber() {
    clearPixels();
    // Direct subtle warm amber on outer L1 & L5 regardless of daytime brightness
    g_pixels[0][0] = 18; g_pixels[0][1] = 6; g_pixels[0][2] = 0;
    g_pixels[4][0] = 18; g_pixels[4][1] = 6; g_pixels[4][2] = 0;
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
             const WeatherData& wx, const MarketData& mk, bool wifiOk) {
    if ((nowMs - g_lastTickMs) < 40) return;
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

    // 3) Alarm ringing has top alert priority
    if (alarmRinging) {
        const bool phase = ((nowMs / 200) & 1U) != 0;
        for (uint8_t i = 0; i < NUM_LEDS; ++i) {
            const bool white = (((i & 1U) != 0) == phase);
            if (white) setPixel(i, 255, 255, 255, max<uint8_t>(bri, 55));
            else setPixel(i, 255, 16, 16, max<uint8_t>(bri, 55));
        }
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
        renderNightAmber();
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

    // 5c) Wi-Fi Disconnected warning pulse
    if (!wifiOk) {
        clearPixels();
        const uint8_t pos = static_cast<uint8_t>((nowMs / 180) % (NUM_LEDS * 2 - 2));
        const uint8_t idx = (pos < NUM_LEDS) ? pos : static_cast<uint8_t>(NUM_LEDS * 2 - 2 - pos);
        setPixel(idx, 255, 170, 0, bri);
        flushPixels();
        return;
    }

    // 5d) Market page sentiment
    if (page == PageId::MARKET && mk.valid) {
        const float avg = (mk.usdChg + mk.eurChg + mk.gramChg) / 3.0f;
        if (avg > 0.08f) fillAll(28, 215, 95, max<uint8_t>(12, bri / 2));
        else if (avg < -0.08f) fillAll(240, 45, 45, max<uint8_t>(12, bri / 2));
        else fillAll(240, 195, 65, max<uint8_t>(12, bri / 2));
        flushPixels();
        return;
    }

    // 5e) General Weather / Ambient desk accent
    const uint8_t amb = max<uint8_t>(10, bri / 2);
    if (wx.valid) {
        if (wx.wmoCode <= 1) {
            fillAll(255, 190, 40, amb); // Clear / Sunny
        } else if (wx.wmoCode <= 48) {
            fillAll(120, 190, 255, amb); // Cloudy / Fog
        } else if (wx.wmoCode >= 71 && wx.wmoCode <= 86 && wx.wmoCode != 80 && wx.wmoCode != 81 && wx.wmoCode != 82) {
            fillAll(220, 235, 255, amb); // Snow
        } else if (wx.wmoCode >= 95) {
            fillAll(175, 70, 255, amb); // Storm
        } else {
            // Gentle rain wave across 5 LEDs
            for (uint8_t i = 0; i < NUM_LEDS; ++i) {
                const float w = 0.35f + 0.65f * (0.5f + 0.5f * sinf(nowMs * 0.004f - i * 0.9f));
                setPixel(i, 30, 120, 255, max<uint8_t>(6, static_cast<uint8_t>(amb * w)));
            }
        }
    } else {
        fillAll(56, 147, 255, amb);
    }
    flushPixels();
}
