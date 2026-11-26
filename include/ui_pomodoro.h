#pragma once
#include "config.h"
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_POMO_TOGGLE = {SX(174), SY(46),  SX(134), SY(40)};
constexpr Rect RECT_POMO_RESET  = {SX(174), SY(96),  SX(134), SY(40)};
constexpr Rect RECT_POMO_PRESET = {SX(174), SY(146), SX(134), SY(40)};

struct PomodoroState {
    bool running;
    bool onBreak;
    bool longPreset;
    uint8_t roundCount;
    uint16_t remainingSec;
    uint16_t totalSec;
    uint32_t lastTickMs;
};

void uiPomodoroInit(PomodoroState& st);
bool uiPomodoroTick(PomodoroState& st, uint32_t nowMs);
void uiPomodoroDraw(TFT_eSPI& tft, const PomodoroState& st, bool fetching);
