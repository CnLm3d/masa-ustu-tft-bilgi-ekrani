#include "ui_news.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "net_services.h"
#include "ui_common.h"

void uiNewsDraw(TFT_eSPI& tft, const NewsData& news, uint8_t itemIndex,
                bool scrollMode, bool fetching) {
    tft.fillScreen(COL_BG);

    char header[32];
    const uint8_t topicIdx = (news.topic < static_cast<uint8_t>(NewsTopic::COUNT)) ? news.topic : 0;
    snprintf(header, sizeof(header), "Google Haber • %s", TR_TOPIC_NAMES[topicIdx]);
    drawTopBar(tft, PageId::NEWS, header, netGetRssi(), netIsWifiConnected(), fetching);

    if (!news.valid || news.count == 0) {
        tft.fillRoundRect(16, 48, SCREEN_W - 32, 136, 12, COL_SURFACE);
        tft.drawRoundRect(16, 48, SCREEN_W - 32, 136, 12, COL_BORDER);
        tft.loadFont(SegoeUIBold18TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(news.hasError ? COL_ERR : COL_ACCENT, COL_SURFACE);
        tft.drawString(news.hasError ? news.errorMsg : "Haberler yükleniyor...",
                       SCREEN_W / 2, 108);
        tft.unloadFont();

        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString("Google Haberler TR bekleniyor", SCREEN_W / 2, 136);
        tft.unloadFont();

        drawBottomHint(tft, "Döndür: Sayfa", "Basılı: Kategori");
        return;
    }

    const uint8_t idx = itemIndex % news.count;
    const NewsItem& item = news.items[idx];

    // Main headline card
    const int16_t cardX = 12;
    const int16_t cardY = 34;
    const int16_t cardW = SCREEN_W - 24;
    const int16_t cardH = 148;
    const uint16_t borderCol = scrollMode ? COL_ACCENT_2 : COL_BORDER;

    tft.fillRoundRect(cardX, cardY, cardW, cardH, 10, COL_SURFACE);
    tft.drawRoundRect(cardX, cardY, cardW, cardH, 10, borderCol);
    if (scrollMode) {
        tft.drawRoundRect(cardX + 1, cardY + 1, cardW - 2, cardH - 2, 9, borderCol);
    }

    // Topic & counter badge row
    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(scrollMode ? COL_ACCENT_2 : COL_ACCENT, COL_SURFACE);
    tft.drawString(scrollMode ? "HABER GEZME AKTİF" : TR_TOPIC_NAMES[topicIdx], cardX + 12, cardY + 8);

    char countBuf[16];
    snprintf(countBuf, sizeof(countBuf), "%u / %u", idx + 1, news.count);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(countBuf, cardX + cardW - 12, cardY + 8);
    tft.unloadFont();

    // Wrapped Turkish headline (up to 4 lines)
    tft.loadFont(SegoeUIBold18TR);
    drawWrappedUtf8(tft, String(item.title), cardX + 12, cardY + 32,
                    cardW - 24, 23, 4, COL_TEXT, COL_SURFACE);
    tft.unloadFont();

    // Source + time row inside card bottom
    tft.drawFastHLine(cardX + 12, cardY + cardH - 28, cardW - 24, COL_BORDER);
    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_ACCENT_2, COL_SURFACE);
    tft.drawString(item.source, cardX + 12, cardY + cardH - 21);

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(item.timeStr, cardX + cardW - 12, cardY + cardH - 21);
    tft.unloadFont();

    // Next article preview strip
    const NewsItem& nextItem = news.items[(idx + 1) % news.count];
    tft.fillRoundRect(12, 188, SCREEN_W - 24, 24, 6, COL_SURFACE);
    tft.loadFont(SegoeUI14TR);
    String nextLine = String("Sonraki: ") + nextItem.title;
    drawWrappedUtf8(tft, nextLine, 20, 193, SCREEN_W - 40, 16, 1, COL_MUTED, COL_SURFACE);
    tft.unloadFont();

    drawBottomHint(tft,
                   scrollMode ? "Döndür: Haberler" : "Döndür: Sayfa",
                   scrollMode ? "Bas: Çık | Basılı: Konu" : "Bas: Gez | Basılı: Konu");
}
