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

        tft.fillRoundRect(16, 32, SCREEN_W - 32, 168, 10, COL_SURFACE);
        tft.drawRoundRect(16, 32, SCREEN_W - 32, 168, 10, COL_ACCENT);

        char tbuf[12];
        snprintf(tbuf, sizeof(tbuf), "%02u   %02u", a.hour, a.minute);
        tft.loadFont(SegoeUIBold42Temp);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(COL_TEXT, COL_SURFACE);
        tft.drawString(tbuf, SCREEN_W / 2, 34);
        tft.fillCircle(SCREEN_W / 2, 48, 3, COL_ACCENT);
        tft.fillCircle(SCREEN_W / 2, 60, 3, COL_ACCENT);
        tft.unloadFont();

        tft.loadFont(SegoeUIBold18TR);
        drawBtn(tft, RECT_AEDIT_H_DEC, "Sa -", COL_BG, COL_BORDER, COL_TEXT);
        drawBtn(tft, RECT_AEDIT_H_INC, "Sa +", COL_BG, COL_BORDER, COL_TEXT);
        drawBtn(tft, RECT_AEDIT_M_DEC, "Dk -", COL_BG, COL_BORDER, COL_TEXT);
        drawBtn(tft, RECT_AEDIT_M_INC, "Dk +", COL_BG, COL_BORDER, COL_TEXT);
        tft.unloadFont();

        tft.loadFont(SegoeUI14TR);
        drawBtn(tft, RECT_AEDIT_REP, repeatLabel(a.repeatMode), COL_BG, COL_ACCENT_2, COL_ACCENT_2);
        drawBtn(tft, RECT_AEDIT_DONE, "Tamam", COL_OK, COL_OK, COL_BG);
        tft.unloadFont();

        drawBottomNav(tft, 3);
        return;
    }

    drawTopBar(tft, "Alarmlar", "Ayar", netGetRssi(), netIsWifiConnected(), fetching);

    for (int i = 0; i < 3; ++i) {
        const AlarmItem& a = cfg.alarms[i];
        const int16_t y = 34 + i * 58;
        const uint16_t bcol = a.enabled ? COL_ACCENT : COL_BORDER;

        tft.fillRoundRect(12, y, SCREEN_W - 24, 50, 9, COL_SURFACE);
        tft.drawRoundRect(12, y, SCREEN_W - 24, 50, 9, bcol);

        char tbuf[10];
        snprintf(tbuf, sizeof(tbuf), "%02u:%02u", a.hour, a.minute);
        tft.loadFont(SegoeUIBold18TR);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(a.enabled ? COL_TEXT : COL_MUTED, COL_SURFACE);
        tft.drawString(tbuf, 22, y + 6);
        tft.unloadFont();

        char sub[40];
        snprintf(sub, sizeof(sub), "%s • %s", a.label, repeatLabel(a.repeatMode));
        tft.loadFont(SegoeUI14TR);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(COL_MUTED, COL_SURFACE);
        tft.drawString(sub, 22, y + 29);
        tft.unloadFont();

        // Toggle switch pill
        const int16_t tx = 246;
        const int16_t ty = y + 13;
        const uint16_t pillCol = a.enabled ? COL_OK : 0x2945;
        tft.fillRoundRect(tx, ty, 50, 24, 12, pillCol);
        const int16_t kx = a.enabled ? (tx + 38) : (tx + 12);
        tft.fillCircle(kx, ty + 12, 9, COL_TEXT);
    }

    drawBottomNav(tft, 3);
}

void uiAlarmRingingDraw(TFT_eSPI& tft, const AlarmItem& alarm, bool invertPhase) {
    const uint16_t bg = invertPhase ? 0x6000 : COL_BG;
    const uint16_t border = invertPhase ? COL_ACCENT_2 : COL_ERR;
    tft.fillScreen(bg);

    tft.fillRoundRect(16, 22, SCREEN_W - 32, 118, 12, COL_SURFACE);
    tft.drawRoundRect(16, 22, SCREEN_W - 32, 118, 12, border);
    tft.drawRoundRect(17, 23, SCREEN_W - 34, 116, 11, border);

    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COL_ACCENT_2, COL_SURFACE);
    tft.drawString("ALARM ÇALIYOR!", SCREEN_W / 2, 32);
    tft.unloadFont();

    char tbuf[10];
    snprintf(tbuf, sizeof(tbuf), "%02u:%02u", alarm.hour, alarm.minute);
    tft.loadFont(SegoeUIBold42Temp);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(tbuf, SCREEN_W / 2, 82);
    tft.unloadFont();

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(alarm.label, SCREEN_W / 2, 130);
    tft.unloadFont();

    tft.loadFont(SegoeUIBold18TR);
    drawBtn(tft, RECT_RING_STOP, "DURDUR", COL_ERR, COL_ERR, COL_TEXT);
    drawBtn(tft, RECT_RING_SNOOZE, "5 DK ERTELE", COL_SURFACE, COL_ACCENT, COL_TEXT);
    tft.unloadFont();
}
