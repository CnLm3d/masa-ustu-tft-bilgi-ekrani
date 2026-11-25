#include "ui_world.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "fonts/SegoeUIBold42Temp.h"
#include "net_services.h"
#include "ui_common.h"
#include <time.h>

void uiWorldDraw(TFT_eSPI& tft, const DeviceConfig& cfg, bool fetching) {
    tft.fillScreen(COL_BG);
    drawTopBar(tft, "Dünya Saatleri", "Ayar", netGetRssi(), netIsWifiConnected(), fetching);

    time_t nowUtc = time(nullptr);
    const bool timeValid = (nowUtc > 1700000000);

    struct tm localTm{};
    int localYday = -1;
    if (timeValid && getLocalTime(&localTm, 2)) {
        localYday = localTm.tm_yday;
    }

    const int16_t cw = 146;
    const int16_t ch = 82;
    const int16_t gap = 8;
    const int16_t sx = 10;
    const int16_t sy = 32;

    for (int i = 0; i < 4; ++i) {
        const int col = i % 2;
        const int row = i / 2;
        const int16_t x = sx + col * (cw + gap);
        const int16_t y = sy + row * (ch + gap);

        tft.fillRoundRect(x, y, cw, ch, 9, COL_SURFACE);
        tft.drawRoundRect(x, y, cw, ch, 9, COL_BORDER);

        const WorldCity& wc = cfg.worldCities[i];
        time_t cityTime = nowUtc + static_cast<time_t>(wc.utcOffsetMin) * 60;
        struct tm ct{};
        gmtime_r(&cityTime, &ct);

        const bool isDay = timeValid ? (ct.tm_hour >= 6 && ct.tm_hour < 19) : true;
        tft.fillCircle(x + cw - 16, y + 15, 5, isDay ? COL_ACCENT_2 : 0x9DDF);

        tft.loadFont(SegoeUIBold18TR);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(COL_ACCENT, COL_SURFACE);
        tft.drawString(wc.name, x + 10, y + 6);
        tft.unloadFont();

        char tbuf[12];
        if (timeValid) snprintf(tbuf, sizeof(tbuf), "%02d:%02d", ct.tm_hour, ct.tm_min);
        else strlcpy(tbuf, "--:--", sizeof(tbuf));

        tft.loadFont(SegoeUIBold42Temp);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(COL_TEXT, COL_SURFACE);
        // SegoeUIBold42Temp has digits + '-' '.' 'C'; draw ':' manually if needed or use SegoeUIBold18TR
        tft.unloadFont();

        // Draw crisp 28-30px-feel big time using GLCD/FONT4 or SegoeUIBold42Temp digits + colon
        char hhBuf[4], mmBuf[4];
        if (timeValid) {
            snprintf(hhBuf, sizeof(hhBuf), "%02d", ct.tm_hour);
            snprintf(mmBuf, sizeof(mmBuf), "%02d", ct.tm_min);
        } else {
            strlcpy(hhBuf, "--", sizeof(hhBuf));
            strlcpy(mmBuf, "--", sizeof(mmBuf));
        }
        tft.loadFont(SegoeUIBold42Temp);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(COL_TEXT, COL_SURFACE);
        tft.drawString(hhBuf, x + 10, y + 26);
        tft.fillCircle(x + 62, y + 40, 2, COL_ACCENT);
        tft.fillCircle(x + 62, y + 51, 2, COL_ACCENT);
        tft.drawString(mmBuf, x + 68, y + 26);
        tft.unloadFont();

        // Offset & day label
        const int offH = wc.utcOffsetMin / 60;
        const int offM = abs(wc.utcOffsetMin % 60);
        const char* dayRel = "Bugün";
        if (timeValid && localYday >= 0) {
            if (ct.tm_yday > localYday || (localYday > 350 && ct.tm_yday < 5)) dayRel = "Yarın";
            else if (ct.tm_yday < localYday) dayRel = "Dün";
        }
        char meta[28];
        if (offM == 0) snprintf(meta, sizeof(meta), "%s • UTC%+d", dayRel, offH);
        else snprintf(meta, sizeof(meta), "%s • UTC%+d:%02d", dayRel, offH, offM);

        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(BL_DATUM);
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString(meta, x + 10, y + ch - 5);
        tft.unloadFont();
    }

    drawBottomNav(tft, 3);
}
