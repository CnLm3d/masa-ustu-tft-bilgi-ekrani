#include "ui_clock.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold88Digits.h"
#include "net_services.h"
#include "ui_clock_themes.h"
#include "ui_common.h"
#include <string.h>
#include <time.h>

namespace {
constexpr int16_t CARD_W = SX(134);
constexpr int16_t CARD_H = SY(134);
constexpr int16_t HALF_H = CARD_H / 2;
constexpr int16_t HOUR_X = SX(16);
constexpr int16_t MIN_X  = SX(170);
constexpr int16_t CARD_Y = SY(33);

int g_lastHour = -1;
int g_lastMin  = -1;
int g_lastSec  = -1;
int g_lastDay  = -1;
uint32_t g_lastWxMs = 0;
bool g_lastFetching = false;
bool g_lastWifiOk   = false;
bool g_forceFull    = true;

uint16_t scaleRgb565(uint16_t c, uint16_t scale256) {
    uint32_t r = ((c >> 11) & 0x1F) * scale256 >> 8;
    uint32_t g = ((c >> 5) & 0x3F) * scale256 >> 8;
    uint32_t b = (c & 0x1F) * scale256 >> 8;
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

template <typename Target>
void renderFlipCardToTarget(Target& dst, int16_t ox, int16_t oy, const char* twoDigits) {
    dst.fillRoundRect(ox, oy, CARD_W, CARD_H, 10, COL_CARD_BOT);
    dst.fillRoundRect(ox, oy, CARD_W, HALF_H + 6, 10, COL_CARD_TOP);
    dst.fillRect(ox, oy + HALF_H, CARD_W, 8, COL_CARD_BOT);
    dst.drawRoundRect(ox, oy, CARD_W, CARD_H, 10, COL_BORDER);

    dst.loadFont(SegoeUIBold88Digits);
    dst.setTextDatum(MC_DATUM);
    dst.setTextColor(COL_TEXT);
    dst.drawString(twoDigits, ox + CARD_W / 2, oy + CARD_H / 2 + 2);
    dst.unloadFont();

    dst.fillRect(ox + 1, oy + HALF_H - 1, CARD_W - 2, 3, COL_HINGE);
    dst.fillRoundRect(ox + 3, oy + HALF_H - 5, 5, 10, 2, COL_MUTED);
    dst.fillRoundRect(ox + CARD_W - 8, oy + HALF_H - 5, 5, 10, 2, COL_MUTED);
}

void drawStaticCard(TFT_eSPI& tft, int16_t x, int16_t y, const char* twoDigits) {
    TFT_eSprite spr(&tft);
    spr.setColorDepth(16);
    if (!spr.createSprite(CARD_W, CARD_H)) {
        renderFlipCardToTarget(tft, x, y, twoDigits);
        return;
    }
    spr.fillSprite(COL_BG);
    renderFlipCardToTarget(spr, 0, 0, twoDigits);
    spr.pushSprite(x, y);
    spr.deleteSprite();
}

void animateFlipCard(TFT_eSPI& tft, int16_t x, int16_t y, const char* oldStr, const char* newStr) {
    TFT_eSprite newCard(&tft);
    newCard.setColorDepth(16);
    if (!newCard.createSprite(CARD_W, CARD_H)) {
        drawStaticCard(tft, x, y, newStr);
        return;
    }

    TFT_eSprite oldTop(&tft);
    oldTop.setColorDepth(16);
    if (!oldTop.createSprite(CARD_W, HALF_H)) {
        newCard.fillSprite(COL_BG);
        renderFlipCardToTarget(newCard, 0, 0, newStr);
        newCard.pushSprite(x, y);
        newCard.deleteSprite();
        return;
    }

    newCard.fillSprite(COL_BG);
    renderFlipCardToTarget(newCard, 0, 0, oldStr);
    memcpy(oldTop.getPointer(), newCard.getPointer(), static_cast<size_t>(CARD_W) * HALF_H * 2);
    newCard.fillSprite(COL_BG);
    renderFlipCardToTarget(newCard, 0, 0, newStr);

    uint16_t rowBuf[CARD_W];
    const bool prevSwap = tft.getSwapBytes();
    tft.setSwapBytes(true);

    constexpr int STEPS = 9;

    for (int s = 1; s <= STEPS; ++s) {
        const int16_t curH = (HALF_H * (STEPS - s)) / STEPS;
        const int16_t uncoveredH = HALF_H - curH;
        const uint16_t scale = static_cast<uint16_t>(115 + (140 * curH) / HALF_H);

        tft.startWrite();
        for (int16_t ry = 0; ry < uncoveredH; ++ry) {
            for (int16_t rx = 0; rx < CARD_W; ++rx) {
                rowBuf[rx] = newCard.readPixel(rx, ry);
            }
            tft.pushImage(x, y + ry, CARD_W, 1, rowBuf);
        }
        for (int16_t dy = 0; dy < curH; ++dy) {
            const int16_t srcY = (dy * HALF_H) / curH;
            for (int16_t rx = 0; rx < CARD_W; ++rx) {
                rowBuf[rx] = scaleRgb565(oldTop.readPixel(rx, srcY), scale);
            }
            tft.pushImage(x, y + uncoveredH + dy, CARD_W, 1, rowBuf);
        }
        if (curH > 2) {
            tft.drawFastHLine(x + 2, y + uncoveredH, CARD_W - 4, COL_HINGE);
        }
        tft.endWrite();
        delay(16);
    }

    for (int s = 1; s <= STEPS; ++s) {
        const int16_t curH = (HALF_H * s) / STEPS;
        const uint16_t scale = static_cast<uint16_t>(115 + (140 * curH) / HALF_H);

        tft.startWrite();
        for (int16_t dy = 0; dy < curH; ++dy) {
            const int16_t srcY = HALF_H + (dy * HALF_H) / curH;
            for (int16_t rx = 0; rx < CARD_W; ++rx) {
                rowBuf[rx] = scaleRgb565(newCard.readPixel(rx, srcY), scale);
            }
            tft.pushImage(x, y + HALF_H + dy, CARD_W, 1, rowBuf);
        }
        tft.fillRect(x + 1, y + HALF_H - 1, CARD_W - 2, 3, COL_HINGE);
        tft.endWrite();
        delay(16);
    }

    tft.setSwapBytes(prevSwap);
    newCard.pushSprite(x, y);

    oldTop.deleteSprite();
    newCard.deleteSprite();
}

void drawColon(TFT_eSPI& tft, bool on) {
    const uint16_t c = on ? COL_ACCENT : COL_BORDER;
    tft.fillCircle(SX(160), SY(86), SR(4), c);
    tft.fillCircle(SX(160), SY(114), SR(4), c);
}

void drawDateAndWeatherPill(TFT_eSPI& tft, const struct tm& ti, bool timeValid,
                            const DeviceConfig& cfg, const WeatherData& wx) {
    tft.fillRoundRect(SX(16), SY(172), SCREEN_W - SX(32), SY(24), 7, COL_SURFACE);
    tft.drawRoundRect(SX(16), SY(172), SCREEN_W - SX(32), SY(24), 7, COL_BORDER);

    char dateBuf[40];
    if (timeValid) {
        snprintf(dateBuf, sizeof(dateBuf), "%d %s %s",
                 ti.tm_mday, TR_MONTHS_SHORT[ti.tm_mon % 12], TR_DAYS_LONG[ti.tm_wday % 7]);
    } else {
        strlcpy(dateBuf, "Saat senkronize ediliyor...", sizeof(dateBuf));
    }

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(dateBuf, SX(26), SY(184));

    if (wx.valid) {
        char wxBuf[48];
        snprintf(wxBuf, sizeof(wxBuf), "%s %.0f°C", cfg.city, wx.tempC);
        tft.setTextDatum(MR_DATUM);
        tft.setTextColor(COL_ACCENT_2, COL_SURFACE);
        tft.drawString(wxBuf, SCREEN_W - SX(26), SY(184));
    }
    tft.unloadFont();
}

void drawSecondBar(TFT_eSPI& tft, int sec) {
    const int16_t barX = SX(16);
    const int16_t barY = SY(200);
    const int16_t barW = SCREEN_W - SX(32);
    const int16_t barH = SY(4);
    tft.fillRoundRect(barX, barY, barW, barH, 2, COL_SURFACE);
    const int16_t fillW = (barW * (sec + 1)) / 60;
    if (fillW > 2) {
        tft.fillRoundRect(barX, barY, fillW, barH, 2, COL_ACCENT);
    }
}
} // namespace

void uiClockEnter(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx) {
    g_forceFull = true;
    uiClockTick(tft, cfg, wx, netIsFetching());
}

void uiClockTick(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx, bool fetching) {
    const ClockTheme theme = static_cast<ClockTheme>(
        cfg.clockTheme < static_cast<uint8_t>(ClockTheme::COUNT) ? cfg.clockTheme : 0);

    if (theme != ClockTheme::FLIP) {
        const bool full = g_forceFull;
        g_forceFull = false;
        uiThemesTick(tft, theme, cfg, wx, fetching, full);
        return;
    }

    struct tm ti{};
    const bool timeValid = getLocalTime(&ti, 5);
    const int hh = timeValid ? ti.tm_hour : 0;
    const int mm = timeValid ? ti.tm_min : 0;
    const int ss = timeValid ? ti.tm_sec : static_cast<int>((millis() / 1000) % 60);
    const int dayKey = timeValid ? (ti.tm_yday * 100 + ti.tm_wday) : -2;
    const bool wifiOk = netIsWifiConnected();

    char newH[4], newM[4];
    if (timeValid) {
        snprintf(newH, sizeof(newH), "%02d", hh);
        snprintf(newM, sizeof(newM), "%02d", mm);
    } else {
        strlcpy(newH, "--", sizeof(newH));
        strlcpy(newM, "--", sizeof(newM));
    }

    if (g_forceFull) {
        g_forceFull = false;
        tft.fillScreen(COL_BG);
        drawTopBar(tft, "Yaprak Saat", "Tema", netGetRssi(), wifiOk, fetching);
        drawStaticCard(tft, HOUR_X, CARD_Y, newH);
        drawStaticCard(tft, MIN_X, CARD_Y, newM);
        drawColon(tft, (ss % 2) == 0);
        drawDateAndWeatherPill(tft, ti, timeValid, cfg, wx);
        drawSecondBar(tft, ss);
        drawBottomNav(tft, 0);

        g_lastHour = hh;
        g_lastMin  = mm;
        g_lastSec  = ss;
        g_lastDay  = dayKey;
        g_lastWxMs = wx.updatedAtMs;
        g_lastFetching = fetching;
        g_lastWifiOk   = wifiOk;
        return;
    }

    if (fetching != g_lastFetching || wifiOk != g_lastWifiOk) {
        g_lastFetching = fetching;
        g_lastWifiOk   = wifiOk;
        drawTopBar(tft, "Yaprak Saat", "Tema", netGetRssi(), wifiOk, fetching);
    }

    if (timeValid && hh != g_lastHour && g_lastHour >= 0) {
        char oldH[4];
        snprintf(oldH, sizeof(oldH), "%02d", g_lastHour);
        animateFlipCard(tft, HOUR_X, CARD_Y, oldH, newH);
        g_lastHour = hh;
    } else if (timeValid && hh != g_lastHour) {
        drawStaticCard(tft, HOUR_X, CARD_Y, newH);
        g_lastHour = hh;
    }

    if (timeValid && mm != g_lastMin && g_lastMin >= 0) {
        char oldM[4];
        snprintf(oldM, sizeof(oldM), "%02d", g_lastMin);
        animateFlipCard(tft, MIN_X, CARD_Y, oldM, newM);
        g_lastMin = mm;
    } else if (timeValid && mm != g_lastMin) {
        drawStaticCard(tft, MIN_X, CARD_Y, newM);
        g_lastMin = mm;
    }

    if (ss != g_lastSec) {
        g_lastSec = ss;
        drawColon(tft, (ss % 2) == 0);
        drawSecondBar(tft, ss);
    }

    if (dayKey != g_lastDay || wx.updatedAtMs != g_lastWxMs) {
        g_lastDay  = dayKey;
        g_lastWxMs = wx.updatedAtMs;
        drawDateAndWeatherPill(tft, ti, timeValid, cfg, wx);
    }
}

void uiClockTriggerDemo(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx) {
    const ClockTheme theme = static_cast<ClockTheme>(
        cfg.clockTheme < static_cast<uint8_t>(ClockTheme::COUNT) ? cfg.clockTheme : 0);

    if (theme != ClockTheme::FLIP) {
        uiThemesDemo(tft, theme, cfg, wx);
        return;
    }

    struct tm ti{};
    if (!getLocalTime(&ti, 5)) {
        animateFlipCard(tft, MIN_X, CARD_Y, "59", "00");
        return;
    }
    char curM[4], prevM[4];
    snprintf(curM, sizeof(curM), "%02d", ti.tm_min);
    snprintf(prevM, sizeof(prevM), "%02d", (ti.tm_min + 59) % 60);
    animateFlipCard(tft, MIN_X, CARD_Y, prevM, curM);
}
