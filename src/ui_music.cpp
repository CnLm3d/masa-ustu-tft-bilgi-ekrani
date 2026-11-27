#include "ui_music.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "net_services.h"
#include "ui_common.h"

namespace {
void drawBtn(TFT_eSPI& tft, const Rect& r, const char* txt, uint16_t bg, uint16_t bcol, uint16_t fg) {
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 7, bg);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 7, bcol);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(txt, r.x + r.w / 2, r.y + r.h / 2);
}
} // namespace

void uiMusicDrawMeter(TFT_eSPI& tft, const RadioState& st) {
    const int16_t mx = SX(22);
    const int16_t my = SY(136);
    const int16_t mw = SX(276);
    const int16_t mh = SY(11);

    tft.fillRect(mx, my, mw, mh + 2, COL_SURFACE);

    const int16_t segW = (mw - 15 * SX(2)) / 16;
    const int activeL = st.isPlaying ? ((st.vuLeft * 8 + 5) / 100) : 0;
    const int activeR = st.isPlaying ? ((st.vuRight * 8 + 5) / 100) : 0;
    for (int i = 0; i < 8; ++i) {
        const uint16_t cL = (i < activeL) ? ((i >= 6) ? COL_ERR : (i >= 4 ? COL_ACCENT_2 : COL_RADIO)) : COL_BORDER;
        const uint16_t cR = (i < activeR) ? ((i >= 6) ? COL_ERR : (i >= 4 ? COL_ACCENT_2 : COL_RADIO)) : COL_BORDER;
        tft.fillRoundRect(mx + i * (segW + SX(2)), my, segW, mh, 2, cL);
        tft.fillRoundRect(mx + (i + 8) * (segW + SX(2)), my, segW, mh, 2, cR);
    }
}

void uiMusicDraw(TFT_eSPI& tft, const RadioState& st, bool fetching) {
    tft.fillScreen(COL_BG);
    drawTopBar(tft, "İnternet Radyosu", "Bağlan", netGetRssi(), netIsWifiConnected(), fetching || st.connecting);

    // Top row: Station selector pill + PCM5102A badge
    tft.loadFont(SegoeUI14TR);
    char stBuf[48];
    if (st.stationCount > 0) {
        snprintf(stBuf, sizeof(stBuf), "%u/%u • %s",
                 static_cast<unsigned>(st.stationIdx + 1),
                 static_cast<unsigned>(st.stationCount),
                 st.stationName);
    } else {
        strlcpy(stBuf, "Kanal Ekleyin (Web UI)", sizeof(stBuf));
    }
    tft.fillRoundRect(RECT_MUS_STATION.x, RECT_MUS_STATION.y, RECT_MUS_STATION.w, RECT_MUS_STATION.h, 7, COL_SURFACE);
    tft.drawRoundRect(RECT_MUS_STATION.x, RECT_MUS_STATION.y, RECT_MUS_STATION.w, RECT_MUS_STATION.h, 7, COL_RADIO);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(stBuf, RECT_MUS_STATION.x + SX(8), RECT_MUS_STATION.y + RECT_MUS_STATION.h / 2);

    const Rect dacBadge = {SX(214), SY(30), SX(94), SY(26)};
    drawBtn(tft, dacBadge, "PCM5102A", COL_RADIO, COL_RADIO, COL_ON_ACCENT);
    tft.unloadFont();

    // Now-Playing Card
    const int16_t cx = SX(12), cy = SY(62), cw = SX(296), ch = SY(94);
    const uint16_t cardBorder = st.hasError ? COL_ERR : (st.isPlaying ? COL_RADIO : COL_BORDER);
    tft.fillRoundRect(cx, cy, cw, ch, 10, COL_SURFACE);
    tft.drawRoundRect(cx, cy, cw, ch, 10, cardBorder);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(st.hasError ? COL_ERR : (st.connecting ? COL_ACCENT_2 : COL_RADIO), COL_SURFACE);
    const char* statusTxt = st.statusMsg[0] ? st.statusMsg : (st.isPlaying ? "CANLI MP3 • ICY" : "Hazır");
    tft.drawString(statusTxt, cx + SX(10), cy + SY(7));

    char volBuf[16];
    snprintf(volBuf, sizeof(volBuf), "Ses: %%%u", st.volume);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_ACCENT_2, COL_SURFACE);
    tft.drawString(volBuf, cx + cw - SX(10), cy + SY(7));
    tft.unloadFont();

    tft.loadFont(SegoeUIBold18TR);
    const char* mainTitle = st.streamTitle[0] ? st.streamTitle : (st.stationName[0] ? st.stationName : "İnternet Radyosu");
    drawWrappedUtf8(tft, String(mainTitle), cx + SX(10), cy + SY(28),
                    cw - SX(20), SY(21), 2, COL_TEXT, COL_SURFACE);
    tft.unloadFont();

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    char infoBuf[48];
    if (st.isPlaying && st.sampleRate > 0) {
        snprintf(infoBuf, sizeof(infoBuf), "PCM5102A • %.1f kHz MP3", st.sampleRate / 1000.0f);
    } else {
        strlcpy(infoBuf, "Web UI'dan yeni MP3 kanalları eklenebilir", sizeof(infoBuf));
    }
    tft.drawString(infoBuf, cx + SX(10), cy + SY(56));
    tft.unloadFont();

    uiMusicDrawMeter(tft, st);

    // Media control buttons
    tft.loadFont(SegoeUI14TR);
    drawBtn(tft, RECT_MUS_PREV, "<|", COL_SURFACE, COL_BORDER, COL_TEXT);
    const bool active = st.isPlaying || st.connecting;
    drawBtn(tft, RECT_MUS_PLAY, active ? "Durdur" : "Çal",
            active ? COL_RADIO : COL_SURFACE,
            COL_RADIO,
            active ? COL_ON_ACCENT : COL_TEXT);
    drawBtn(tft, RECT_MUS_NEXT, "|>", COL_SURFACE, COL_BORDER, COL_TEXT);
    drawBtn(tft, RECT_MUS_VOLDN, "Ses -", COL_SURFACE, COL_BORDER, COL_TEXT);
    drawBtn(tft, RECT_MUS_VOLUP, "Ses +", COL_SURFACE, COL_BORDER, COL_TEXT);
    tft.unloadFont();

    drawBottomNav(tft, 7);
}
