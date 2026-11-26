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
    drawTopBar(tft, header, "Yenile", netGetRssi(), netIsWifiConnected(), fetching);

    if (!news.valid || news.count == 0) {
        tft.fillRoundRect(SX(16), SY(44), SCREEN_W - SX(32), SY(124), 12, COL_SURFACE);
        tft.drawRoundRect(SX(16), SY(44), SCREEN_W - SX(32), SY(124), 12, COL_BORDER);
        tft.loadFont(SegoeUIBold18TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(news.hasError ? COL_ERR : COL_ACCENT, COL_SURFACE);
        tft.drawString(news.hasError ? news.errorMsg : "Haberler yükleniyor...",
                       SCREEN_W / 2, SY(94));
        tft.unloadFont();

        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString("Google Haberler TR bekleniyor", SCREEN_W / 2, SY(124));
        drawActionButton(tft, RECT_NEWS_TOPIC, "Konu Değiş", COL_ACCENT);
        tft.unloadFont();

        drawBottomNav(tft, 3);
        return;
    }

    const uint8_t idx = itemIndex % news.count;
    const NewsItem& item = news.items[idx];

    const int16_t cardX = SX(12);
    const int16_t cardY = SY(32);
    const int16_t cardW = SCREEN_W - SX(24);
    const int16_t cardH = SY(136);

    tft.fillRoundRect(cardX, cardY, cardW, cardH, 10, COL_SURFACE);
    tft.drawRoundRect(cardX, cardY, cardW, cardH, 10, COL_BORDER);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_ACCENT, COL_SURFACE);
    tft.drawString(TR_TOPIC_NAMES[topicIdx], cardX + SX(12), cardY + SY(8));

    char countBuf[16];
    snprintf(countBuf, sizeof(countBuf), "%u / %u", idx + 1, news.count);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(countBuf, cardX + cardW - SX(12), cardY + SY(8));
    tft.unloadFont();

    tft.loadFont(SegoeUIBold18TR);
    drawWrappedUtf8(tft, String(item.title), cardX + SX(12), cardY + SY(30),
                    cardW - SX(24), SY(23), 3, COL_TEXT, COL_SURFACE);
    tft.unloadFont();

    tft.drawFastHLine(cardX + SX(12), cardY + cardH - SY(27), cardW - SX(24), COL_BORDER);
    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_ACCENT_2, COL_SURFACE);
    tft.drawString(item.source, cardX + SX(12), cardY + cardH - SY(21));

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(item.timeStr, cardX + cardW - SX(12), cardY + cardH - SY(21));

    drawActionButton(tft, RECT_NEWS_PREV, "< Önceki", COL_BORDER);
    drawActionButton(tft, RECT_NEWS_TOPIC, "Konu Değiş", COL_ACCENT);
    drawActionButton(tft, RECT_NEWS_NEXT, "Sonraki >", COL_BORDER);
    tft.unloadFont();

    drawBottomNav(tft, 3);
}
