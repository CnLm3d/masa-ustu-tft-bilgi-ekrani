#include "ui_clock_themes.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "net_services.h"
#include "ui_common.h"
#include <math.h>
#include <time.h>

namespace {
int g_prevDigits[4] = {-1, -1, -1, -1};
int g_prevSec = -1;
int g_prevDay = -1;
uint32_t g_prevWxMs = 0;
bool g_prevFetching = false;
bool g_prevWifi = false;

void getClockVals(struct tm& ti, bool& valid, int d[4], int& sec, int& dayKey) {
    valid = getLocalTime(&ti, 5);
    if (valid) {
        d[0] = ti.tm_hour / 10;
        d[1] = ti.tm_hour % 10;
        d[2] = ti.tm_min / 10;
        d[3] = ti.tm_min % 10;
        sec = ti.tm_sec;
        dayKey = ti.tm_yday * 100 + ti.tm_wday;
    } else {
        d[0] = 0; d[1] = 0; d[2] = 0; d[3] = 0;
        sec = static_cast<int>((millis() / 1000) % 60);
        dayKey = -2;
    }
}

void drawInfoSummaryPanel(TFT_eSPI& tft, const struct tm& ti, bool valid,
                          const DeviceConfig& cfg, const WeatherData& wx, uint16_t accentCol) {
    const int16_t px = 200;
    tft.fillRoundRect(px, 32, SCREEN_W - px - 10, 170, 10, COL_SURFACE);
    tft.drawRoundRect(px, 32, SCREEN_W - px - 10, 170, 10, COL_BORDER);

    char tbuf[8];
    if (valid) snprintf(tbuf, sizeof(tbuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
    else strlcpy(tbuf, "--:--", sizeof(tbuf));

    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(tbuf, px + 55, 56);
    tft.unloadFont();

    tft.drawFastHLine(px + 10, 76, 90, COL_BORDER);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    if (valid) {
        char dbuf[24];
        snprintf(dbuf, sizeof(dbuf), "%d %s %s",
                 ti.tm_mday, TR_MONTHS_SHORT[ti.tm_mon % 12], TR_DAYS_SHORT[ti.tm_wday % 7]);
        tft.setTextColor(COL_TEXT, COL_SURFACE);
        tft.drawString(dbuf, px + 55, 96);
    }
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(cfg.city, px + 55, 118);

    if (wx.valid) {
        char wbuf[20];
        snprintf(wbuf, sizeof(wbuf), "%.0f°C", wx.tempC);
        tft.setTextColor(accentCol, COL_SURFACE);
        tft.drawString(wbuf, px + 55, 140);
    }

    tft.drawFastHLine(px + 10, 158, 90, COL_BORDER);
    bool hasAlarm = false;
    for (int i = 0; i < 3; ++i) {
        if (cfg.alarms[i].enabled) {
            char abuf[20];
            snprintf(abuf, sizeof(abuf), "Alarm %02u:%02u", cfg.alarms[i].hour, cfg.alarms[i].minute);
            tft.setTextColor(COL_OK, COL_SURFACE);
            tft.drawString(abuf, px + 55, 178);
            hasAlarm = true;
            break;
        }
    }
    if (!hasAlarm) {
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString("Alarm Kapalı", px + 55, 178);
    }
    tft.unloadFont();
}

// =========================================================================
// 1. REALISTIC NIXIE TUBE CLOCK (IN-18)
// =========================================================================
struct Pt { uint8_t x; uint8_t y; }; // 0..100 normalized
const Pt NIXIE_PATHS[10][12] = {
    {{50,4},{80,8},{90,30},{90,70},{80,92},{50,96},{20,92},{10,70},{10,30},{20,8},{50,4},{255,255}}, // 0
    {{30,20},{54,5},{54,95},{30,95},{78,95},{255,255}}, // 1 (break handled via count)
    {{16,24},{26,7},{52,4},{78,8},{86,24},{82,42},{16,95},{88,95},{255,255}}, // 2
    {{18,14},{48,4},{78,12},{84,28},{72,44},{42,48},{76,54},{88,72},{78,90},{48,96},{16,86},{255,255}}, // 3
    {{74,95},{74,5},{14,66},{88,66},{255,255}}, // 4
    {{82,5},{24,5},{20,44},{54,42},{82,52},{86,74},{74,92},{46,96},{18,86},{255,255}}, // 5
    {{76,12},{52,4},{24,14},{12,44},{12,76},{26,93},{54,96},{82,88},{88,66},{76,48},{48,44},{18,52}}, // 6
    {{16,6},{86,6},{40,95},{255,255}}, // 7
    {{50,4},{76,9},{82,24},{72,41},{24,54},{12,72},{22,91},{50,96},{78,91},{88,72},{76,54},{24,41}}, // 8
    {{82,48},{50,54},{18,44},{14,22},{28,7},{56,4},{82,16},{88,48},{78,82},{52,96},{24,90},{255,255}} // 9
};

void drawThickLine(TFT_eSPI& tft, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   int16_t r, uint16_t col) {
    tft.drawLine(x0, y0, x1, y1, col);
    if (r <= 0) return;
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t cx = x0, cy = y0;
    while (true) {
        tft.fillCircle(cx, cy, r, col);
        if (cx == x1 && cy == y1) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 < dx)  { err += dx; cy += sy; }
    }
}

void drawNixieWire(TFT_eSPI& tft, int digit, int16_t ox, int16_t oy, int16_t w, int16_t h,
                   int16_t r, uint16_t col) {
    digit = digit % 10;
    if (digit == 1) {
        int16_t x0 = ox + 30 * w / 100, y0 = oy + 20 * h / 100;
        int16_t x1 = ox + 54 * w / 100, y1 = oy + 5 * h / 100;
        int16_t x2 = x1, y2 = oy + 95 * h / 100;
        drawThickLine(tft, x0, y0, x1, y1, r, col);
        drawThickLine(tft, x1, y1, x2, y2, r, col);
        drawThickLine(tft, ox + 30 * w / 100, y2, ox + 78 * w / 100, y2, r, col);
        return;
    }
    const Pt* pts = NIXIE_PATHS[digit];
    for (int i = 0; i < 11; ++i) {
        if (pts[i + 1].x == 255) break;
        int16_t x0 = ox + pts[i].x * w / 100;
        int16_t y0 = oy + pts[i].y * h / 100;
        int16_t x1 = ox + pts[i + 1].x * w / 100;
        int16_t y1 = oy + pts[i + 1].y * h / 100;
        drawThickLine(tft, x0, y0, x1, y1, r, col);
    }
    if (digit == 8) {
        drawThickLine(tft, ox + 24 * w / 100, oy + 41 * h / 100,
                      ox + 18 * w / 100, oy + 24 * h / 100, r, col);
        drawThickLine(tft, ox + 18 * w / 100, oy + 24 * h / 100,
                      ox + 24 * w / 100, oy + 9 * h / 100, r, col);
        drawThickLine(tft, ox + 24 * w / 100, oy + 9 * h / 100,
                      ox + 50 * w / 100, oy + 4 * h / 100, r, col);
    }
}

void drawNixieTube(TFT_eSPI& tft, int16_t x, int digit) {
    constexpr int16_t tw = 60, th = 128, ty = 30;
    // Exhaust pip & glass envelope
    tft.fillRoundRect(x + tw / 2 - 4, ty - 4, 8, 8, 3, 0x10A3);
    tft.drawRoundRect(x + tw / 2 - 4, ty - 4, 8, 8, 3, 0x5ACB);
    tft.fillRoundRect(x, ty, tw, th, 20, 0x0821);
    tft.drawRoundRect(x, ty, tw, th, 20, 0x5AEB);
    tft.fillRoundRect(x + 3, ty + 4, tw - 6, th - 16, 17, 0x1041);

    // Mica insulator rings & support rods
    tft.fillRect(x + 10, ty + 16, tw - 20, 3, 0x3944);
    tft.fillRect(x + 10, ty + th - 22, tw - 20, 3, 0x3944);
    tft.drawFastVLine(x + 12, ty + 18, th - 38, 0x4207);
    tft.drawFastVLine(x + tw - 12, ty + 18, th - 38, 0x4207);

    // Diamond anode wire mesh + unlit cathode silhouette
    for (int16_t s = -32; s < 84; s += 10) {
        int16_t x1 = max<int16_t>(x + 6, x + 6 + s);
        int16_t y1 = ty + 20 + max<int16_t>(0, -s);
        int16_t x2 = min<int16_t>(x + tw - 6, x + 6 + s + 64);
        int16_t y2 = ty + 20 + min<int16_t>(84, 64 - s);
        if (x1 < x2 && y1 < y2) tft.drawLine(x1, y1, x2, y2, 0x20C2);
    }
    drawNixieWire(tft, 8, x + 13, ty + 28, 34, 68, 0, 0x2923);

    // 3-layer glowing cathode wire
    drawNixieWire(tft, digit, x + 13, ty + 28, 34, 68, 3, 0x91C0);
    drawNixieWire(tft, digit, x + 13, ty + 28, 34, 68, 1, 0xFC42);
    drawNixieWire(tft, digit, x + 13, ty + 28, 34, 68, 0, 0xFED3);

    // Glass specular highlights
    tft.drawFastVLine(x + 6, ty + 20, 36, 0x8430);
    tft.drawFastVLine(x + tw - 6, ty + 24, th - 48, 0x4228);

    // Brass collar + bakelite base + metal pins
    tft.fillRoundRect(x + 3, ty + th - 14, tw - 6, 12, 4, 0x59E3);
    tft.drawRoundRect(x + 3, ty + th - 14, tw - 6, 12, 4, 0x9B46);
    tft.fillRect(x + 6, ty + th - 3, tw - 12, 7, 0x18C2);
    for (int16_t px = x + 12; px < x + tw - 10; px += 9) {
        tft.drawFastVLine(px, ty + th + 4, 5, 0xAD55);
    }
}

void drawNixieColon(TFT_eSPI& tft, bool on) {
    for (int16_t cy : {76, 110}) {
        tft.fillRoundRect(154, cy - 8, 12, 16, 5, 0x1041);
        tft.drawRoundRect(154, cy - 8, 12, 16, 5, 0x528A);
        if (on) {
            tft.fillCircle(160, cy, 4, 0x91C0);
            tft.fillCircle(160, cy, 2, 0xFC83);
            tft.drawPixel(160, cy, 0xFED3);
        }
    }
}

void tickNixie(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx,
               bool fetching, bool forceFull) {
    struct tm ti{}; bool valid = false; int d[4], sec = 0, dayKey = 0;
    getClockVals(ti, valid, d, sec, dayKey);
    const bool wifiOk = netIsWifiConnected();
    constexpr int16_t xs[4] = {16, 86, 174, 244};

    if (forceFull) {
        tft.fillScreen(0x0821);
        drawTopBar(tft, "Nixie Tüp Saat", "Tema", netGetRssi(), wifiOk, fetching);
        tft.fillRoundRect(10, 152, SCREEN_W - 20, 22, 5, 0x28C2);
        tft.drawRoundRect(10, 152, SCREEN_W - 20, 22, 5, 0x4983);
        for (int i = 0; i < 4; ++i) {
            drawNixieTube(tft, xs[i], d[i]);
            g_prevDigits[i] = d[i];
        }
        drawNixieColon(tft, (sec % 2) == 0);
        drawBottomNav(tft, 0);
        g_prevDay = -999;
        g_prevSec = sec;
    }

    for (int i = 0; i < 4; ++i) {
        if (d[i] != g_prevDigits[i]) {
            g_prevDigits[i] = d[i];
            drawNixieTube(tft, xs[i], d[i]);
        }
    }
    if (sec != g_prevSec) {
        g_prevSec = sec;
        drawNixieColon(tft, (sec % 2) == 0);
    }
    if (dayKey != g_prevDay || wx.updatedAtMs != g_prevWxMs) {
        g_prevDay = dayKey; g_prevWxMs = wx.updatedAtMs;
        tft.fillRoundRect(14, 179, SCREEN_W - 28, 24, 6, 0x1061);
        tft.drawRoundRect(14, 179, SCREEN_W - 28, 24, 6, 0x4163);
        tft.loadFont(SegoeUI14TR);
        if (valid) {
            char dbuf[40];
            snprintf(dbuf, sizeof(dbuf), "%d %s %s",
                     ti.tm_mday, TR_MONTHS_SHORT[ti.tm_mon % 12], TR_DAYS_LONG[ti.tm_wday % 7]);
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(0xEDE6, 0x1061);
            tft.drawString(dbuf, 24, 191);
        }
        if (wx.valid) {
            char wbuf[40];
            snprintf(wbuf, sizeof(wbuf), "%s %.0f°C", cfg.city, wx.tempC);
            tft.setTextDatum(MR_DATUM);
            tft.setTextColor(0xFC85, 0x1061);
            tft.drawString(wbuf, SCREEN_W - 24, 191);
        }
        tft.unloadFont();
    }
    if (fetching != g_prevFetching || wifiOk != g_prevWifi) {
        g_prevFetching = fetching; g_prevWifi = wifiOk;
        drawTopBar(tft, "Nixie Tüp Saat", "Tema", netGetRssi(), wifiOk, fetching);
    }
}

// =========================================================================
// 2. DIGITAL 7-SEGMENT LED CLOCK
// =========================================================================
constexpr uint8_t SEG7_MASK[10] = {
    0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
    0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111
};

void draw7SegDigit(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, int16_t t,
                   int digit, uint16_t onCol, uint16_t offCol) {
    const uint8_t m = SEG7_MASK[digit % 10];
    const int16_t half = h / 2;
    auto segCol = [&](int bit) { return (m & (1 << bit)) ? onCol : offCol; };
    // A (top), B (TR), C (BR), D (bot), E (BL), F (TL), G (mid)
    tft.fillRoundRect(x + t, y, w - 2 * t, t, 2, segCol(0));
    tft.fillRoundRect(x + w - t, y + t, t, half - t - 1, 2, segCol(1));
    tft.fillRoundRect(x + w - t, y + half + 1, t, half - t - 1, 2, segCol(2));
    tft.fillRoundRect(x + t, y + h - t, w - 2 * t, t, 2, segCol(3));
    tft.fillRoundRect(x, y + half + 1, t, half - t - 1, 2, segCol(4));
    tft.fillRoundRect(x, y + t, t, half - t - 1, 2, segCol(5));
    tft.fillRoundRect(x + t, y + half - t / 2, w - 2 * t, t, 2, segCol(6));
}

void tickDigital(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx,
                 bool fetching, bool forceFull) {
    struct tm ti{}; bool valid = false; int d[4], sec = 0, dayKey = 0;
    getClockVals(ti, valid, d, sec, dayKey);
    const bool wifiOk = netIsWifiConnected();
    constexpr int16_t xs[4] = {22, 74, 138, 190};
    constexpr uint16_t dimCol = 0x1105;

    if (forceFull) {
        tft.fillScreen(0x0042);
        drawTopBar(tft, "Dijital 7-Segment", "Tema", netGetRssi(), wifiOk, fetching);
        tft.fillRoundRect(10, 32, SCREEN_W - 20, 130, 10, 0x0863);
        tft.drawRoundRect(10, 32, SCREEN_W - 20, 130, 10, 0x1967);
        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COL_MUTED, 0x0863);
        tft.drawString("SN", 274, 62);
        tft.unloadFont();
        for (int i = 0; i < 4; ++i) {
            draw7SegDigit(tft, xs[i], 48, 42, 86, 7, d[i], COL_CYAN, dimCol);
            g_prevDigits[i] = d[i];
        }
        draw7SegDigit(tft, 248, 86, 24, 48, 4, sec / 10, COL_ACCENT, dimCol);
        draw7SegDigit(tft, 278, 86, 24, 48, 4, sec % 10, COL_ACCENT, dimCol);
        tft.fillRect(124, 72, 6, 7, COL_CYAN);
        tft.fillRect(124, 102, 6, 7, COL_CYAN);
        drawBottomNav(tft, 0);
        g_prevSec = sec;
        g_prevDay = -999;
    }

    for (int i = 0; i < 4; ++i) {
        if (d[i] != g_prevDigits[i]) {
            g_prevDigits[i] = d[i];
            draw7SegDigit(tft, xs[i], 48, 42, 86, 7, d[i], COL_CYAN, dimCol);
        }
    }
    if (sec != g_prevSec) {
        g_prevSec = sec;
        draw7SegDigit(tft, 248, 86, 24, 48, 4, sec / 10, COL_ACCENT, dimCol);
        draw7SegDigit(tft, 278, 86, 24, 48, 4, sec % 10, COL_ACCENT, dimCol);
        const uint16_t cc = (sec % 2 == 0) ? COL_CYAN : dimCol;
        tft.fillRect(124, 72, 6, 7, cc);
        tft.fillRect(124, 102, 6, 7, cc);
    }
    if (dayKey != g_prevDay || wx.updatedAtMs != g_prevWxMs) {
        g_prevDay = dayKey; g_prevWxMs = wx.updatedAtMs;
        tft.fillRoundRect(10, 170, 184, 30, 7, COL_SURFACE);
        tft.drawRoundRect(10, 170, 184, 30, 7, COL_BORDER);
        tft.fillRoundRect(200, 170, SCREEN_W - 210, 30, 7, COL_SURFACE);
        tft.drawRoundRect(200, 170, SCREEN_W - 210, 30, 7, COL_BORDER);
        tft.loadFont(SegoeUI14TR);
        if (valid) {
            char dbuf[32];
            snprintf(dbuf, sizeof(dbuf), "%d %s %s",
                     ti.tm_mday, TR_MONTHS_SHORT[ti.tm_mon % 12], TR_DAYS_LONG[ti.tm_wday % 7]);
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(COL_TEXT, COL_SURFACE);
            tft.drawString(dbuf, 18, 185);
        }
        if (wx.valid) {
            char wbuf[48];
            snprintf(wbuf, sizeof(wbuf), "%.0f° %s", wx.tempC, cfg.city);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(COL_ACCENT_2, COL_SURFACE);
            tft.drawString(wbuf, 255, 185);
        }
        tft.unloadFont();
    }
    if (fetching != g_prevFetching || wifiOk != g_prevWifi) {
        g_prevFetching = fetching; g_prevWifi = wifiOk;
        drawTopBar(tft, "Dijital 7-Segment", "Tema", netGetRssi(), wifiOk, fetching);
    }
}

// =========================================================================
// 3 & 4. ANALOG ROLEX (SUBMARINER) & PATEK PHILIPPE (NAUTILUS)
// =========================================================================
void renderWatchHands(TFT_eSprite& spr, int16_t cx, int16_t cy, int h, int m, int s,
                      uint16_t hrCol, uint16_t minCol, uint16_t secCol, bool mercedes) {
    const float ha = ((h % 12) + m / 60.0f) * 0.523599f - 1.570796f;
    const int16_t hx = cx + static_cast<int16_t>(cosf(ha) * 35);
    const int16_t hy = cy + static_cast<int16_t>(sinf(ha) * 35);
    spr.drawWideLine(cx, cy, hx, hy, 5, hrCol);
    if (mercedes) {
        const int16_t mx = cx + static_cast<int16_t>(cosf(ha) * 23);
        const int16_t my = cy + static_cast<int16_t>(sinf(ha) * 23);
        spr.fillCircle(mx, my, 6, hrCol);
        spr.fillCircle(mx, my, 4, 0xDFBD);
    }

    const float ma = m * 0.10472f - 1.570796f;
    const int16_t mx2 = cx + static_cast<int16_t>(cosf(ma) * 52);
    const int16_t my2 = cy + static_cast<int16_t>(sinf(ma) * 52);
    spr.drawWideLine(cx, cy, mx2, my2, 3, minCol);

    const float sa = s * 0.10472f - 1.570796f;
    const int16_t sx = cx + static_cast<int16_t>(cosf(sa) * 57);
    const int16_t sy = cy + static_cast<int16_t>(sinf(sa) * 57);
    const int16_t tx = cx - static_cast<int16_t>(cosf(sa) * 14);
    const int16_t ty = cy - static_cast<int16_t>(sinf(sa) * 14);
    spr.drawWideLine(tx, ty, sx, sy, 2, secCol);
    spr.fillCircle(cx, cy, 4, secCol);
}

void drawAnalogDial(TFT_eSPI& tft, bool isPatek, const struct tm& ti, bool valid) {
    constexpr int16_t S = 172;
    TFT_eSprite spr(&tft);
    spr.setColorDepth(16);
    if (!spr.createSprite(S, S)) return;

    spr.fillSprite(COL_BG);
    const int16_t cx = S / 2;
    const int16_t cy = S / 2;
    const int16_t r = 76;

    if (!isPatek) {
        // Rolex Submariner emerald-black bezel
        spr.fillCircle(cx, cy, r + 6, 0x9CF3);
        spr.fillCircle(cx, cy, r, 0x09C4);
        spr.fillTriangle(cx, cy - r + 10, cx - 6, cy - r + 2, cx + 6, cy - r + 2, COL_GOLD);
        const int16_t dr = r - 12;
        spr.fillCircle(cx, cy, dr, 0x0861);
        spr.drawCircle(cx, cy, dr, 0xBCEB);

        for (int i = 0; i < 12; ++i) {
            const float a = i * 0.523599f - 1.570796f;
            const int16_t mx = cx + static_cast<int16_t>(cosf(a) * (dr - 9));
            const int16_t my = cy + static_cast<int16_t>(sinf(a) * (dr - 9));
            if (i == 0) {
                spr.fillTriangle(mx, my + 5, mx - 5, my - 4, mx + 5, my - 4, 0xE7FB);
            } else if (i == 3) {
                spr.fillRoundRect(mx - 8, my - 6, 16, 12, 2, 0xF7BE);
                spr.setTextDatum(MC_DATUM);
                spr.setTextColor(TFT_BLACK, 0xF7BE);
                char db[4]; snprintf(db, sizeof(db), "%d", valid ? ti.tm_mday : 25);
                spr.drawString(db, mx, my, 1);
            } else if (i == 6 || i == 9) {
                spr.fillRect(mx - 3, my - 3, 6, 6, 0xE7FB);
            } else {
                spr.fillCircle(mx, my, 3, 0xE7FB);
            }
        }
        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(COL_GOLD, 0x0861);
        spr.drawString("ROLEX", cx, cy - 26, 2);
        spr.setTextColor(0x9D33, 0x0861);
        spr.drawString("OYSTER", cx, cy + 24, 1);

        renderWatchHands(spr, cx, cy, valid ? ti.tm_hour : 16, valid ? ti.tm_min : 30,
                         valid ? ti.tm_sec : 0, 0xEFFD, 0xF7DF, COL_ERR, true);
    } else {
        // Patek Philippe Nautilus
        spr.fillRoundRect(cx - r - 7, cy - 16, (r + 7) * 2, 32, 7, 0x8C73);
        spr.fillRoundRect(cx - r, cy - r, r * 2, r * 2, 30, 0xAD57);
        const int16_t dr = r - 11;
        spr.fillRoundRect(cx - dr, cy - dr, dr * 2, dr * 2, 24, 0x1107);
        for (int16_t sy = cy - dr + 10; sy <= cy + dr - 8; sy += 8) {
            spr.drawFastHLine(cx - dr + 10, sy, (dr - 10) * 2, 0x196A);
        }
        for (int i = 0; i < 12; ++i) {
            const float a = i * 0.523599f - 1.570796f;
            const int16_t x1 = cx + static_cast<int16_t>(cosf(a) * (dr - 13));
            const int16_t y1 = cy + static_cast<int16_t>(sinf(a) * (dr - 13));
            const int16_t x2 = cx + static_cast<int16_t>(cosf(a) * (dr - 5));
            const int16_t y2 = cy + static_cast<int16_t>(sinf(a) * (dr - 5));
            if (i == 3) {
                spr.fillRect(x1 - 5, y1 - 5, 14, 11, 0xF7BE);
                spr.setTextDatum(MC_DATUM);
                spr.setTextColor(TFT_BLACK, 0xF7BE);
                char db[4]; snprintf(db, sizeof(db), "%d", valid ? ti.tm_mday : 25);
                spr.drawString(db, x1 + 2, y1, 1);
            } else {
                spr.drawWideLine(x1, y1, x2, y2, (i % 3 == 0) ? 3 : 2, 0xEF7D);
            }
        }
        // Moonphase subdial
        spr.fillCircle(cx, cy + 32, 14, 0x08A4);
        spr.drawCircle(cx, cy + 32, 14, 0x9D16);
        spr.fillCircle(cx, cy + 30, 6, COL_GOLD);
        spr.fillCircle(cx + 3, cy + 28, 5, 0x08A4);

        spr.setTextDatum(MC_DATUM);
        spr.setTextColor(0xE73C, 0x1107);
        spr.drawString("PATEK PHILIPPE", cx, cy - 26, 1);
        spr.setTextColor(0x8D37, 0x1107);
        spr.drawString("GENEVE", cx, cy - 16, 1);

        renderWatchHands(spr, cx, cy, valid ? ti.tm_hour : 16, valid ? ti.tm_min : 30,
                         valid ? ti.tm_sec : 0, 0xEF7D, 0xF7DF, COL_ACCENT, false);
    }

    spr.pushSprite(14, 30);
    spr.deleteSprite();
}

void tickAnalog(TFT_eSPI& tft, bool isPatek, const DeviceConfig& cfg, const WeatherData& wx,
                bool fetching, bool forceFull) {
    struct tm ti{}; bool valid = false; int d[4], sec = 0, dayKey = 0;
    getClockVals(ti, valid, d, sec, dayKey);
    const bool wifiOk = netIsWifiConnected();
    const char* title = isPatek ? "Analog • Nautilus" : "Analog • Submariner";
    const uint16_t acc = isPatek ? COL_ACCENT : COL_GOLD;

    if (forceFull) {
        tft.fillScreen(COL_BG);
        drawTopBar(tft, title, "Tema", netGetRssi(), wifiOk, fetching);
        drawAnalogDial(tft, isPatek, ti, valid);
        drawInfoSummaryPanel(tft, ti, valid, cfg, wx, acc);
        drawBottomNav(tft, 0);
        g_prevSec = sec;
        g_prevDigits[3] = d[3];
        g_prevDay = dayKey;
        g_prevWxMs = wx.updatedAtMs;
        return;
    }

    if (sec != g_prevSec) {
        g_prevSec = sec;
        drawAnalogDial(tft, isPatek, ti, valid);
    }
    if (d[3] != g_prevDigits[3] || dayKey != g_prevDay || wx.updatedAtMs != g_prevWxMs) {
        g_prevDigits[3] = d[3]; g_prevDay = dayKey; g_prevWxMs = wx.updatedAtMs;
        drawInfoSummaryPanel(tft, ti, valid, cfg, wx, acc);
    }
    if (fetching != g_prevFetching || wifiOk != g_prevWifi) {
        g_prevFetching = fetching; g_prevWifi = wifiOk;
        drawTopBar(tft, title, "Tema", netGetRssi(), wifiOk, fetching);
    }
}

// =========================================================================
// 5. MARIO THEME CLOCK (16x16 NES SPRITE + MINUTE JUMP-HIT ANIMATION)
// =========================================================================
constexpr uint16_t SKY_COL = 0x5CB3; // #5C94FC NES sky blue

const char* const MARIO_STAND[16] PROGMEM = {
    "....RRRRR.......",
    "...RRRRRRRRR....",
    "...BBBSSKS......",
    "..BSBSSSKSSS....",
    "..BSBBSSSKBBB...",
    "..BBSSSSBBBB....",
    "....SSSSSSS.....",
    "...RROOOR.......",
    "..RRROOORRRR....",
    ".RRRROOOORRRR...",
    ".SSRYOOOORYSS...",
    ".SSSOOOOOOSSS...",
    ".SSOOOOOOOOSS...",
    "...OOO..OOO.....",
    "..BBB....BBB....",
    ".BBBB....BBBB..."
};

const char* const MARIO_JUMP[16] PROGMEM = {
    "............SSS.",
    "....RRRRR...SSS.",
    "...RRRRRRRRRSSR.",
    "...BBBSSKS..RRR.",
    "..BSBSSSKSSRRR..",
    "..BSBBSSSKBBB...",
    "..BBSSSSBBBB....",
    "....SSSSSSS.....",
    ".RRRRROOORR.....",
    "RRRRRROOORRR....",
    "SS...OOOOOO.....",
    "....OOOOOOY.....",
    "...OOOOOOOO.....",
    "..BBBOOOOOOBBB..",
    "..BBB....BBB....",
    ".........BBBB..."
};

const uint8_t PIX_5X7[10][7] PROGMEM = {
    {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110}, // 0
    {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110}, // 1
    {0b01110,0b10001,0b00001,0b00110,0b01000,0b10000,0b11111}, // 2
    {0b11110,0b00001,0b00001,0b01110,0b00001,0b00001,0b11110}, // 3
    {0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010}, // 4
    {0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110}, // 5
    {0b01110,0b10000,0b10000,0b11110,0b10001,0b10001,0b01110}, // 6
    {0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000}, // 7
    {0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110}, // 8
    {0b01110,0b10001,0b10001,0b01111,0b00001,0b00001,0b01110}, // 9
};

uint16_t marioPalColor(char c) {
    if (c == 'R') return 0xE103; // Red
    if (c == 'B') return 0x81E2; // Brown
    if (c == 'S') return 0xFDF0; // Peach
    if (c == 'K') return 0x1082; // Dark
    if (c == 'O') return 0x22BC; // Overalls blue
    if (c == 'Y') return 0xFE65; // Gold button
    return 0;
}

template <typename Target>
void drawMarioSprite(Target& dst, const char* const grid[16], int16_t x, int16_t y) {
    for (int r = 0; r < 16; ++r) {
        const char* row = grid[r];
        for (int c = 0; c < 16; ++c) {
            const uint16_t col = marioPalColor(row[c]);
            if (col != 0) dst.fillRect(x + c * 2, y + r * 2, 2, 2, col);
        }
    }
}

template <typename Target>
void drawSmbBlock(Target& dst, int16_t x, int16_t y, int digit) {
    constexpr int16_t sz = 42;
    dst.fillRect(x, y, sz, sz, 0x1881);
    dst.fillRect(x + 2, y + 2, sz - 4, sz - 4, 0xF543);
    dst.fillRect(x + 2, y + 2, sz - 4, 2, 0xFF11);
    dst.fillRect(x + 2, y + 2, 2, sz - 4, 0xFF11);
    dst.fillRect(x + 3, y + sz - 4, sz - 5, 2, 0xA2C1);
    dst.fillRect(x + sz - 4, y + 3, 2, sz - 5, 0xA2C1);
    // 4 corner rivets
    dst.fillRect(x + 6, y + 6, 3, 3, 0x81E1);
    dst.fillRect(x + sz - 9, y + 6, 3, 3, 0x81E1);
    dst.fillRect(x + 6, y + sz - 9, 3, 3, 0x81E1);
    dst.fillRect(x + sz - 9, y + sz - 9, 3, 3, 0x81E1);

    const int16_t ox = x + (sz - 15) / 2;
    const int16_t oy = y + (sz - 21) / 2;
    for (int r = 0; r < 7; ++r) {
        uint8_t bits = pgm_read_byte(&PIX_5X7[digit % 10][r]);
        for (int c = 0; c < 5; ++c) {
            if (bits & (1 << (4 - c))) {
                dst.fillRect(ox + c * 3 + 2, oy + r * 3 + 2, 3, 3, 0x4900);
                dst.fillRect(ox + c * 3, oy + r * 3, 3, 3, 0xFFFC);
            }
        }
    }
}

void drawMarioStaticBg(TFT_eSPI& tft, const struct tm& ti, bool valid,
                       const DeviceConfig& cfg, const WeatherData& wx) {
    tft.fillRect(0, TOP_BAR_H + 1, SCREEN_W, NAV_BAR_Y - TOP_BAR_H - 4, SKY_COL);
    // Pixel clouds clear of HUD
    for (int16_t cx : {46, 274}) {
        int16_t cy = (cx < 100) ? 124 : 46;
        tft.fillRoundRect(cx - 22, cy - 5, 44, 12, 5, TFT_WHITE);
        tft.fillCircle(cx, cy - 4, 10, TFT_WHITE);
    }
    // Spotted hill on left
    tft.fillCircle(38, 176, 42, 0x1D46);
    tft.fillCircle(26, 156, 4, 0x0B83);
    tft.fillCircle(50, 162, 3, 0x0B83);

    // Shaded green warp pipe on right
    tft.fillRect(262, 146, 40, 30, 0x2585);
    tft.drawRect(262, 146, 40, 30, 0x0A42);
    tft.fillRect(268, 147, 6, 28, 0x7F4D);
    tft.fillRect(292, 147, 7, 28, 0x13A3);
    tft.fillRect(258, 132, 48, 15, 0x2E67);
    tft.drawRect(258, 132, 48, 15, 0x0A42);
    tft.fillRect(265, 134, 8, 11, 0x8FAD);

    // Two rows of NES brick ground
    for (int16_t ry : {176, 192}) {
        for (int16_t gx = 0; gx < SCREEN_W; gx += 16) {
            tft.fillRect(gx, ry, 15, 15, 0xCA61);
            tft.drawRect(gx, ry, 16, 16, 0x1881);
            tft.drawFastHLine(gx + 1, ry + 1, 14, 0xF50E);
            tft.drawFastVLine(gx + 8, ry + 2, 6, 0x1881);
            tft.drawFastHLine(gx + 1, ry + 8, 14, 0x1881);
        }
    }

    // Arcade HUD
    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    if (valid) {
        char dbuf[20];
        snprintf(dbuf, sizeof(dbuf), "%d %s %s", ti.tm_mday,
                 TR_MONTHS_SHORT[ti.tm_mon % 12], TR_DAYS_SHORT[ti.tm_wday % 7]);
        tft.setTextColor(TFT_WHITE, SKY_COL);
        tft.drawString(dbuf, 12, 32);
    }
    if (wx.valid) {
        tft.fillCircle(104, 38, 6, COL_ACCENT_2);
        char wbuf[16];
        snprintf(wbuf, sizeof(wbuf), "%.0f°C", wx.tempC);
        tft.setTextColor(0xFF92, SKY_COL);
        tft.drawString(wbuf, 115, 32);
    }
    tft.unloadFont();
    (void)cfg;
}

void animateMarioJumpHit(TFT_eSPI& tft, int oldDigit, int newDigit) {
    // Animate inside offscreen sprite covering x=182..242 (w=60), y=32..176 (h=144)
    constexpr int16_t sx = 182, sy = 32, sw = 60, sh = 144;
    TFT_eSprite spr(&tft);
    spr.setColorDepth(16);
    if (!spr.createSprite(sw, sh)) {
        drawSmbBlock(tft, 188, 68, newDigit);
        drawMarioSprite(tft, MARIO_STAND, 193, 144);
        return;
    }

    constexpr int FRAMES = 12;
    for (int f = 0; f <= FRAMES; ++f) {
        spr.fillSprite(SKY_COL);
        // Normalized jump arc 0..1..0
        float t = static_cast<float>(f) / FRAMES;
        float arc = 4.0f * t * (1.0f - t); // peaks at 1.0 at f == FRAMES/2
        int16_t marioY = 144 - static_cast<int16_t>(arc * 42.0f);
        bool hitPhase = (f >= FRAMES / 2);
        int16_t blockBounce = (f == 6 || f == 7) ? -10 : ((f == 5 || f == 8) ? -5 : 0);

        // Draw popping coin after hit
        if (f >= 6 && f <= 11) {
            int16_t coinY = (58 - sy) - (f - 5) * 4;
            if (coinY >= 4) {
                spr.fillEllipse(209 - sx, coinY, 5, 8, 0xFEA6);
                spr.drawEllipse(209 - sx, coinY, 5, 8, 0x7A21);
                spr.drawFastVLine(209 - sx, coinY - 4, 8, 0xFFFC);
            }
        }

        drawSmbBlock(spr, 188 - sx, (68 - sy) + blockBounce, hitPhase ? newDigit : oldDigit);
        drawMarioSprite(spr, (arc > 0.2f) ? MARIO_JUMP : MARIO_STAND,
                        (arc > 0.2f ? 186 : 193) - sx, marioY - sy);
        spr.pushSprite(sx, sy);
        delay(28);
    }
    spr.deleteSprite();
}

void tickMario(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx,
               bool fetching, bool forceFull) {
    struct tm ti{}; bool valid = false; int d[4], sec = 0, dayKey = 0;
    getClockVals(ti, valid, d, sec, dayKey);
    const bool wifiOk = netIsWifiConnected();
    constexpr int16_t xs[4] = {24, 72, 140, 188};

    if (forceFull) {
        tft.fillScreen(COL_BG);
        drawTopBar(tft, "Mario Saat", "Tema", netGetRssi(), wifiOk, fetching);
        drawMarioStaticBg(tft, ti, valid, cfg, wx);
        for (int i = 0; i < 4; ++i) {
            drawSmbBlock(tft, xs[i], 68, d[i]);
            g_prevDigits[i] = d[i];
        }
        drawMarioSprite(tft, MARIO_STAND, 193, 144);
        tft.fillRect(124, 80, 5, 5, TFT_WHITE);
        tft.fillRect(124, 96, 5, 5, TFT_WHITE);
        drawBottomNav(tft, 0);
        g_prevSec = sec; g_prevDay = dayKey; g_prevWxMs = wx.updatedAtMs;
        return;
    }

    for (int i = 0; i < 3; ++i) {
        if (d[i] != g_prevDigits[i]) {
            g_prevDigits[i] = d[i];
            drawSmbBlock(tft, xs[i], 68, d[i]);
        }
    }
    if (d[3] != g_prevDigits[3]) {
        int oldD = g_prevDigits[3];
        g_prevDigits[3] = d[3];
        animateMarioJumpHit(tft, oldD, d[3]);
    }
    if (sec != g_prevSec) {
        g_prevSec = sec;
        const uint16_t cc = (sec % 2 == 0) ? TFT_WHITE : 0x2292;
        tft.fillRect(124, 80, 5, 5, cc);
        tft.fillRect(124, 96, 5, 5, cc);
    }
    if (dayKey != g_prevDay || wx.updatedAtMs != g_prevWxMs) {
        g_prevDay = dayKey; g_prevWxMs = wx.updatedAtMs;
        tickMario(tft, cfg, wx, fetching, true);
        return;
    }
    if (fetching != g_prevFetching || wifiOk != g_prevWifi) {
        g_prevFetching = fetching; g_prevWifi = wifiOk;
        drawTopBar(tft, "Mario Saat", "Tema", netGetRssi(), wifiOk, fetching);
    }
}

// =========================================================================
// 6. TETRIS THEME CLOCK (ONLY CHANGING DIGIT RECEIVES FALLING PIECES)
// =========================================================================
const char* const TETRIS_MAP[10][5] PROGMEM = {
    {"III", "S.L", "S.L", "O.J", "OOJ"}, // 0
    {".I.", "II.", ".I.", ".I.", "LLL"}, // 1
    {"III", "..L", "TTT", "Z..", "OOO"}, // 2
    {"LLL", "..L", "SSS", "..J", "JJJ"}, // 3
    {"S.L", "S.L", "TTT", "..J", "..J"}, // 4
    {"ZZZ", "Z..", "TTT", "..L", "OOO"}, // 5
    {"TTT", "T..", "ZZO", "ZOO", "JJO"}, // 6
    {"III", "..L", "..L", ".J.", ".J."}, // 7
    {"TTT", "S.L", "OOO", "Z.J", "ZZJ"}, // 8
    {"TTT", "S.L", "OOO", "..J", "JJJ"}  // 9
};

uint16_t tetroColor(char k) {
    if (k == 'I') return 0x26FF; // Cyan
    if (k == 'O') return 0xFE85; // Yellow
    if (k == 'T') return 0xB27E; // Purple
    if (k == 'S') return 0x470C; // Green
    if (k == 'Z') return 0xF229; // Red
    if (k == 'J') return 0x3C1F; // Blue
    if (k == 'L') return 0xFC84; // Orange
    return 0;
}

template <typename Target>
void drawTetroCell(Target& dst, int16_t x, int16_t y, int16_t bs, uint16_t c) {
    dst.fillRect(x, y, bs - 1, bs - 1, c);
    dst.drawFastHLine(x, y, bs - 1, TFT_WHITE);
    dst.drawFastVLine(x, y, bs - 1, TFT_WHITE);
}

void drawTetrisColumnStatic(TFT_eSPI& tft, int colIdx, int digit) {
    constexpr int16_t xs[4] = {20, 66, 124, 170};
    constexpr int16_t bs = 12;
    constexpr int16_t dy = 92;
    const int16_t x0 = xs[colIdx];

    // Clear column above floor
    tft.fillRect(x0, 30, 3 * bs, 144, 0x0883);
    for (int16_t gx = x0; gx <= x0 + 3 * bs; gx += bs) {
        tft.drawFastVLine(gx, 30, 144, 0x10E5);
    }
    for (int16_t gy = 30; gy < 174; gy += bs) {
        tft.drawFastHLine(x0, gy, 3 * bs, 0x10E5);
    }

    for (int r = 0; r < 5; ++r) {
        const char* row = TETRIS_MAP[digit % 10][r];
        for (int c = 0; c < 3; ++c) {
            if (row[c] != '.') {
                drawTetroCell(tft, x0 + c * bs, dy + r * bs, bs, tetroColor(row[c]));
            }
        }
    }
}

void animateTetrisDigitDrop(TFT_eSPI& tft, int colIdx, int newDigit) {
    constexpr int16_t xs[4] = {20, 66, 124, 170};
    constexpr int16_t bs = 12;
    constexpr int16_t dy = 92;
    const int16_t x0 = xs[colIdx];
    constexpr int16_t cw = 3 * bs;
    constexpr int16_t ch = 144;

    TFT_eSprite spr(&tft);
    spr.setColorDepth(16);
    if (!spr.createSprite(cw, ch)) {
        drawTetrisColumnStatic(tft, colIdx, newDigit);
        return;
    }

    // Drop row-by-row from bottom (r=4) up to top (r=0), ONLY in this changing column!
    for (int settleRow = 4; settleRow >= 0; --settleRow) {
        const int16_t targetY = (dy - 30) + settleRow * bs;
        for (int16_t dropY = 4; dropY <= targetY; dropY += 12) {
            spr.fillSprite(0x0883);
            for (int16_t gx = 0; gx < cw; gx += bs) spr.drawFastVLine(gx, 0, ch, 0x10E5);
            for (int16_t gy = 0; gy < ch; gy += bs) spr.drawFastHLine(0, gy, cw, 0x10E5);

            // Already landed rows below settleRow
            for (int r = settleRow + 1; r < 5; ++r) {
                const char* row = TETRIS_MAP[newDigit % 10][r];
                for (int c = 0; c < 3; ++c) {
                    if (row[c] != '.') {
                        drawTetroCell(spr, c * bs, (dy - 30) + r * bs, bs, tetroColor(row[c]));
                    }
                }
            }
            // Currently dropping row
            const char* curRow = TETRIS_MAP[newDigit % 10][settleRow];
            for (int c = 0; c < 3; ++c) {
                if (curRow[c] != '.') {
                    drawTetroCell(spr, c * bs, dropY, bs, tetroColor(curRow[c]));
                }
            }
            spr.pushSprite(x0, 30);
            delay(18);
        }
    }
    spr.deleteSprite();
    drawTetrisColumnStatic(tft, colIdx, newDigit);
}

void drawTetrisSidePanel(TFT_eSPI& tft, const struct tm& ti, bool valid, const WeatherData& wx) {
    constexpr int16_t sx = 228;
    tft.fillRoundRect(sx, 28, SCREEN_W - sx - 10, 177, 8, COL_SURFACE);
    tft.drawRoundRect(sx, 28, SCREEN_W - sx - 10, 177, 8, COL_BORDER);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString("SONRAKİ", sx + 41, 42);
    tft.unloadFont();

    // Mini L-piece
    for (auto p : {Pt{26, 60}, Pt{26, 72}, Pt{26, 84}, Pt{38, 84}}) {
        drawTetroCell(tft, sx + p.x, p.y, 11, COL_ACCENT_2);
    }
    tft.drawFastHLine(sx + 10, 110, 62, COL_BORDER);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    if (valid) {
        char dbuf[16];
        snprintf(dbuf, sizeof(dbuf), "%d %s", ti.tm_mday, TR_MONTHS_SHORT[ti.tm_mon % 12]);
        tft.setTextColor(COL_TEXT, COL_SURFACE);
        tft.drawString(dbuf, sx + 41, 128);
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString(TR_DAYS_SHORT[ti.tm_wday % 7], sx + 41, 148);
    }
    if (wx.valid) {
        char wbuf[12];
        snprintf(wbuf, sizeof(wbuf), "%.0f°C", wx.tempC);
        tft.setTextColor(COL_CYAN, COL_SURFACE);
        tft.drawString(wbuf, sx + 41, 178);
    }
    tft.unloadFont();
}

void tickTetris(TFT_eSPI& tft, const WeatherData& wx, bool fetching, bool forceFull) {
    struct tm ti{}; bool valid = false; int d[4], sec = 0, dayKey = 0;
    getClockVals(ti, valid, d, sec, dayKey);
    const bool wifiOk = netIsWifiConnected();

    if (forceFull) {
        tft.fillScreen(COL_BG);
        drawTopBar(tft, "Tetris Saat", "Tema", netGetRssi(), wifiOk, fetching);
        tft.fillRect(10, 28, 212, 177, 0x0883);
        tft.drawRect(10, 28, 212, 177, 0x3A2C);
        for (int16_t gx = 22; gx < 222; gx += 12) tft.drawFastVLine(gx, 30, 173, 0x10E5);
        for (int16_t gy = 40; gy < 205; gy += 12) tft.drawFastHLine(12, gy, 208, 0x10E5);

        // Floor debris
        const char keys[6] = {'Z', 'I', 'O', 'S', 'T', 'L'};
        for (int i = 0; i < 17; ++i) {
            drawTetroCell(tft, 14 + i * 12, 191, 12, tetroColor(keys[i % 6]));
            if (i % 4 != 3) drawTetroCell(tft, 14 + i * 12, 179, 12, tetroColor(keys[(i + 2) % 6]));
        }

        for (int i = 0; i < 4; ++i) {
            drawTetrisColumnStatic(tft, i, d[i]);
            g_prevDigits[i] = d[i];
        }
        drawTetroCell(tft, 108, 106, 10, COL_GOLD);
        drawTetroCell(tft, 108, 130, 10, COL_GOLD);
        drawTetrisSidePanel(tft, ti, valid, wx);
        drawBottomNav(tft, 0);
        g_prevSec = sec; g_prevDay = dayKey; g_prevWxMs = wx.updatedAtMs;
        return;
    }

    // Drop pieces ONLY in columns whose digit actually changed!
    for (int i = 0; i < 4; ++i) {
        if (d[i] != g_prevDigits[i]) {
            g_prevDigits[i] = d[i];
            animateTetrisDigitDrop(tft, i, d[i]);
        }
    }
    if (sec != g_prevSec) {
        g_prevSec = sec;
        const uint16_t cc = (sec % 2 == 0) ? COL_GOLD : 0x2104;
        drawTetroCell(tft, 108, 106, 10, cc);
        drawTetroCell(tft, 108, 130, 10, cc);
    }
    if (dayKey != g_prevDay || wx.updatedAtMs != g_prevWxMs) {
        g_prevDay = dayKey; g_prevWxMs = wx.updatedAtMs;
        drawTetrisSidePanel(tft, ti, valid, wx);
    }
    if (fetching != g_prevFetching || wifiOk != g_prevWifi) {
        g_prevFetching = fetching; g_prevWifi = wifiOk;
        drawTopBar(tft, "Tetris Saat", "Tema", netGetRssi(), wifiOk, fetching);
    }
}
} // namespace

