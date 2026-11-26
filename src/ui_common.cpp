#include "ui_common.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "fonts/SegoeUIBold88Digits.h"
#include "net_services.h"
#include <time.h>

namespace {
size_t nextUtf8Boundary(const String& s, size_t idx) {
    if (idx >= s.length()) return s.length();
    size_t i = idx + 1;
    while (i < s.length() && (static_cast<uint8_t>(s[i]) & 0xC0) == 0x80) {
        ++i;
    }
    return i;
}

size_t prevUtf8Start(const String& s, size_t idx) {
    if (idx == 0) return 0;
    size_t i = idx - 1;
    while (i > 0 && (static_cast<uint8_t>(s[i]) & 0xC0) == 0x80) {
        --i;
    }
    return i;
}

void drawCloudShape(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, uint16_t col) {
    int16_t r1 = r * 42 / 100;
    int16_t r2 = r * 54 / 100;
    int16_t r3 = r * 38 / 100;
    tft.fillCircle(cx - r * 35 / 100, cy + r * 12 / 100, r1, col);
    tft.fillCircle(cx, cy - r * 8 / 100, r2, col);
    tft.fillCircle(cx + r * 38 / 100, cy + r * 15 / 100, r3, col);
    tft.fillRoundRect(cx - r * 62 / 100, cy + r * 10 / 100,
                      r * 124 / 100, r * 42 / 100, r * 18 / 100, col);
}
} // namespace

void drawTopBar(TFT_eSPI& tft, const char* title, const char* rightPill,
                int rssi, bool wifiOk, bool fetching) {
    tft.fillRect(0, 0, SCREEN_W, TOP_BAR_H, COL_SURFACE);
    tft.drawFastHLine(0, TOP_BAR_H, SCREEN_W, COL_BORDER);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(title, SX(10), SY(12));

    if (rightPill && rightPill[0] != '\0') {
        tft.fillRoundRect(SX(216), SY(4), SX(48), SY(16), 5, COL_BG);
        tft.drawRoundRect(SX(216), SY(4), SX(48), SY(16), 5, COL_BORDER);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.drawString(rightPill, SX(240), SY(12));
    }
    tft.unloadFont();

    tft.fillCircle(SX(274), SY(12), SR(3), fetching ? COL_ACCENT_2 : COL_BORDER);

    int bars = 0;
    if (wifiOk) {
        if (rssi > -58) bars = 4;
        else if (rssi > -70) bars = 3;
        else if (rssi > -82) bars = 2;
        else bars = 1;
    }
    for (int b = 0; b < 4; ++b) {
        const int16_t bx = SX(285 + b * 6);
        const int16_t bh = SY(4 + b * 3);
        const int16_t by = SY(19) - bh;
        uint16_t c = (b < bars) ? COL_OK : COL_BORDER;
        if (!wifiOk) c = COL_ERR;
        tft.fillRect(bx, by, SX(4), bh, c);
    }
}

void drawBottomNav(TFT_eSPI& tft, uint8_t activeTab) {
    tft.fillRect(0, NAV_BAR_Y - SY(3), SCREEN_W, SCREEN_H - NAV_BAR_Y + SY(3), COL_BG);

    const char* const labels[4] = {"Saat", "Müzik", "Piyasa", "Menü"};
    const int16_t xs[4] = {SX(4), SX(83), SX(162), SX(241)};
    const int16_t tabW = SX(75);
    const int16_t tabH = NAV_BAR_H;

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    for (uint8_t i = 0; i < 4; ++i) {
        const bool active = (i == activeTab);
        const uint16_t fillCol = active ? COL_ACCENT : COL_SURFACE;
        const uint16_t borderCol = active ? COL_ACCENT : COL_BORDER;
        const uint16_t textCol = active ? COL_TEXT : COL_MUTED;

        tft.fillRoundRect(xs[i], NAV_BAR_Y, tabW, tabH, 6, fillCol);
        tft.drawRoundRect(xs[i], NAV_BAR_Y, tabW, tabH, 6, borderCol);
        tft.setTextColor(textCol, fillCol);
        tft.drawString(labels[i], xs[i] + tabW / 2, NAV_BAR_Y + tabH / 2);
    }
    tft.unloadFont();
}

