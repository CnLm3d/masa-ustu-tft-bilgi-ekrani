#pragma once
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_POMO_TOGGLE = {174, 46, 134, 40};
constexpr Rect RECT_POMO_RESET  = {174, 96, 134, 40};
constexpr Rect RECT_POMO_PRESET = {174, 146, 134, 40};

struct PomodoroState {
    bool running;
    bool onBreak;
    bool longPreset; // false: 25/5, true: 50/10
    uint8_t roundCount;
    uint16_t remainingSec;
    uint16_t totalSec;
    uint32_t lastTickMs;
};

void uiPomodoroInit(PomodoroState& st);
bool uiPomodoroTick(PomodoroState& st, uint32_t nowMs);
void uiPomodoroDraw(TFT_eSPI& tft, const PomodoroState& st, bool fetching);
