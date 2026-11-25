#pragma once
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_ALARM_TOGGLE[3] = {
    {228, 34, 80, 50},
    {228, 92, 80, 50},
    {228, 150, 80, 50}
};

constexpr Rect RECT_ALARM_EDIT[3] = {
    {12, 34, 212, 50},
    {12, 92, 212, 50},
    {12, 150, 212, 50}
};

// Inline edit mode hit rects (when editIndex >= 0)
constexpr Rect RECT_AEDIT_H_DEC = {24,  76, 56, 36};
constexpr Rect RECT_AEDIT_H_INC = {92,  76, 56, 36};
constexpr Rect RECT_AEDIT_M_DEC = {172, 76, 56, 36};
constexpr Rect RECT_AEDIT_M_INC = {240, 76, 56, 36};
constexpr Rect RECT_AEDIT_REP   = {24,  126, 156, 38};
constexpr Rect RECT_AEDIT_DONE  = {192, 126, 104, 38};

// Ringing screen hit rects
constexpr Rect RECT_RING_STOP   = {20, 150, 132, 48};
constexpr Rect RECT_RING_SNOOZE = {168, 150, 132, 48};

void uiAlarmDraw(TFT_eSPI& tft, const DeviceConfig& cfg, int8_t editIdx, bool fetching);
void uiAlarmRingingDraw(TFT_eSPI& tft, const AlarmItem& alarm, bool invertPhase);