void uiMenuDraw(TFT_eSPI& tft) {
    tft.fillScreen(COL_BG);
    drawTopBar(tft, "Ana Menü", "Ayar", netGetRssi(), netIsWifiConnected(), netIsFetching());

    const char* const labels[8] = {
        "Saat", "Hava", "Piyasa", "Dünya",
        "Alarm", "Haber", "Odak", "Müzik"
    };
    const char* const badges[8] = {"S", "H", "₺", "D", "A", "G", "O", "♫"};
    const uint16_t cols[8] = {
        COL_ACCENT, COL_CYAN, COL_GOLD, COL_PURPLE,
        COL_ERR, COL_ACCENT_2, COL_OK, COL_SPOTIFY
    };

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    for (int i = 0; i < 8; ++i) {
        const Rect& r = RECT_MENU_TILES[i];
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 9, COL_SURFACE);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 9, cols[i]);

        const int16_t icx = r.x + r.w / 2;
        const int16_t icy = r.y + SY(28);
        tft.drawCircle(icx, icy, SR(15), cols[i]);
        tft.drawCircle(icx, icy, SR(14), cols[i]);
        tft.setTextColor(cols[i], COL_SURFACE);
        tft.drawString(i == 7 ? "M" : badges[i], icx, icy);

        tft.setTextColor(COL_TEXT, COL_SURFACE);
        tft.drawString(labels[i], icx, r.y + SY(60));
    }
    tft.unloadFont();

    drawBottomNav(tft, 3);
}

void uiSettingsDraw(TFT_eSPI& tft, const DeviceConfig& cfg) {
    tft.fillScreen(COL_BG);
    drawTopBar(tft, "Cihaz Ayarları", "", netGetRssi(), netIsWifiConnected(), false);

    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(MC_DATUM);

    tft.fillRoundRect(RECT_SET_WIFI.x, RECT_SET_WIFI.y, RECT_SET_WIFI.w, RECT_SET_WIFI.h, 9, COL_SURFACE);
    tft.drawRoundRect(RECT_SET_WIFI.x, RECT_SET_WIFI.y, RECT_SET_WIFI.w, RECT_SET_WIFI.h, 9, COL_ACCENT);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString("Wi-Fi & Şehir Kurulumu", SCREEN_W / 2, RECT_SET_WIFI.y + RECT_SET_WIFI.h / 2);

    tft.fillRoundRect(RECT_SET_CAL.x, RECT_SET_CAL.y, RECT_SET_CAL.w, RECT_SET_CAL.h, 9, COL_SURFACE);
    tft.drawRoundRect(RECT_SET_CAL.x, RECT_SET_CAL.y, RECT_SET_CAL.w, RECT_SET_CAL.h, 9, COL_ACCENT_2);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString("Dokunmatik Kalibrasyon", SCREEN_W / 2, RECT_SET_CAL.y + RECT_SET_CAL.h / 2);

    tft.fillRoundRect(RECT_SET_BACK.x, RECT_SET_BACK.y, RECT_SET_BACK.w, RECT_SET_BACK.h, 9, COL_BG);
    tft.drawRoundRect(RECT_SET_BACK.x, RECT_SET_BACK.y, RECT_SET_BACK.w, RECT_SET_BACK.h, 9, COL_BORDER);
    tft.setTextColor(COL_MUTED, COL_BG);
    tft.drawString("Geri Dön", SCREEN_W / 2, RECT_SET_BACK.y + RECT_SET_BACK.h / 2);
    tft.unloadFont();

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(COL_MUTED, COL_BG);
    char info[96];
    snprintf(info, sizeof(info), "Ağ: %s • Şehir: %s", cfg.ssid, cfg.city);
    tft.drawString(info, SCREEN_W / 2, SCREEN_H - SY(10));
    tft.unloadFont();
}

