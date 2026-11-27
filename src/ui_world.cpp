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

    const int16_t cw = SX(146);
    const int16_t ch = SY(82);
    const int16_t gapX = SX(8);
    const int16_t gapY = SY(8);
    const int16_t sx = SX(10);
    const int16_t sy = SY(32);

    for (int i = 0; i < 4; ++i) {
        const int col = i % 2;
        const int row = i / 2;
        const int16_t x = sx + col * (cw + gapX);
        const int16_t y = sy + row * (ch + gapY);

        tft.fillRoundRect(x, y, cw, ch, 9, COL_SURFACE);
        tft.drawRoundRect(x, y, cw, ch, 9, COL_BORDER);

        const WorldCity& wc = cfg.worldCities[i];
        time_t cityTime = nowUtc + static_cast<time_t>(wc.utcOffsetMin) * 60;
        struct tm ct{};
        gmtime_r(&cityTime, &ct);

        const bool isDay = timeValid ? (ct.tm_hour >= 6 && ct.tm_hour < 19) : true;
        tft.fillCircle(x + cw - SX(16), y + SY(15), SR(5), isDay ? COL_ACCENT_2 : (uiIsLightTheme() ? COL_ACCENT : 0x9DDF));

        tft.loadFont(SegoeUIBold18TR);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(COL_ACCENT, COL_SURFACE);
        tft.drawString(wc.name, x + SX(10), y + SY(6));
        tft.unloadFont();

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
        tft.drawString(hhBuf, x + SX(10), y + SY(26));
        tft.fillCircle(x + SX(62), y + SY(40), SR(2), COL_ACCENT);
        tft.fillCircle(x + SX(62), y + SY(51), SR(2), COL_ACCENT);
        tft.drawString(mmBuf, x + SX(68), y + SY(26));
        tft.unloadFont();

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
        tft.drawString(meta, x + SX(10), y + ch - SY(5));
        tft.unloadFont();
    }

    drawBottomNav(tft, 3);
}