void uiThemesReset() {
    for (int i = 0; i < 4; ++i) g_prevDigits[i] = -1;
    g_prevSec = -1;
    g_prevDay = -1;
    g_prevWxMs = 0;
}

void uiThemesTick(TFT_eSPI& tft, ClockTheme theme, const DeviceConfig& cfg,
                  const WeatherData& wx, bool fetching, bool forceFull) {
    if (forceFull) uiThemesReset();

    switch (theme) {
        case ClockTheme::NIXIE:   tickNixie(tft, cfg, wx, fetching, forceFull); break;
        case ClockTheme::DIGITAL: tickDigital(tft, cfg, wx, fetching, forceFull); break;
        case ClockTheme::ROLEX:   tickAnalog(tft, false, cfg, wx, fetching, forceFull); break;
        case ClockTheme::PATEK:   tickAnalog(tft, true, cfg, wx, fetching, forceFull); break;
        case ClockTheme::MARIO:   tickMario(tft, cfg, wx, fetching, forceFull); break;
        case ClockTheme::TETRIS:  tickTetris(tft, wx, fetching, forceFull); break;
        default: break;
    }
}

void uiThemesDemo(TFT_eSPI& tft, ClockTheme theme, const DeviceConfig& cfg,
                  const WeatherData& wx) {
    struct tm ti{}; bool valid = false; int d[4], sec = 0, dayKey = 0;
    getClockVals(ti, valid, d, sec, dayKey);
    if (theme == ClockTheme::MARIO) {
        animateMarioJumpHit(tft, (d[3] + 9) % 10, d[3]);
    } else if (theme == ClockTheme::TETRIS) {
        animateTetrisDigitDrop(tft, 3, d[3]);
    } else {
        uiThemesTick(tft, theme, cfg, wx, netIsFetching(), true);
    }
}
