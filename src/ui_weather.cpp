#include "ui_weather.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "fonts/SegoeUIBold42Temp.h"
#include "net_services.h"
#include "ui_common.h"
#include <time.h>

namespace {
const char* dayLabelFromIso(const char* iso, int offsetIdx) {
    if (offsetIdx == 1) return "Yarın";
    int y = 0, m = 0, d = 0;
    if (sscanf(iso, "%d-%d-%d", &y, &m, &d) == 3) {
        struct tm t{};
        t.tm_year = y - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
        t.tm_hour = 12;
        mktime(&t);
        if (t.tm_wday >= 0 && t.tm_wday < 7) {
            return TR_DAYS_SHORT[t.tm_wday];
        }
    }
    return "---";
}
} // namespace

void uiWeatherDraw(TFT_eSPI& tft, const DeviceConfig& cfg, const WeatherData& wx, bool fetching) {
    tft.fillScreen(COL_BG);
    drawTopBar(tft, cfg.city, "Yenile", netGetRssi(), netIsWifiConnected(), fetching);

    if (!wx.valid) {
        tft.fillRoundRect(16, 44, SCREEN_W - 32, 136, 12, COL_SURFACE);
        tft.drawRoundRect(16, 44, SCREEN_W - 32, 136, 12, COL_BORDER);
        tft.loadFont(SegoeUIBold18TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(wx.hasError ? COL_ERR : COL_ACCENT, COL_SURFACE);
        tft.drawString(wx.hasError ? wx.errorMsg : "Hava durumu yükleniyor...",
                       SCREEN_W / 2, 102);
        tft.unloadFont();

        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString("Yenilemek için karta dokunun", SCREEN_W / 2, 132);
        tft.unloadFont();

        drawBottomNav(tft, 3);
        return;
    }

    tft.fillRoundRect(12, 32, SCREEN_W - 24, 88, 10, COL_SURFACE);
    tft.drawRoundRect(12, 32, SCREEN_W - 24, 88, 10, COL_BORDER);

    drawWeatherIcon(tft, 46, 63, 20, wx.wmoCode, wx.isDay);

    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%.0f°C", wx.tempC);
    tft.loadFont(SegoeUIBold42Temp);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(tempBuf, 78, 35);
    tft.unloadFont();

    char line1[28], line2[28], line3[28];
    snprintf(line1, sizeof(line1), "Hissedilen: %.0f°", wx.feelsLikeC);
    snprintf(line2, sizeof(line2), "Nem: %%%d", wx.humidity);
    snprintf(line3, sizeof(line3), "Rüzgar: %.0f km/s", wx.windKmh);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(line1, SCREEN_W - 22, 38);
    tft.drawString(line2, SCREEN_W - 22, 55);
    tft.drawString(line3, SCREEN_W - 22, 72);
    tft.unloadFont();

    tft.drawFastHLine(22, 92, SCREEN_W - 44, COL_BORDER);

    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_ACCENT, COL_SURFACE);
    tft.drawString(wmoToTurkish(wx.wmoCode), 22, 96);
    tft.unloadFont();

    tft.fillRoundRect(240, 95, 60, 20, 6, COL_BG);
    tft.drawRoundRect(240, 95, 60, 20, 6, COL_ACCENT);
    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("Yenile", 270, 105);
    tft.unloadFont();

    const int16_t cardW = 94;
    const int16_t cardH = 76;
    const int16_t gap = 7;
    const int16_t startX = 12;
    const int16_t cardY = 126;

    for (int i = 1; i <= 3; ++i) {
        const int16_t cx = startX + (i - 1) * (cardW + gap);
        tft.fillRoundRect(cx, cardY, cardW, cardH, 8, COL_SURFACE);
        tft.drawRoundRect(cx, cardY, cardW, cardH, 8, COL_BORDER);

        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(COL_TEXT, COL_SURFACE);
        tft.drawString(dayLabelFromIso(wx.daily[i].dateIso, i), cx + cardW / 2, cardY + 6);
        tft.unloadFont();

        drawWeatherIcon(tft, cx + cardW / 2, cardY + 38, 13, wx.daily[i].wmoCode, true);

        char rangeBuf[20];
        snprintf(rangeBuf, sizeof(rangeBuf), "%.0f° / %.0f°", wx.daily[i].tempMax, wx.daily[i].tempMin);
        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(BC_DATUM);
        tft.setTextColor(COL_ACCENT_2, COL_SURFACE);
        tft.drawString(rangeBuf, cx + cardW / 2, cardY + cardH - 5);
        tft.unloadFont();
    }

    drawBottomNav(tft, 3);
}
