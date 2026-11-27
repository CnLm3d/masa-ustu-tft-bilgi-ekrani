#include "ui_market.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "net_services.h"
#include "ui_common.h"

namespace {
void drawTickerRow(TFT_eSPI& tft, int16_t y, const char* name, const char* sub,
                   const char* valStr, uint16_t valCol, float chgPct, bool highlighted) {
    const int16_t rx = SX(10);
    const int16_t rw = SCREEN_W - SX(20);
    const int16_t rh = SY(31);
    const uint16_t accentCol = (chgPct > 0.005f) ? COL_OK : ((chgPct < -0.005f) ? COL_ERR : COL_GOLD);

    tft.fillRoundRect(rx, y, rw, rh, 7, COL_SURFACE);
    if (highlighted) {
        tft.drawRoundRect(rx, y, rw, rh, 7, accentCol);
        tft.drawRoundRect(rx + 1, y + 1, rw - 2, rh - 2, 6, accentCol);
        tft.fillRoundRect(rx + SX(3), y + SY(4), SX(4), rh - SY(8), 2, accentCol);
    } else {
        tft.drawRoundRect(rx, y, rw, rh, 7, COL_BORDER);
    }

    const int16_t tx = highlighted ? SX(22) : SX(18);
    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(highlighted ? accentCol : COL_TEXT, COL_SURFACE);
    tft.drawString(name, tx, y + SY(3));
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(sub, tx, y + SY(17));
    tft.unloadFont();

    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(valCol, COL_SURFACE);
    tft.drawString(valStr, SX(228), y + rh / 2);
    tft.unloadFont();

    const bool neg = chgPct < -0.005f;
    const uint16_t badgeBg = uiIsLightTheme()
        ? (neg ? 0xFDB6 : 0xC7B8)
        : (neg ? 0x30A2 : 0x1184);
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

void drawRowByIndex(TFT_eSPI& tft, const MarketData& mk, uint8_t idx, bool highlighted) {
    char valBuf[20];
    switch (idx % 5) {
        case 0:
            snprintf(valBuf, sizeof(valBuf), "%.2f ₺", mk.usdTry);
            drawTickerRow(tft, SY(31), "USD / TRY", "TCMB Satış", valBuf, COL_TEXT, mk.usdChg, highlighted);
            break;
        case 1:
            snprintf(valBuf, sizeof(valBuf), "%.2f ₺", mk.eurTry);
            drawTickerRow(tft, SY(66), "EUR / TRY", "TCMB Satış", valBuf, COL_TEXT, mk.eurChg, highlighted);
            break;
        case 2:
            snprintf(valBuf, sizeof(valBuf), "%.2f ₺", mk.gbpTry);
            drawTickerRow(tft, SY(101), "GBP / TRY", "TCMB Satış", valBuf, COL_TEXT, mk.gbpChg, highlighted);
            break;
        case 3:
            snprintf(valBuf, sizeof(valBuf), "%.0f ₺", mk.gramAltinTry);
            drawTickerRow(tft, SY(136), "GRAM ALTIN", "24 Ayar", valBuf, COL_GOLD, mk.gramChg, highlighted);
            break;
        default:
            snprintf(valBuf, sizeof(valBuf), "%.0f $", mk.onsAltinUsd);
            drawTickerRow(tft, SY(171), "ONS ALTIN", "XAU / USD", valBuf, COL_GOLD, mk.onsChg, highlighted);
            break;
    }
}
} // namespace

void uiMarketDraw(TFT_eSPI& tft, const MarketData& mk, uint8_t focusRow, bool fetching) {
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

    const uint8_t active = focusRow % 5;
    for (uint8_t i = 0; i < 5; ++i) {
        drawRowByIndex(tft, mk, i, i == active);
    }

    drawBottomNav(tft, 2);
}

void uiMarketUpdateHighlight(TFT_eSPI& tft, const MarketData& mk, uint8_t prevRow, uint8_t newRow) {
    if (!mk.valid) return;
    prevRow %= 5;
    newRow %= 5;
    if (prevRow != newRow) {
        drawRowByIndex(tft, mk, prevRow, false);
    }
    drawRowByIndex(tft, mk, newRow, true);
}
