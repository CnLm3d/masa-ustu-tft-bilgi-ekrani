#include "ui_pomodoro.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "net_services.h"
#include "ui_common.h"
#include <math.h>

namespace {
void drawThickArc(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r,
                  float frac, uint16_t activeCol, uint16_t bgCol) {
    constexpr int STEPS = 72;
    const int activeSteps = static_cast<int>(frac * STEPS);
    for (int i = 0; i < STEPS; ++i) {
        const float a = (i * 2.0f * 3.1415926f / STEPS) - 1.5707963f;
        const int16_t px = cx + static_cast<int16_t>(cosf(a) * r);
        const int16_t py = cy + static_cast<int16_t>(sinf(a) * r);
        tft.fillCircle(px, py, 3, (i <= activeSteps && frac > 0.0f) ? activeCol : bgCol);
    }
}

void drawPomoBtn(TFT_eSPI& tft, const Rect& r, const char* lbl, uint16_t borderCol) {
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, COL_SURFACE);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 8, borderCol);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString(lbl, r.x + r.w / 2, r.y + r.h / 2);
}
} // namespace

void uiPomodoroInit(PomodoroState& st) {
    st.running = false;
    st.onBreak = false;
    st.longPreset = false;
    st.roundCount = 1;
    st.totalSec = 25 * 60;
    st.remainingSec = st.totalSec;
    st.lastTickMs = millis();
}

bool uiPomodoroTick(PomodoroState& st, uint32_t nowMs) {
    if (!st.running) return false;
    if ((nowMs - st.lastTickMs) < 1000) return false;
    st.lastTickMs += 1000;
    if ((nowMs - st.lastTickMs) > 2000) st.lastTickMs = nowMs;

    if (st.remainingSec > 0) {
        --st.remainingSec;
        return true;
    }

    // Switch phase automatically when timer hits 00:00
    st.onBreak = !st.onBreak;
    if (!st.onBreak) {
        st.roundCount = (st.roundCount % 4) + 1;
        st.totalSec = st.longPreset ? (50 * 60) : (25 * 60);
    } else {
        st.totalSec = st.longPreset ? (10 * 60) : (5 * 60);
    }
    st.remainingSec = st.totalSec;
    return true;
}

void uiPomodoroDraw(TFT_eSPI& tft, const PomodoroState& st, bool fetching) {
    tft.fillScreen(COL_BG);
    drawTopBar(tft, "Odak Zamanlayıcı", st.longPreset ? "50/10" : "25/5",
               netGetRssi(), netIsWifiConnected(), fetching);

    const int16_t cx = 88;
    const int16_t cy = 116;
    const uint16_t ringCol = st.onBreak ? COL_ACCENT : COL_OK;
    const float frac = (st.totalSec > 0) ? (static_cast<float>(st.remainingSec) / st.totalSec) : 0.0f;

    drawThickArc(tft, cx, cy, 62, frac, ringCol, COL_BORDER);

    char tbuf[10];
    snprintf(tbuf, sizeof(tbuf), "%02u:%02u", st.remainingSec / 60, st.remainingSec % 60);
    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(tbuf, cx, cy - 8);
    tft.unloadFont();

    char phase[20];
    snprintf(phase, sizeof(phase), "%s • %u/4", st.onBreak ? "MOLA" : "ODAK", st.roundCount);
    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ringCol, COL_BG);
    tft.drawString(phase, cx, cy + 16);

    drawPomoBtn(tft, RECT_POMO_TOGGLE, st.running ? "Duraklat" : "Başlat",
                st.running ? COL_ACCENT_2 : COL_OK);
    drawPomoBtn(tft, RECT_POMO_RESET, "Sıfırla", COL_BORDER);
    drawPomoBtn(tft, RECT_POMO_PRESET, st.longPreset ? "Mod: 50 / 10 dk" : "Mod: 25 / 5 dk", COL_ACCENT);
    tft.unloadFont();

    drawBottomNav(tft, 3);
}
