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
    const int16_t px = SX(200);
    const int16_t pw = SCREEN_W - px - SX(10);
    tft.fillRoundRect(px, SY(32), pw, SY(170), 10, COL_SURFACE);
    tft.drawRoundRect(px, SY(32), pw, SY(170), 10, COL_BORDER);

    char tbuf[8];
    if (valid) snprintf(tbuf, sizeof(tbuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
    else strlcpy(tbuf, "--:--", sizeof(tbuf));

    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(tbuf, px + pw / 2, SY(56));
    tft.unloadFont();

    tft.drawFastHLine(px + SX(10), SY(76), pw - SX(20), COL_BORDER);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    if (valid) {
        char dbuf[24];
        snprintf(dbuf, sizeof(dbuf), "%d %s %s",
                 ti.tm_mday, TR_MONTHS_SHORT[ti.tm_mon % 12], TR_DAYS_SHORT[ti.tm_wday % 7]);
        tft.setTextColor(COL_TEXT, COL_SURFACE);
        tft.drawString(dbuf, px + pw / 2, SY(96));
    }
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(cfg.city, px + pw / 2, SY(118));

    if (wx.valid) {
        char wbuf[20];
        snprintf(wbuf, sizeof(wbuf), "%.0f°C", wx.tempC);
        tft.setTextColor(accentCol, COL_SURFACE);
        tft.drawString(wbuf, px + pw / 2, SY(140));
    }

    tft.drawFastHLine(px + SX(10), SY(158), pw - SX(20), COL_BORDER);
    bool hasAlarm = false;
    for (int i = 0; i < 3; ++i) {
        if (cfg.alarms[i].enabled) {
            char abuf[20];
            snprintf(abuf, sizeof(abuf), "Alarm %02u:%02u", cfg.alarms[i].hour, cfg.alarms[i].minute);
            tft.setTextColor(COL_OK, COL_SURFACE);
            tft.drawString(abuf, px + pw / 2, SY(178));
            hasAlarm = true;
            break;
        }
    }
    if (!hasAlarm) {
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString("Alarm Kapalı", px + pw / 2, SY(178));
    }
    tft.unloadFont();
}

// =========================================================================
// 1. REALISTIC NIXIE TUBE CLOCK (IN-18)
// =========================================================================
struct Pt { uint8_t x; uint8_t y; };
const Pt NIXIE_PATHS[10][12] = {
    {{50,4},{80,8},{90,30},{90,70},{80,92},{50,96},{20,92},{10,70},{10,30},{20,8},{50,4},{255,255}},
    {{30,20},{54,5},{54,95},{30,95},{78,95},{255,255}},
    {{16,24},{26,7},{52,4},{78,8},{86,24},{82,42},{16,95},{88,95},{255,255}},
    {{18,14},{48,4},{78,12},{84,28},{72,44},{42,48},{76,54},{88,72},{78,90},{48,96},{16,86},{255,255}},
    {{74,95},{74,5},{14,66},{88,66},{255,255}},
    {{82,5},{24,5},{20,44},{54,42},{82,52},{86,74},{74,92},{46,96},{18,86},{255,255}},
    {{76,12},{52,4},{24,14},{12,44},{12,76},{26,93},{54,96},{82,88},{88,66},{76,48},{48,44},{18,52}},
    {{16,6},{86,6},{40,95},{255,255}},
    {{50,4},{76,9},{82,24},{72,41},{24,54},{12,72},{22,91},{50,96},{78,91},{88,72},{76,54},{24,41}},
    {{82,48},{50,54},{18,44},{14,22},{28,7},{56,4},{82,16},{88,48},{78,82},{52,96},{24,90},{255,255}}
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
    const int16_t tw = SX(60), th = SY(128), ty = SY(30);
    tft.fillRoundRect(x + tw / 2 - SX(4), ty - SY(4), SX(8), SY(8), 3, 0x10A3);
    tft.drawRoundRect(x + tw / 2 - SX(4), ty - SY(4), SX(8), SY(8), 3, 0x5ACB);
    tft.fillRoundRect(x, ty, tw, th, SR(20), 0x0821);
    tft.drawRoundRect(x, ty, tw, th, SR(20), 0x5AEB);
    tft.fillRoundRect(x + SX(3), ty + SY(4), tw - SX(6), th - SY(16), SR(17), 0x1041);

    tft.fillRect(x + SX(10), ty + SY(16), tw - SX(20), 3, 0x3944);
    tft.fillRect(x + SX(10), ty + th - SY(22), tw - SX(20), 3, 0x3944);
    tft.drawFastVLine(x + SX(12), ty + SY(18), th - SY(38), 0x4207);
    tft.drawFastVLine(x + tw - SX(12), ty + SY(18), th - SY(38), 0x4207);

    for (int16_t s = -SX(32); s < SX(84); s += SX(10)) {
        int16_t x1 = max<int16_t>(x + SX(6), x + SX(6) + s);
        int16_t y1 = ty + SY(20) + max<int16_t>(0, -s);
        int16_t x2 = min<int16_t>(x + tw - SX(6), x + SX(6) + s + SX(64));
        int16_t y2 = ty + SY(20) + min<int16_t>(SY(84), SY(64) - s);
        if (x1 < x2 && y1 < y2) tft.drawLine(x1, y1, x2, y2, 0x20C2);
    }
    drawNixieWire(tft, 8, x + SX(13), ty + SY(28), SX(34), SY(68), 0, 0x2923);

    drawNixieWire(tft, digit, x + SX(13), ty + SY(28), SX(34), SY(68), SR(3), 0x91C0);
    drawNixieWire(tft, digit, x + SX(13), ty + SY(28), SX(34), SY(68), SR(1), 0xFC42);
    drawNixieWire(tft, digit, x + SX(13), ty + SY(28), SX(34), SY(68), 0, 0xFED3);

    tft.drawFastVLine(x + SX(6), ty + SY(20), SY(36), 0x8430);
    tft.drawFastVLine(x + tw - SX(6), ty + SY(24), th - SY(48), 0x4228);

    tft.fillRoundRect(x + SX(3), ty + th - SY(14), tw - SX(6), SY(12), 4, 0x59E3);
    tft.drawRoundRect(x + SX(3), ty + th - SY(14), tw - SX(6), SY(12), 4, 0x9B46);
    tft.fillRect(x + SX(6), ty + th - SY(3), tw - SX(12), SY(7), 0x18C2);
    for (int16_t px = x + SX(12); px < x + tw - SX(10); px += SX(9)) {
        tft.drawFastVLine(px, ty + th + SY(4), SY(5), 0xAD55);
    }
}

void drawNixieColon(TFT_eSPI& tft, bool on) {
    for (int16_t cy : {SY(76), SY(110)}) {
        tft.fillRoundRect(SX(154), cy - SY(8), SX(12), SY(16), 5, 0x1041);
        tft.drawRoundRect(SX(154), cy - SY(8), SX(12), SY(16), 5, 0x528A);
        if (on) {
            tft.fillCircle(SX(160), cy, SR(4), 0x91C0);
            tft.fillCircle(SX(160), cy, SR(2), 0xFC83);
            tft.drawPixel(SX(160), cy, 0xFED3);
        }
    }
}

void tickNixie(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx,
               bool fetching, bool forceFull) {
    struct tm ti{}; bool valid = false; int d[4], sec = 0, dayKey = 0;
    getClockVals(ti, valid, d, sec, dayKey);
    const bool wifiOk = netIsWifiConnected();
    const int16_t xs[4] = {SX(16), SX(86), SX(174), SX(244)};

    if (forceFull) {
        tft.fillScreen(0x0821);
        drawTopBar(tft, "Nixie Tüp Saat", "Tema", netGetRssi(), wifiOk, fetching);
        tft.fillRoundRect(SX(10), SY(152), SCREEN_W - SX(20), SY(22), 5, 0x28C2);
        tft.drawRoundRect(SX(10), SY(152), SCREEN_W - SX(20), SY(22), 5, 0x4983);
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
        tft.fillRoundRect(SX(14), SY(179), SCREEN_W - SX(28), SY(24), 6, 0x1061);
        tft.drawRoundRect(SX(14), SY(179), SCREEN_W - SX(28), SY(24), 6, 0x4163);
        tft.loadFont(SegoeUI14TR);
        if (valid) {
            char dbuf[40];
            snprintf(dbuf, sizeof(dbuf), "%d %s %s",
                     ti.tm_mday, TR_MONTHS_SHORT[ti.tm_mon % 12], TR_DAYS_LONG[ti.tm_wday % 7]);
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(0xEDE6, 0x1061);
            tft.drawString(dbuf, SX(24), SY(191));
        }
        if (wx.valid) {
            char wbuf[40];
            snprintf(wbuf, sizeof(wbuf), "%s %.0f°C", cfg.city, wx.tempC);
            tft.setTextDatum(MR_DATUM);
            tft.setTextColor(0xFC85, 0x1061);
            tft.drawString(wbuf, SCREEN_W - SX(24), SY(191));
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
    const int16_t xs[4] = {SX(22), SX(74), SX(138), SX(190)};
    constexpr uint16_t dimCol = 0x1105;

    if (forceFull) {
        tft.fillScreen(0x0042);
        drawTopBar(tft, "Dijital 7-Segment", "Tema", netGetRssi(), wifiOk, fetching);
        tft.fillRoundRect(SX(10), SY(32), SCREEN_W - SX(20), SY(130), 10, 0x0863);
        tft.drawRoundRect(SX(10), SY(32), SCREEN_W - SX(20), SY(130), 10, 0x1967);
        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COL_MUTED, 0x0863);
        tft.drawString("SN", SX(274), SY(62));
        tft.unloadFont();
        for (int i = 0; i < 4; ++i) {
            draw7SegDigit(tft, xs[i], SY(48), SX(42), SY(86), SR(7), d[i], COL_CYAN, dimCol);
            g_prevDigits[i] = d[i];
        }
        draw7SegDigit(tft, SX(248), SY(86), SX(24), SY(48), SR(4), sec / 10, COL_ACCENT, dimCol);
        draw7SegDigit(tft, SX(278), SY(86), SX(24), SY(48), SR(4), sec % 10, COL_ACCENT, dimCol);
        tft.fillRect(SX(124), SY(72), SX(6), SY(7), COL_CYAN);
        tft.fillRect(SX(124), SY(102), SX(6), SY(7), COL_CYAN);
        drawBottomNav(tft, 0);
        g_prevSec = sec;
        g_prevDay = -999;
    }

    for (int i = 0; i < 4; ++i) {
        if (d[i] != g_prevDigits[i]) {
            g_prevDigits[i] = d[i];
            draw7SegDigit(tft, xs[i], SY(48), SX(42), SY(86), SR(7), d[i], COL_CYAN, dimCol);
        }
    }
    if (sec != g_prevSec) {
        g_prevSec = sec;
        draw7SegDigit(tft, SX(248), SY(86), SX(24), SY(48), SR(4), sec / 10, COL_ACCENT, dimCol);
        draw7SegDigit(tft, SX(278), SY(86), SX(24), SY(48), SR(4), sec % 10, COL_ACCENT, dimCol);
        const uint16_t cc = (sec % 2 == 0) ? COL_CYAN : dimCol;
        tft.fillRect(SX(124), SY(72), SX(6), SY(7), cc);
        tft.fillRect(SX(124), SY(102), SX(6), SY(7), cc);
    }
    if (dayKey != g_prevDay || wx.updatedAtMs != g_prevWxMs) {
        g_prevDay = dayKey; g_prevWxMs = wx.updatedAtMs;
        tft.fillRoundRect(SX(10), SY(170), SX(184), SY(30), 7, COL_SURFACE);
        tft.drawRoundRect(SX(10), SY(170), SX(184), SY(30), 7, COL_BORDER);
        tft.fillRoundRect(SX(200), SY(170), SCREEN_W - SX(210), SY(30), 7, COL_SURFACE);
        tft.drawRoundRect(SX(200), SY(170), SCREEN_W - SX(210), SY(30), 7, COL_BORDER);
        tft.loadFont(SegoeUI14TR);
        if (valid) {
            char dbuf[32];
            snprintf(dbuf, sizeof(dbuf), "%d %s %s",
                     ti.tm_mday, TR_MONTHS_SHORT[ti.tm_mon % 12], TR_DAYS_LONG[ti.tm_wday % 7]);
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(COL_TEXT, COL_SURFACE);
            tft.drawString(dbuf, SX(18), SY(185));
        }
        if (wx.valid) {
            char wbuf[48];
            snprintf(wbuf, sizeof(wbuf), "%.0f° %s", wx.tempC, cfg.city);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(COL_ACCENT_2, COL_SURFACE);
            tft.drawString(wbuf, SX(255), SY(185));
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
template <typename Target>
void renderWatchHands(Target& dst, int16_t cx, int16_t cy, int h, int m, int s,
                      uint16_t hrCol, uint16_t minCol, uint16_t secCol, bool mercedes) {
    const float ha = ((h % 12) + m / 60.0f) * 0.523599f - 1.570796f;
    const int16_t hx = cx + static_cast<int16_t>(cosf(ha) * SR(35));
    const int16_t hy = cy + static_cast<int16_t>(sinf(ha) * SR(35));
    dst.drawWideLine(cx, cy, hx, hy, SR(5), hrCol);
    if (mercedes) {
        const int16_t mx = cx + static_cast<int16_t>(cosf(ha) * SR(23));
        const int16_t my = cy + static_cast<int16_t>(sinf(ha) * SR(23));
        dst.fillCircle(mx, my, SR(6), hrCol);
        dst.fillCircle(mx, my, SR(4), 0xDFBD);
    }

    const float ma = m * 0.10472f - 1.570796f;
    const int16_t mx2 = cx + static_cast<int16_t>(cosf(ma) * SR(52));
    const int16_t my2 = cy + static_cast<int16_t>(sinf(ma) * SR(52));
    dst.drawWideLine(cx, cy, mx2, my2, SR(3), minCol);

    const float sa = s * 0.10472f - 1.570796f;
    const int16_t sx = cx + static_cast<int16_t>(cosf(sa) * SR(57));
    const int16_t sy = cy + static_cast<int16_t>(sinf(sa) * SR(57));
    const int16_t tx = cx - static_cast<int16_t>(cosf(sa) * SR(14));
    const int16_t ty = cy - static_cast<int16_t>(sinf(sa) * SR(14));
    dst.drawWideLine(tx, ty, sx, sy, 2, secCol);
    dst.fillCircle(cx, cy, SR(4), secCol);
}

template <typename Target>
void drawDialContents(Target& dst, int16_t cx, int16_t cy, bool isPatek, const struct tm& ti, bool valid) {
    const int16_t r = SR(74);
    if (!isPatek) {
        dst.fillCircle(cx, cy, r + SR(6), 0x9CF3);
        dst.fillCircle(cx, cy, r, 0x09C4);
        dst.fillTriangle(cx, cy - r + SR(10), cx - SR(6), cy - r + 2, cx + SR(6), cy - r + 2, COL_GOLD);
        const int16_t dr = r - SR(12);
        dst.fillCircle(cx, cy, dr, 0x0861);
        dst.drawCircle(cx, cy, dr, 0xBCEB);

        for (int i = 0; i < 12; ++i) {
            const float a = i * 0.523599f - 1.570796f;
            const int16_t mx = cx + static_cast<int16_t>(cosf(a) * (dr - SR(9)));
            const int16_t my = cy + static_cast<int16_t>(sinf(a) * (dr - SR(9)));
            if (i == 0) {
                dst.fillTriangle(mx, my + SR(5), mx - SR(5), my - SR(4), mx + SR(5), my - SR(4), 0xE7FB);
            } else if (i == 3) {
                dst.fillRoundRect(mx - SR(8), my - SR(6), SR(16), SR(12), 2, 0xF7BE);
                dst.setTextDatum(MC_DATUM);
                dst.setTextColor(TFT_BLACK, 0xF7BE);
                char db[4]; snprintf(db, sizeof(db), "%d", valid ? ti.tm_mday : 25);
                dst.drawString(db, mx, my, 1);
            } else if (i == 6 || i == 9) {
                dst.fillRect(mx - SR(3), my - SR(3), SR(6), SR(6), 0xE7FB);
            } else {
                dst.fillCircle(mx, my, SR(3), 0xE7FB);
            }
        }
        dst.setTextDatum(MC_DATUM);
        dst.setTextColor(COL_GOLD, 0x0861);
        dst.drawString("ROLEX", cx, cy - SR(26), 2);
        dst.setTextColor(0x9D33, 0x0861);
        dst.drawString("OYSTER", cx, cy + SR(24), 1);

        renderWatchHands(dst, cx, cy, valid ? ti.tm_hour : 16, valid ? ti.tm_min : 30,
                         valid ? ti.tm_sec : 0, 0xEFFD, 0xF7DF, COL_ERR, true);
    } else {
        dst.fillRoundRect(cx - r - SR(7), cy - SR(16), (r + SR(7)) * 2, SR(32), 7, 0x8C73);
        dst.fillRoundRect(cx - r, cy - r, r * 2, r * 2, SR(30), 0xAD57);
        const int16_t dr = r - SR(11);
        dst.fillRoundRect(cx - dr, cy - dr, dr * 2, dr * 2, SR(24), 0x1107);
        for (int16_t sy = cy - dr + SR(10); sy <= cy + dr - SR(8); sy += SR(8)) {
            dst.drawFastHLine(cx - dr + SR(10), sy, (dr - SR(10)) * 2, 0x196A);
        }
        for (int i = 0; i < 12; ++i) {
            const float a = i * 0.523599f - 1.570796f;
            const int16_t x1 = cx + static_cast<int16_t>(cosf(a) * (dr - SR(13)));
            const int16_t y1 = cy + static_cast<int16_t>(sinf(a) * (dr - SR(13)));
            const int16_t x2 = cx + static_cast<int16_t>(cosf(a) * (dr - SR(5)));
            const int16_t y2 = cy + static_cast<int16_t>(sinf(a) * (dr - SR(5)));
            if (i == 3) {
                dst.fillRect(x1 - SR(5), y1 - SR(5), SR(14), SR(11), 0xF7BE);
                dst.setTextDatum(MC_DATUM);
                dst.setTextColor(TFT_BLACK, 0xF7BE);
                char db[4]; snprintf(db, sizeof(db), "%d", valid ? ti.tm_mday : 25);
                dst.drawString(db, x1 + SR(2), y1, 1);
            } else {
                dst.drawWideLine(x1, y1, x2, y2, (i % 3 == 0) ? 3 : 2, 0xEF7D);
            }
        }
        dst.fillCircle(cx, cy + SR(32), SR(14), 0x08A4);
        dst.drawCircle(cx, cy + SR(32), SR(14), 0x9D16);
        dst.fillCircle(cx, cy + SR(30), SR(6), COL_GOLD);
        dst.fillCircle(cx + SR(3), cy + SR(28), SR(5), 0x08A4);

        dst.setTextDatum(MC_DATUM);
        dst.setTextColor(0xE73C, 0x1107);
        dst.drawString("PATEK PHILIPPE", cx, cy - SR(26), 1);
        dst.setTextColor(0x8D37, 0x1107);
        dst.drawString("GENEVE", cx, cy - SR(16), 1);

        renderWatchHands(dst, cx, cy, valid ? ti.tm_hour : 16, valid ? ti.tm_min : 30,
                         valid ? ti.tm_sec : 0, 0xEF7D, 0xF7DF, COL_ACCENT, false);
    }
}

void drawAnalogDial(TFT_eSPI& tft, bool isPatek, const struct tm& ti, bool valid) {
    const int16_t S = SR(168);
    const int16_t ox = SX(16);
    const int16_t oy = SY(32);

    TFT_eSprite spr(&tft);
    spr.setColorDepth(16);
    if (spr.createSprite(S, S)) {
        spr.fillSprite(COL_BG);
        drawDialContents(spr, S / 2, S / 2, isPatek, ti, valid);
        spr.pushSprite(ox, oy);
        spr.deleteSprite();
    } else {
        drawDialContents(tft, ox + S / 2, oy + S / 2, isPatek, ti, valid);
    }
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
constexpr uint16_t SKY_COL = 0x5CB3;

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
    {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110},
    {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110},
    {0b01110,0b10001,0b00001,0b00110,0b01000,0b10000,0b11111},
    {0b11110,0b00001,0b00001,0b01110,0b00001,0b00001,0b11110},
    {0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010},
    {0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110},
    {0b01110,0b10000,0b10000,0b11110,0b10001,0b10001,0b01110},
    {0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000},
    {0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110},
    {0b01110,0b10001,0b10001,0b01111,0b00001,0b00001,0b01110},
};

uint16_t marioPalColor(char c) {
    if (c == 'R') return 0xE103;
    if (c == 'B') return 0x81E2;
    if (c == 'S') return 0xFDF0;
    if (c == 'K') return 0x1082;
    if (c == 'O') return 0x22BC;
    if (c == 'Y') return 0xFE65;
    return 0;
}

template <typename Target>
void drawMarioSprite(Target& dst, const char* const grid[16], int16_t x, int16_t y) {
    constexpr int16_t sc = SR(2);
    for (int r = 0; r < 16; ++r) {
        const char* row = grid[r];
        for (int c = 0; c < 16; ++c) {
            const uint16_t col = marioPalColor(row[c]);
            if (col != 0) dst.fillRect(x + c * sc, y + r * sc, sc, sc, col);
        }
    }
}

template <typename Target>
void drawSmbBlock(Target& dst, int16_t x, int16_t y, int digit) {
    const int16_t sz = SR(42);
    dst.fillRect(x, y, sz, sz, 0x1881);
    dst.fillRect(x + 2, y + 2, sz - 4, sz - 4, 0xF543);
    dst.fillRect(x + 2, y + 2, sz - 4, 2, 0xFF11);
    dst.fillRect(x + 2, y + 2, 2, sz - 4, 0xFF11);
    dst.fillRect(x + 3, y + sz - 4, sz - 5, 2, 0xA2C1);
    dst.fillRect(x + sz - 4, y + 3, 2, sz - 5, 0xA2C1);
    dst.fillRect(x + 6, y + 6, 3, 3, 0x81E1);
    dst.fillRect(x + sz - 9, y + 6, 3, 3, 0x81E1);
    dst.fillRect(x + 6, y + sz - 9, 3, 3, 0x81E1);
    dst.fillRect(x + sz - 9, y + sz - 9, 3, 3, 0x81E1);

    const int16_t ps = SR(3);
    const int16_t ox = x + (sz - 5 * ps) / 2;
    const int16_t oy = y + (sz - 7 * ps) / 2;
    for (int r = 0; r < 7; ++r) {
        uint8_t bits = pgm_read_byte(&PIX_5X7[digit % 10][r]);
        for (int c = 0; c < 5; ++c) {
            if (bits & (1 << (4 - c))) {
                dst.fillRect(ox + c * ps + 2, oy + r * ps + 2, ps, ps, 0x4900);
                dst.fillRect(ox + c * ps, oy + r * ps, ps, ps, 0xFFFC);
            }
        }
    }
}

void drawMarioStaticBg(TFT_eSPI& tft, const struct tm& ti, bool valid,
                       const DeviceConfig& cfg, const WeatherData& wx) {
    tft.fillRect(0, TOP_BAR_H + 1, SCREEN_W, NAV_BAR_Y - TOP_BAR_H - SY(4), SKY_COL);
    for (int16_t cx : {SX(46), SX(274)}) {
        int16_t cy = (cx < SX(100)) ? SY(124) : SY(46);
        tft.fillRoundRect(cx - SX(22), cy - SY(5), SX(44), SY(12), 5, TFT_WHITE);
        tft.fillCircle(cx, cy - SY(4), SR(10), TFT_WHITE);
    }
    tft.fillCircle(SX(38), SY(176), SR(42), 0x1D46);
    tft.fillCircle(SX(26), SY(156), SR(4), 0x0B83);
    tft.fillCircle(SX(50), SY(162), SR(3), 0x0B83);

    tft.fillRect(SX(262), SY(146), SX(40), SY(30), 0x2585);
    tft.drawRect(SX(262), SY(146), SX(40), SY(30), 0x0A42);
    tft.fillRect(SX(268), SY(147), SX(6), SY(28), 0x7F4D);
    tft.fillRect(SX(292), SY(147), SX(7), SY(28), 0x13A3);
    tft.fillRect(SX(258), SY(132), SX(48), SY(15), 0x2E67);
    tft.drawRect(SX(258), SY(132), SX(48), SY(15), 0x0A42);
    tft.fillRect(SX(265), SY(134), SX(8), SY(11), 0x8FAD);

    const int16_t bs = SY(16);
    const int16_t rows[2] = {SY(176), static_cast<int16_t>(SY(176) + bs)};
    for (int16_t ry : rows) {
        for (int16_t gx = 0; gx < SCREEN_W; gx += bs) {
            tft.fillRect(gx, ry, bs - 1, bs - 1, 0xCA61);
            tft.drawRect(gx, ry, bs, bs, 0x1881);
            tft.drawFastHLine(gx + 1, ry + 1, bs - 2, 0xF50E);
            tft.drawFastVLine(gx + bs / 2, ry + 2, bs / 2 - 2, 0x1881);
            tft.drawFastHLine(gx + 1, ry + bs / 2, bs - 2, 0x1881);
        }
    }

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    if (valid) {
        char dbuf[20];
        snprintf(dbuf, sizeof(dbuf), "%d %s %s", ti.tm_mday,
                 TR_MONTHS_SHORT[ti.tm_mon % 12], TR_DAYS_SHORT[ti.tm_wday % 7]);
        tft.setTextColor(TFT_WHITE, SKY_COL);
        tft.drawString(dbuf, SX(12), SY(32));
    }
    if (wx.valid) {
        tft.fillCircle(SX(104), SY(38), SR(6), COL_ACCENT_2);
        char wbuf[16];
        snprintf(wbuf, sizeof(wbuf), "%.0f°C", wx.tempC);
        tft.setTextColor(0xFF92, SKY_COL);
        tft.drawString(wbuf, SX(115), SY(32));
    }
    tft.unloadFont();
    (void)cfg;
}

void animateMarioJumpHit(TFT_eSPI& tft, int oldDigit, int newDigit) {
    const int16_t sx = SX(182), sy = SY(32), sw = SX(62), sh = SY(144);
    TFT_eSprite spr(&tft);
    spr.setColorDepth(16);
    if (!spr.createSprite(sw, sh)) {
        drawSmbBlock(tft, SX(188), SY(68), newDigit);
        drawMarioSprite(tft, MARIO_STAND, SX(193), SY(144));
        return;
    }

    constexpr int FRAMES = 12;
    for (int f = 0; f <= FRAMES; ++f) {
        spr.fillSprite(SKY_COL);
        float t = static_cast<float>(f) / FRAMES;
        float arc = 4.0f * t * (1.0f - t);
        int16_t marioY = SY(144) - static_cast<int16_t>(arc * SY(42));
        bool hitPhase = (f >= FRAMES / 2);
        int16_t blockBounce = (f == 6 || f == 7) ? -SY(10) : ((f == 5 || f == 8) ? -SY(5) : 0);

        if (f >= 6 && f <= 11) {
            int16_t coinY = (SY(58) - sy) - (f - 5) * SY(4);
            if (coinY >= 4) {
                spr.fillEllipse(SX(209) - sx, coinY, SR(5), SR(8), 0xFEA6);
                spr.drawEllipse(SX(209) - sx, coinY, SR(5), SR(8), 0x7A21);
                spr.drawFastVLine(SX(209) - sx, coinY - SR(4), SR(8), 0xFFFC);
            }
        }

        drawSmbBlock(spr, SX(188) - sx, (SY(68) - sy) + blockBounce, hitPhase ? newDigit : oldDigit);
        drawMarioSprite(spr, (arc > 0.2f) ? MARIO_JUMP : MARIO_STAND,
                        (arc > 0.2f ? SX(186) : SX(193)) - sx, marioY - sy);
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
    const int16_t xs[4] = {SX(24), SX(72), SX(140), SX(188)};

    if (forceFull) {
        tft.fillScreen(COL_BG);
        drawTopBar(tft, "Mario Saat", "Tema", netGetRssi(), wifiOk, fetching);
        drawMarioStaticBg(tft, ti, valid, cfg, wx);
        for (int i = 0; i < 4; ++i) {
            drawSmbBlock(tft, xs[i], SY(68), d[i]);
            g_prevDigits[i] = d[i];
        }
        drawMarioSprite(tft, MARIO_STAND, SX(193), SY(144));
        tft.fillRect(SX(124), SY(80), SR(5), SR(5), TFT_WHITE);
        tft.fillRect(SX(124), SY(96), SR(5), SR(5), TFT_WHITE);
        drawBottomNav(tft, 0);
        g_prevSec = sec; g_prevDay = dayKey; g_prevWxMs = wx.updatedAtMs;
        return;
    }

    for (int i = 0; i < 3; ++i) {
        if (d[i] != g_prevDigits[i]) {
            g_prevDigits[i] = d[i];
            drawSmbBlock(tft, xs[i], SY(68), d[i]);
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
        tft.fillRect(SX(124), SY(80), SR(5), SR(5), cc);
        tft.fillRect(SX(124), SY(96), SR(5), SR(5), cc);
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
    {"III", "S.L", "S.L", "O.J", "OOJ"},
    {".I.", "II.", ".I.", ".I.", "LLL"},
    {"III", "..L", "TTT", "Z..", "OOO"},
    {"LLL", "..L", "SSS", "..J", "JJJ"},
    {"S.L", "S.L", "TTT", "..J", "..J"},
    {"ZZZ", "Z..", "TTT", "..L", "OOO"},
    {"TTT", "T..", "ZZO", "ZOO", "JJO"},
    {"III", "..L", "..L", ".J.", ".J."},
    {"TTT", "S.L", "OOO", "Z.J", "ZZJ"},
    {"TTT", "S.L", "OOO", "..J", "JJJ"}
};

uint16_t tetroColor(char k) {
    if (k == 'I') return 0x26FF;
    if (k == 'O') return 0xFE85;
    if (k == 'T') return 0xB27E;
    if (k == 'S') return 0x470C;
    if (k == 'Z') return 0xF229;
    if (k == 'J') return 0x3C1F;
    if (k == 'L') return 0xFC84;
    return 0;
}

template <typename Target>
void drawTetroCell(Target& dst, int16_t x, int16_t y, int16_t bs, uint16_t c) {
    dst.fillRect(x, y, bs - 1, bs - 1, c);
    dst.drawFastHLine(x, y, bs - 1, TFT_WHITE);
    dst.drawFastVLine(x, y, bs - 1, TFT_WHITE);
}

void drawTetrisColumnStatic(TFT_eSPI& tft, int colIdx, int digit) {
    const int16_t xs[4] = {SX(20), SX(66), SX(124), SX(170)};
    const int16_t bs = SY(12);
    const int16_t dy = SY(92);
    const int16_t x0 = xs[colIdx];

    tft.fillRect(x0, SY(30), 3 * bs, SY(144), 0x0883);
    for (int16_t gx = x0; gx <= x0 + 3 * bs; gx += bs) {
        tft.drawFastVLine(gx, SY(30), SY(144), 0x10E5);
    }
    for (int16_t gy = SY(30); gy < SY(174); gy += bs) {
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
    const int16_t xs[4] = {SX(20), SX(66), SX(124), SX(170)};
    const int16_t bs = SY(12);
    const int16_t dy = SY(92);
    const int16_t x0 = xs[colIdx];
    const int16_t cw = 3 * bs;
    const int16_t ch = SY(144);

    TFT_eSprite spr(&tft);
    spr.setColorDepth(16);
    if (!spr.createSprite(cw, ch)) {
        drawTetrisColumnStatic(tft, colIdx, newDigit);
        return;
    }

    for (int settleRow = 4; settleRow >= 0; --settleRow) {
        const int16_t targetY = (dy - SY(30)) + settleRow * bs;
        for (int16_t dropY = 4; dropY <= targetY; dropY += bs) {
            spr.fillSprite(0x0883);
            for (int16_t gx = 0; gx < cw; gx += bs) spr.drawFastVLine(gx, 0, ch, 0x10E5);
            for (int16_t gy = 0; gy < ch; gy += bs) spr.drawFastHLine(0, gy, cw, 0x10E5);

            for (int r = settleRow + 1; r < 5; ++r) {
                const char* row = TETRIS_MAP[newDigit % 10][r];
                for (int c = 0; c < 3; ++c) {
                    if (row[c] != '.') {
                        drawTetroCell(spr, c * bs, (dy - SY(30)) + r * bs, bs, tetroColor(row[c]));
                    }
                }
            }
            const char* curRow = TETRIS_MAP[newDigit % 10][settleRow];
            for (int c = 0; c < 3; ++c) {
                if (curRow[c] != '.') {
                    drawTetroCell(spr, c * bs, dropY, bs, tetroColor(curRow[c]));
                }
            }
            spr.pushSprite(x0, SY(30));
            delay(18);
        }
    }
    spr.deleteSprite();
    drawTetrisColumnStatic(tft, colIdx, newDigit);
}

void drawTetrisSidePanel(TFT_eSPI& tft, const struct tm& ti, bool valid, const WeatherData& wx) {
    const int16_t sx = SX(228);
    const int16_t sw = SCREEN_W - sx - SX(10);
    tft.fillRoundRect(sx, SY(28), sw, SY(177), 8, COL_SURFACE);
    tft.drawRoundRect(sx, SY(28), sw, SY(177), 8, COL_BORDER);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString("SONRAKİ", sx + sw / 2, SY(42));
    tft.unloadFont();

    const int16_t ps = SR(11);
    for (auto p : {Pt{26, 60}, Pt{26, 72}, Pt{26, 84}, Pt{38, 84}}) {
        drawTetroCell(tft, sx + SX(p.x), SY(p.y), ps, COL_ACCENT_2);
    }
    tft.drawFastHLine(sx + SX(10), SY(110), sw - SX(20), COL_BORDER);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    if (valid) {
        char dbuf[16];
        snprintf(dbuf, sizeof(dbuf), "%d %s", ti.tm_mday, TR_MONTHS_SHORT[ti.tm_mon % 12]);
        tft.setTextColor(COL_TEXT, COL_SURFACE);
        tft.drawString(dbuf, sx + sw / 2, SY(128));
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString(TR_DAYS_SHORT[ti.tm_wday % 7], sx + sw / 2, SY(148));
    }
    if (wx.valid) {
        char wbuf[12];
        snprintf(wbuf, sizeof(wbuf), "%.0f°C", wx.tempC);
        tft.setTextColor(COL_CYAN, COL_SURFACE);
        tft.drawString(wbuf, sx + sw / 2, SY(178));
    }
    tft.unloadFont();
}

void tickTetris(TFT_eSPI& tft, const WeatherData& wx, bool fetching, bool forceFull) {
    struct tm ti{}; bool valid = false; int d[4], sec = 0, dayKey = 0;
    getClockVals(ti, valid, d, sec, dayKey);
    const bool wifiOk = netIsWifiConnected();
    const int16_t bs = SY(12);

    if (forceFull) {
        tft.fillScreen(COL_BG);
        drawTopBar(tft, "Tetris Saat", "Tema", netGetRssi(), wifiOk, fetching);
        tft.fillRect(SX(10), SY(28), SX(212), SY(177), 0x0883);
        tft.drawRect(SX(10), SY(28), SX(212), SY(177), 0x3A2C);
        for (int16_t gx = SX(22); gx < SX(220); gx += bs) tft.drawFastVLine(gx, SY(30), SY(173), 0x10E5);
        for (int16_t gy = SY(40); gy < SY(204); gy += bs) tft.drawFastHLine(SX(12), gy, SX(208), 0x10E5);

        const char keys[6] = {'Z', 'I', 'O', 'S', 'T', 'L'};
        for (int i = 0; i < 17; ++i) {
            drawTetroCell(tft, SX(14) + i * bs, SY(191), bs, tetroColor(keys[i % 6]));
            if (i % 4 != 3) drawTetroCell(tft, SX(14) + i * bs, SY(179), bs, tetroColor(keys[(i + 2) % 6]));
        }

        for (int i = 0; i < 4; ++i) {
            drawTetrisColumnStatic(tft, i, d[i]);
            g_prevDigits[i] = d[i];
        }
        drawTetroCell(tft, SX(108), SY(106), SR(10), COL_GOLD);
        drawTetroCell(tft, SX(108), SY(130), SR(10), COL_GOLD);
        drawTetrisSidePanel(tft, ti, valid, wx);
        drawBottomNav(tft, 0);
        g_prevSec = sec; g_prevDay = dayKey; g_prevWxMs = wx.updatedAtMs;
        return;
    }

    for (int i = 0; i < 4; ++i) {
        if (d[i] != g_prevDigits[i]) {
            g_prevDigits[i] = d[i];
            animateTetrisDigitDrop(tft, i, d[i]);
        }
    }
    if (sec != g_prevSec) {
        g_prevSec = sec;
        const uint16_t cc = (sec % 2 == 0) ? COL_GOLD : 0x2104;
        drawTetroCell(tft, SX(108), SY(106), SR(10), cc);
        drawTetroCell(tft, SX(108), SY(130), SR(10), cc);
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
