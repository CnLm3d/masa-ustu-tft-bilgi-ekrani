#include "ui_market.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "net_services.h"
#include "ui_common.h"

namespace {
void drawTickerRow(TFT_eSPI& tft, int16_t y, const char* name, const char* sub,
                   const char* valStr, uint16_t valCol, float chgPct) {
    const int16_t rh = SY(31);
    tft.fillRoundRect(SX(10), y, SCREEN_W - SX(20), rh, 7, COL_SURFACE);
    tft.drawRoundRect(SX(10), y, SCREEN_W - SX(20), rh, 7, COL_BORDER);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(name, SX(18), y + SY(3));
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(sub, SX(18), y + SY(17));
    tft.unloadFont();

    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(valCol, COL_SURFACE);
    tft.drawString(valStr, SX(228), y + rh / 2);
    tft.unloadFont();

    const bool neg = chgPct < -0.005f;
    const uint16_t badgeBg = neg ? 0x30A2 : 0x1184;
    const uint16_t badgeFg = neg ? COL_ERR : COL_OK;
    tft.fillRoundRect(SX(236), y + SY(7), SX(66), SY(17), 5, badgeBg);

    char chgBuf[16];
    if (fabsf(chgPct) < 0.005f) {
        strlcpy(chgBuf, "TCMB", sizeof(chgBuf));
    } else {
        snprintf(chgBuf, sizeof(chgBuf), "%+.2f%%", chgPct);
    }

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(badgeFg, badgeBg);
    tft.drawString(chgBuf, SX(269), y + rh / 2);
    tft.unloadFont();
}
} // namespace

void uiMarketDraw(TFT_eSPI& tft, const MarketData& mk, bool fetching) {
    tft.fillScreen(COL_BG);
    drawTopBar(tft, "Piyasa • TCMB & Altın", "Yenile", netGetRssi(), netIsWifiConnected(), fetching);

    if (!mk.valid) {
        tft.fillRoundRect(SX(16), SY(44), SCREEN_W - SX(32), SY(136), 12, COL_SURFACE);
        tft.drawRoundRect(SX(16), SY(44), SCREEN_W - SX(32), SY(136), 12, COL_BORDER);
        tft.loadFont(SegoeUIBold18TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(mk.hasError ? COL_ERR : COL_GOLD, COL_SURFACE);
        tft.drawString(mk.hasError ? mk.errorMsg : "Piyasa yükleniyor...", SCREEN_W / 2, SY(102));
        tft.unloadFont();

        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString("TCMB XML ve Altın verisi bekleniyor", SCREEN_W / 2, SY(132));
        tft.unloadFont();

        drawBottomNav(tft, 2);
        return;
    }

    char vUsd[20], vEur[20], vGbp[20], vGram[20], vOns[20];
    snprintf(vUsd, sizeof(vUsd), "%.2f ₺", mk.usdTry);
    snprintf(vEur, sizeof(vEur), "%.2f ₺", mk.eurTry);
    snprintf(vGbp, sizeof(vGbp), "%.2f ₺", mk.gbpTry);
    snprintf(vGram, sizeof(vGram), "%.0f ₺", mk.gramAltinTry);
    snprintf(vOns, sizeof(vOns), "%.0f $", mk.onsAltinUsd);

    drawTickerRow(tft, SY(31),  "USD / TRY",  "TCMB Satış", vUsd,  COL_TEXT, mk.usdChg);
    drawTickerRow(tft, SY(66),  "EUR / TRY",  "TCMB Satış", vEur,  COL_TEXT, mk.eurChg);
    drawTickerRow(tft, SY(101), "GBP / TRY",  "TCMB Satış", vGbp,  COL_TEXT, mk.gbpChg);
    drawTickerRow(tft, SY(136), "GRAM ALTIN", "24 Ayar",    vGram, COL_GOLD, mk.gramChg);
    drawTickerRow(tft, SY(171), "ONS ALTIN",  "XAU / USD",  vOns,  COL_GOLD, mk.onsChg);

    drawBottomNav(tft, 2);
}
