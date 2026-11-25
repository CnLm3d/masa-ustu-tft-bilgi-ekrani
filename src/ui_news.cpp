#include "ui_news.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "net_services.h"
#include "ui_common.h"

namespace {
void drawActionButton(TFT_eSPI& tft, const Rect& r, const char* label, uint16_t borderCol) {
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 7, COL_SURFACE);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 7, borderCol);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
}
} // namespace

void uiNewsDraw(TFT_eSPI& tft, const NewsData& news, uint8_t itemIndex, bool fetching) {
    tft.fillScreen(COL_BG);

    char header[36];
    const uint8_t topicIdx = (news.topic < static_cast<uint8_t>(NewsTopic::COUNT)) ? news.topic : 0;
    snprintf(header, sizeof(header), "Haber • %s", TR_TOPIC_NAMES[topicIdx]);
    drawTopBar(tft, header, netGetRssi(), netIsWifiConnected(), fetching);

    if (!news.valid || news.count == 0) {
        tft.fillRoundRect(16, 44, SCREEN_W - 32, 124, 12, COL_SURFACE);
        tft.drawRoundRect(16, 44, SCREEN_W - 32, 124, 12, COL_BORDER);
        tft.loadFont(SegoeUIBold18TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(news.hasError ? COL_ERR : COL_ACCENT, COL_SURFACE);
        tft.drawString(news.hasError ? news.errorMsg : "Haberler yükleniyor...",
                       SCREEN_W / 2, 94);
        tft.unloadFont();

        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString("Google Haberler TR bekleniyor", SCREEN_W / 2, 124);
        drawActionButton(tft, RECT_NEWS_TOPIC, "Konu Değiş", COL_ACCENT);
        tft.unloadFont();

        drawBottomTabs(tft, PageId::NEWS);
        return;
    }

    const uint8_t idx = itemIndex % news.count;
    const NewsItem& item = news.items[idx];

    // Main headline card
    const int16_t cardX = 12;
    const int16_t cardY = 32;
    const int16_t cardW = SCREEN_W - 24;
    const int16_t cardH = 136;

    tft.fillRoundRect(cardX, cardY, cardW, cardH, 10, COL_SURFACE);
    tft.drawRoundRect(cardX, cardY, cardW, cardH, 10, COL_BORDER);

    // Topic & counter row
    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_ACCENT, COL_SURFACE);
    tft.drawString(TR_TOPIC_NAMES[topicIdx], cardX + 12, cardY + 8);

    char countBuf[16];
    snprintf(countBuf, sizeof(countBuf), "%u / %u", idx + 1, news.count);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(countBuf, cardX + cardW - 12, cardY + 8);
    tft.unloadFont();

    // Wrapped Turkish headline (up to 3 lines)
    tft.loadFont(SegoeUIBold18TR);
    drawWrappedUtf8(tft, String(item.title), cardX + 12, cardY + 30,
                    cardW - 24, 23, 3, COL_TEXT, COL_SURFACE);
    tft.unloadFont();

    // Source + time footer inside card
    tft.drawFastHLine(cardX + 12, cardY + cardH - 27, cardW - 24, COL_BORDER);
    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_ACCENT_2, COL_SURFACE);
    tft.drawString(item.source, cardX + 12, cardY + cardH - 21);

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(item.timeStr, cardX + cardW - 12, cardY + cardH - 21);

    // Touch action bar
    drawActionButton(tft, RECT_NEWS_PREV, "< Önceki", COL_BORDER);
    drawActionButton(tft, RECT_NEWS_TOPIC, "Konu Değiş", COL_ACCENT);
    drawActionButton(tft, RECT_NEWS_NEXT, "Sonraki >", COL_BORDER);
    tft.unloadFont();

    drawBottomTabs(tft, PageId::NEWS);
}