void uiNightDraw(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx) {
    tft.fillScreen(TFT_BLACK);

    struct tm ti{};
    const bool ok = getLocalTime(&ti, 5);
    char timeBuf[8];
    if (ok) snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
    else strlcpy(timeBuf, "--:--", sizeof(timeBuf));

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(0x4081, TFT_BLACK);
    tft.drawString("GECE MODU • DOKUN -> UYAN", SCREEN_W / 2, SY(16));
    tft.unloadFont();

    tft.loadFont(SegoeUIBold88Digits);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_NIGHT_RED, TFT_BLACK);
    tft.drawString(timeBuf, SCREEN_W / 2, SY(108));
    tft.unloadFont();

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0x6102, TFT_BLACK);
    char sub[64];
    if (ok && wx.valid) {
        snprintf(sub, sizeof(sub), "%d %s %s • %.0f°C",
                 ti.tm_mday, TR_MONTHS_SHORT[ti.tm_mon % 12], TR_DAYS_LONG[ti.tm_wday % 7], wx.tempC);
    } else if (ok) {
        snprintf(sub, sizeof(sub), "%d %s %s",
                 ti.tm_mday, TR_MONTHS_SHORT[ti.tm_mon % 12], TR_DAYS_LONG[ti.tm_wday % 7]);
    } else {
        strlcpy(sub, cfg.city, sizeof(sub));
    }
    tft.drawString(sub, SCREEN_W / 2, SY(174));

    for (int i = 0; i < 3; ++i) {
        if (cfg.alarms[i].enabled) {
            char abuf[32];
            snprintf(abuf, sizeof(abuf), "Alarm: %02u:%02u", cfg.alarms[i].hour, cfg.alarms[i].minute);
            tft.setTextColor(0x7142, TFT_BLACK);
            tft.drawString(abuf, SCREEN_W / 2, SY(202));
            break;
        }
    }
    tft.unloadFont();
}

uint8_t drawWrappedUtf8(TFT_eSPI& tft, const String& text, int16_t x, int16_t y,
                        int16_t maxW, int16_t lineH, uint8_t maxLines,
                        uint16_t fg, uint16_t bg) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(fg, bg);

    uint8_t lineCount = 0;
    size_t pos = 0;
    const size_t len = text.length();

    while (pos < len && lineCount < maxLines) {
        while (pos < len && text[pos] == ' ') ++pos;
        if (pos >= len) break;

        size_t end = pos;
        size_t lastSpace = pos;
        bool hasSpace = false;

        while (end < len) {
            if (text[end] == '\n') {
                lastSpace = end;
                hasSpace = true;
                break;
            }
            size_t next = nextUtf8Boundary(text, end);
            String cand = text.substring(pos, next);
            if (tft.textWidth(cand) > maxW) {
                break;
            }
            if (text[end] == ' ') {
                lastSpace = end;
                hasSpace = true;
            }
            end = next;
        }

        size_t lineEnd = end;
        if (end < len && text[end] != '\n' && hasSpace && lastSpace > pos) {
            lineEnd = lastSpace;
        } else if (lineEnd == pos) {
            lineEnd = nextUtf8Boundary(text, pos);
        }

        String line = text.substring(pos, lineEnd);
        line.trim();

        const bool isLastLine = (lineCount == maxLines - 1);
        size_t nextPos = lineEnd;
        while (nextPos < len && (text[nextPos] == ' ' || text[nextPos] == '\n')) ++nextPos;

        if (isLastLine && nextPos < len) {
            while (line.length() > 0 && tft.textWidth(line + "...") > maxW) {
                size_t cut = prevUtf8Start(line, line.length());
                line = line.substring(0, cut);
                line.trim();
            }
            line += "...";
        }

        tft.drawString(line, x, y + lineCount * lineH);
        ++lineCount;
        pos = nextPos;
    }
    return lineCount;
}

void drawWeatherIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, int wmoCode, bool isDay) {
    const uint16_t sunCol = 0xFEA0;
    const uint16_t cloudCol = 0xDEFB;
    const uint16_t rainCol = COL_ACCENT;

    if (wmoCode == 0) {
        if (isDay) {
            tft.fillCircle(cx, cy, r * 48 / 100, sunCol);
            for (int a = 0; a < 8; ++a) {
                const float rad = a * 0.785398f;
                int16_t x1 = cx + static_cast<int16_t>(cosf(rad) * r * 64 / 100);
                int16_t y1 = cy + static_cast<int16_t>(sinf(rad) * r * 64 / 100);
                int16_t x2 = cx + static_cast<int16_t>(cosf(rad) * r * 88 / 100);
                int16_t y2 = cy + static_cast<int16_t>(sinf(rad) * r * 88 / 100);
                tft.drawLine(x1, y1, x2, y2, sunCol);
            }
        } else {
            tft.fillCircle(cx, cy, r * 48 / 100, 0xFFE8);
            tft.fillCircle(cx + r * 22 / 100, cy - r * 14 / 100, r * 40 / 100, COL_SURFACE);
        }
        return;
    }

    if (wmoCode == 1 || wmoCode == 2) {
        if (isDay) {
            tft.fillCircle(cx - r * 28 / 100, cy - r * 26 / 100, r * 30 / 100, sunCol);
        } else {
            tft.fillCircle(cx - r * 28 / 100, cy - r * 26 / 100, r * 26 / 100, 0xFFE8);
        }
        drawCloudShape(tft, cx + r * 8 / 100, cy + r * 8 / 100, r * 78 / 100, cloudCol);
        return;
    }

    if (wmoCode == 3) {
        drawCloudShape(tft, cx, cy, r * 90 / 100, cloudCol);
        return;
    }

    if (wmoCode == 45 || wmoCode == 48) {
        for (int i = -1; i <= 1; ++i) {
            int16_t ly = cy + i * (r * 28 / 100);
            tft.fillRoundRect(cx - r * 65 / 100, ly - 2, r * 130 / 100, 4, 2, COL_MUTED);
        }
        return;
    }

    if ((wmoCode >= 51 && wmoCode <= 67) || (wmoCode >= 80 && wmoCode <= 82)) {
        drawCloudShape(tft, cx, cy - r * 18 / 100, r * 82 / 100, cloudCol);
        for (int i = -1; i <= 1; ++i) {
            int16_t rx = cx + i * (r * 28 / 100);
            tft.drawLine(rx, cy + r * 26 / 100, rx - 4, cy + r * 58 / 100, rainCol);
            tft.drawLine(rx + 1, cy + r * 26 / 100, rx - 3, cy + r * 58 / 100, rainCol);
        }
        return;
    }

    if ((wmoCode >= 71 && wmoCode <= 77) || wmoCode == 85 || wmoCode == 86) {
        drawCloudShape(tft, cx, cy - r * 18 / 100, r * 82 / 100, cloudCol);
        for (int i = -1; i <= 1; ++i) {
            int16_t sx = cx + i * (r * 30 / 100);
            int16_t sy = cy + r * 44 / 100;
            tft.fillCircle(sx, sy, 3, COL_TEXT);
        }
        return;
    }

    drawCloudShape(tft, cx, cy - r * 18 / 100, r * 82 / 100, 0x9CF3);
    tft.fillTriangle(cx + 2, cy + r * 14 / 100,
                     cx - 7, cy + r * 42 / 100,
                     cx + 1, cy + r * 42 / 100, sunCol);
    tft.fillTriangle(cx + 8, cy + r * 36 / 100,
                     cx - 2, cy + r * 66 / 100,
                     cx + 1, cy + r * 36 / 100, sunCol);
}
