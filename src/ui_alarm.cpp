#include "ui_alarm.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "fonts/SegoeUIBold42Temp.h"
#include "net_services.h"
#include "ui_common.h"

namespace {
const char* repeatLabel(uint8_t mode) {
    if (mode == 1) return "Hafta İçi";
    if (mode == 2) return "Hafta Sonu";
    return "Her Gün";
}

void drawBtn(TFT_eSPI& tft, const Rect& r, const char* txt, uint16_t bg, uint16_t border, uint16_t fg) {
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, bg);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 8, border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(txt, r.x + r.w / 2, r.y + r.h / 2);
}
} // namespace

void uiAlarmDraw(TFT_eSPI& tft, const DeviceConfig& cfg, int8_t editIdx, bool fetching) {
    tft.fillScreen(COL_BG);

    if (editIdx >= 0 && editIdx < 3) {
        const AlarmItem& a = cfg.alarms[editIdx];
        char hdr[32];
        snprintf(hdr, sizeof(hdr), "Alarm %d Düzenle", editIdx + 1);
        drawTopBar(tft, hdr, "", netGetRssi(), netIsWifiConnected(), fetching);

        tft.fillRoundRect(SX(16), SY(32), SCREEN_W - SX(32), SY(168), 10, COL_SURFACE);
        tft.drawRoundRect(SX(16), SY(32), SCREEN_W - SX(32), SY(168), 10, COL_ACCENT);

        char tbuf[12];
        snprintf(tbuf, sizeof(tbuf), "%02u   %02u", a.hour, a.minute);
        tft.loadFont(SegoeUIBold42Temp);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(COL_TEXT, COL_SURFACE);
        tft.drawString(tbuf, SCREEN_W / 2, SY(34));
        tft.fillCircle(SCREEN_W / 2, SY(48), SR(3), COL_ACCENT);
        tft.fillCircle(SCREEN_W / 2, SY(60), SR(3), COL_ACCENT);
        tft.unloadFont();

        tft.loadFont(SegoeUIBold18TR);
        drawBtn(tft, RECT_AEDIT_H_DEC, "Sa -", COL_BG, COL_BORDER, COL_TEXT);
        drawBtn(tft, RECT_AEDIT_H_INC, "Sa +", COL_BG, COL_BORDER, COL_TEXT);
        drawBtn(tft, RECT_AEDIT_M_DEC, "Dk -", COL_BG, COL_BORDER, COL_TEXT);
        drawBtn(tft, RECT_AEDIT_M_INC, "Dk +", COL_BG, COL_BORDER, COL_TEXT);
        tft.unloadFont();

        tft.loadFont(SegoeUI14TR);
        drawBtn(tft, RECT_AEDIT_REP, repeatLabel(a.repeatMode), COL_BG, COL_ACCENT_2, COL_ACCENT_2);
        drawBtn(tft, RECT_AEDIT_DONE, "Tamam", COL_OK, COL_OK, COL_ON_ACCENT);
        tft.unloadFont();

        drawBottomNav(tft, 4);
        return;
    }

    drawTopBar(tft, "Alarmlar", "Ayar", netGetRssi(), netIsWifiConnected(), fetching);

    for (int i = 0; i < 3; ++i) {
        const AlarmItem& a = cfg.alarms[i];
        const int16_t y = SY(34 + i * 58);
        const int16_t h = SY(50);
        const uint16_t bcol = a.enabled ? COL_ACCENT : COL_BORDER;

        tft.fillRoundRect(SX(12), y, SCREEN_W - SX(24), h, 9, COL_SURFACE);
        tft.drawRoundRect(SX(12), y, SCREEN_W - SX(24), h, 9, bcol);

        char tbuf[10];
        snprintf(tbuf, sizeof(tbuf), "%02u:%02u", a.hour, a.minute);
        tft.loadFont(SegoeUIBold18TR);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(a.enabled ? COL_TEXT : COL_MUTED, COL_SURFACE);
        tft.drawString(tbuf, SX(22), y + SY(6));
        tft.unloadFont();

        char sub[40];
        snprintf(sub, sizeof(sub), "%s • %s", a.label, repeatLabel(a.repeatMode));
        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString(sub, SX(22), y + SY(29));
        tft.unloadFont();

        const int16_t tx = SX(246);
        const int16_t ty = y + SY(13);
        const int16_t tw = SX(50);
        const int16_t th = SY(24);
        const uint16_t pillCol = a.enabled ? COL_OK : COL_BORDER;
        tft.fillRoundRect(tx, ty, tw, th, th / 2, pillCol);
        const int16_t kx = a.enabled ? (tx + tw - th / 2) : (tx + th / 2);
        tft.fillCircle(kx, ty + th / 2, SR(9), COL_ON_ACCENT);
    }

    drawBottomNav(tft, 4);
}

void uiAlarmRingingDraw(TFT_eSPI& tft, const AlarmItem& alarm, bool invertPhase) {
    const uint16_t bg = invertPhase ? 0x6000 : COL_BG;
    const uint16_t border = invertPhase ? COL_ACCENT_2 : COL_ERR;
    tft.fillScreen(bg);

    tft.fillRoundRect(SX(16), SY(22), SCREEN_W - SX(32), SY(118), 12, COL_SURFACE);
    tft.drawRoundRect(SX(16), SY(22), SCREEN_W - SX(32), SY(118), 12, border);
    tft.drawRoundRect(SX(16) + 1, SY(22) + 1, SCREEN_W - SX(32) - 2, SY(118) - 2, 11, border);

    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COL_ACCENT_2, COL_SURFACE);
    tft.drawString("ALARM ÇALIYOR!", SCREEN_W / 2, SY(32));
    tft.unloadFont();

    char tbuf[10];
    snprintf(tbuf, sizeof(tbuf), "%02u:%02u", alarm.hour, alarm.minute);
    tft.loadFont(SegoeUIBold42Temp);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(tbuf, SCREEN_W / 2, SY(82));
    tft.unloadFont();

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(alarm.label, SCREEN_W / 2, SY(130));
    tft.unloadFont();

    tft.loadFont(SegoeUIBold18TR);
    drawBtn(tft, RECT_RING_STOP, "DURDUR", COL_ERR, COL_ERR, COL_TEXT);
    drawBtn(tft, RECT_RING_SNOOZE, "5 DK ERTELE", COL_SURFACE, COL_ACCENT, COL_TEXT);
    tft.unloadFont();
}
