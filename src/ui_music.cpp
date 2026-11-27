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

void uiMusicDrawMeter(TFT_eSPI& tft, const MusicState& st) {
    const int16_t mx = SX(22);
    const int16_t my = SY(134);
    const int16_t mw = SX(276);
    const int16_t mh = SY(12);

    tft.fillRect(mx, my, mw, mh + 2, COL_SURFACE);

    if (st.btActive) {
        // 16-segment stereo VU bars (left=8 bars, right=8 bars)
        const int16_t segW = (mw - 15 * SX(2)) / 16;
        const int activeL = (st.vuLeft * 8) / 100;
        const int activeR = (st.vuRight * 8) / 100;
        for (int i = 0; i < 8; ++i) {
            const uint16_t cL = (i < activeL) ? ((i >= 6) ? COL_ERR : (i >= 4 ? COL_ACCENT_2 : COL_SPOTIFY)) : COL_BORDER;
            const uint16_t cR = (i < activeR) ? ((i >= 6) ? COL_ERR : (i >= 4 ? COL_ACCENT_2 : COL_SPOTIFY)) : COL_BORDER;
            tft.fillRoundRect(mx + i * (segW + SX(2)), my, segW, mh, 2, cL);
            tft.fillRoundRect(mx + (i + 8) * (segW + SX(2)), my, segW, mh, 2, cR);
        }
    } else {
        tft.fillRoundRect(mx, my + SY(3), mw, SY(6), 3, COL_BG);
        if (st.durationMs > 0) {
            int16_t fw = static_cast<int16_t>(
                min<uint32_t>(mw, (static_cast<uint64_t>(mw) * st.progressMs) / st.durationMs));
            if (fw > 3) tft.fillRoundRect(mx, my + SY(3), fw, SY(6), 3, COL_SPOTIFY);
        }
    }
}

void uiMusicDraw(TFT_eSPI& tft, const MusicState& st, bool btSupported, bool fetching) {
    tft.fillScreen(COL_BG);
    drawTopBar(tft, st.btActive ? "BT Hoparlör (PCM5102A)" : "Spotify Wi-Fi",
               st.btActive ? "Wi-Fi" : "BT Aç", netGetRssi(), netIsWifiConnected(), fetching);

    tft.loadFont(SegoeUI14TR);
    drawBtn(tft, RECT_MUS_MODE_BT,
            btSupported ? (st.btActive ? "BT Aktif (I2S)" : "BT Hoparlör") : "C3 BT Yok",
            st.btActive ? COL_ACCENT : COL_SURFACE,
            st.btActive ? COL_ACCENT : COL_BORDER,
            st.btActive ? COL_ON_ACCENT : COL_MUTED);
    drawBtn(tft, RECT_MUS_MODE_WIFI, "Spotify Wi-Fi",
            !st.btActive ? COL_SPOTIFY : COL_SURFACE,
            !st.btActive ? COL_SPOTIFY : COL_BORDER,
            !st.btActive ? COL_ON_ACCENT : COL_MUTED);
    tft.unloadFont();

    // Now-Playing Card
    const int16_t cx = SX(12), cy = SY(62), cw = SX(296), ch = SY(92);
    tft.fillRoundRect(cx, cy, cw, ch, 10, COL_SURFACE);
    tft.drawRoundRect(cx, cy, cw, ch, 10, st.btActive ? COL_ACCENT : COL_SPOTIFY);

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(st.btActive ? COL_ACCENT : COL_SPOTIFY, COL_SURFACE);
    tft.drawString(st.statusMsg[0] ? st.statusMsg : (st.btActive ? "Bluetooth Hazır" : "Spotify"),
                   cx + SX(10), cy + SY(7));

    char volBuf[16];
    snprintf(volBuf, sizeof(volBuf), "Ses: %%%u", st.volume);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_ACCENT_2, COL_SURFACE);
    tft.drawString(volBuf, cx + cw - SX(10), cy + SY(7));
    tft.unloadFont();

    tft.loadFont(SegoeUIBold18TR);
    const char* titleTxt = st.title[0] ? st.title : (st.btActive ? "Telefonunuzdan Bağlanın" : "Spotify Çalmıyor");
    drawWrappedUtf8(tft, String(titleTxt), cx + SX(10), cy + SY(27),
                    cw - SX(20), SY(21), 2, COL_TEXT, COL_SURFACE);
    tft.unloadFont();

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    const char* artistTxt = st.artist[0] ? st.artist : "PCM5102A I2S DAC";
    tft.drawString(artistTxt, cx + SX(10), cy + SY(56));
    tft.unloadFont();

    uiMusicDrawMeter(tft, st);

    // Media control buttons
    tft.loadFont(SegoeUI14TR);
    drawBtn(tft, RECT_MUS_PREV, "<|", COL_SURFACE, COL_BORDER, COL_TEXT);
    drawBtn(tft, RECT_MUS_PLAY, st.isPlaying ? "Duraklat" : "Oynat",
            COL_SURFACE, st.btActive ? COL_ACCENT : COL_SPOTIFY, COL_TEXT);
    drawBtn(tft, RECT_MUS_NEXT, "|>", COL_SURFACE, COL_BORDER, COL_TEXT);
    drawBtn(tft, RECT_MUS_VOLDN, "Ses -", COL_SURFACE, COL_BORDER, COL_TEXT);
    drawBtn(tft, RECT_MUS_VOLUP, "Ses +", COL_SURFACE, COL_BORDER, COL_TEXT);
    tft.unloadFont();

    drawBottomNav(tft, 7);
}
