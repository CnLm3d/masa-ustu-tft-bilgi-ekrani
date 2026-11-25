#include "ui_market.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "net_services.h"
#include "ui_common.h"

namespace {
void drawTickerRow(TFT_eSPI& tft, int16_t y, const char* name, const char* sub,
                   const char* valStr, uint16_t valCol, float chgPct) {
    tft.fillRoundRect(10, y, SCREEN_W - 20, 31, 7, COL_SURFACE);
    tft.drawRoundRect(10, y, SCREEN_W - 20, 31, 7, COL_BORDER);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(name, 18, y + 3);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(sub, 18, y + 17);
    tft.unloadFont();

    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(valCol, COL_SURFACE);
    tft.drawString(valStr, 228, y + 16);
    tft.unloadFont();

    const bool neg = chgPct < -0.005f;
    const uint16_t badgeBg = neg ? 0x30A2 : 0x1184;
    const uint16_t badgeFg = neg ? COL_ERR : COL_OK;
    tft.fillRoundRect(236, y + 7, 66, 17, 5, badgeBg);

    char chgBuf[16];
    if (fabsf(chgPct) < 0.005f) {
        strlcpy(chgBuf, "TCMB", sizeof(chgBuf));
    } else {
        snprintf(chgBuf, sizeof(chgBuf), "%+.2f%%", chgPct);
    }

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(badgeFg, badgeBg);
    tft.drawString(chgBuf, 269, y + 15);
    tft.unloadFont();
}
} // namespace

void uiMarketDraw(TFT_eSPI& tft, const MarketData& mk, bool fetching) {
    tft.fillScreen(COL_BG);
    drawTopBar(tft, "Piyasa • TCMB & Altın", "Yenile", netGetRssi(), netIsWifiConnected(), fetching);

    if (!mk.valid) {
        tft.fillRoundRect(16, 44, SCREEN_W - 32, 136, 12, COL_SURFACE);
        tft.drawRoundRect(16, 44, SCREEN_W - 32, 136, 12, COL_BORDER);
        tft.loadFont(SegoeUIBold18TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(mk.hasError ? COL_ERR : COL_GOLD, COL_SURFACE);
        tft.drawString(mk.hasError ? mk.errorMsg : "Piyasa yükleniyor...", SCREEN_W / 2, 102);
        tft.unloadFont();

        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString("TCMB XML ve Altın verisi bekleniyor", SCREEN_W / 2, 132);
        tft.unloadFont();

        drawBottomNav(tft, 1);
        return;
    }

    char vUsd[20], vEur[20], vGbp[20], vGram[20], vOns[20];
    snprintf(vUsd, sizeof(vUsd), "%.2f ₺", mk.usdTry);
    snprintf(vEur, sizeof(vEur), "%.2f ₺", mk.eurTry);
    snprintf(vGbp, sizeof(vGbp), "%.2f ₺", mk.gbpTry);
    snprintf(vGram, sizeof(vGram), "%.0f ₺", mk.gramAltinTry);
    snprintf(vOns, sizeof(vOns), "%.0f $", mk.onsAltinUsd);

    drawTickerRow(tft, 31, "USD / TRY", "TCMB Satış", vUsd, COL_TEXT, mk.usdChg);
    drawTickerRow(tft, 66, "EUR / TRY", "TCMB Satış", vEur, COL_TEXT, mk.eurChg);
    drawTickerRow(tft, 101, "GBP / TRY", "TCMB Satış", vGbp, COL_TEXT, mk.gbpChg);
    drawTickerRow(tft, 136, "GRAM ALTIN", "24 Ayar", vGram, COL_GOLD, mk.gramChg);
    drawTickerRow(tft, 171, "ONS ALTIN", "XAU / USD", vOns, COL_GOLD, mk.onsChg);

    drawBottomNav(tft, 1);
}
