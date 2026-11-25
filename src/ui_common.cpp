#include "ui_common.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"

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

void drawTopBar(TFT_eSPI& tft, PageId page, const char* title, int rssi, bool wifiOk, bool fetching) {
    tft.fillRect(0, 0, SCREEN_W, 26, COL_SURFACE);
    tft.drawFastHLine(0, 26, SCREEN_W, COL_BORDER);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(title, 12, 13);
    tft.unloadFont();

    // Page indicator pills centered near right
    const int16_t dotsStartX = 216;
    for (uint8_t i = 0; i < static_cast<uint8_t>(PageId::COUNT); ++i) {
        const bool active = (i == static_cast<uint8_t>(page));
        const int16_t dx = dotsStartX + i * 16;
        if (active) {
            tft.fillRoundRect(dx - 5, 10, 12, 6, 3, COL_ACCENT);
        } else {
            tft.fillCircle(dx, 13, 3, COL_BORDER);
        }
    }

    // Sync dot
    tft.fillCircle(274, 13, 3, fetching ? COL_ACCENT_2 : COL_BORDER);

    // Wi-Fi 4-bar signal meter
    int bars = 0;
    if (wifiOk) {
        if (rssi > -58) bars = 4;
        else if (rssi > -70) bars = 3;
        else if (rssi > -82) bars = 2;
        else bars = 1;
    }
    for (int b = 0; b < 4; ++b) {
        const int16_t bx = 286 + b * 6;
        const int16_t bh = 4 + b * 3;
        const int16_t by = 20 - bh;
        uint16_t c = (b < bars) ? COL_OK : COL_BORDER;
        if (!wifiOk) c = COL_ERR;
        tft.fillRect(bx, by, 4, bh, c);
    }
}

void drawBottomHint(TFT_eSPI& tft, const char* leftHint, const char* rightHint) {
    tft.fillRect(0, SCREEN_H - 20, SCREEN_W, 20, COL_SURFACE);
    tft.drawFastHLine(0, SCREEN_H - 20, SCREEN_W, COL_BORDER);

    tft.loadFont(SegoeUI14TR);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(leftHint, 10, SCREEN_H - 10);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(rightHint, SCREEN_W - 10, SCREEN_H - 10);
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
    const uint16_t sunCol = 0xFEA0;   // warm yellow
    const uint16_t cloudCol = 0xDEFB; // soft light grey
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

    // Thunderstorm (>= 95)
    drawCloudShape(tft, cx, cy - r * 18 / 100, r * 82 / 100, 0x9CF3);
    tft.fillTriangle(cx + 2, cy + r * 14 / 100,
                     cx - 7, cy + r * 42 / 100,
                     cx + 1, cy + r * 42 / 100, sunCol);
    tft.fillTriangle(cx + 8, cy + r * 36 / 100,
                     cx - 2, cy + r * 66 / 100,
                     cx + 1, cy + r * 36 / 100, sunCol);
}
