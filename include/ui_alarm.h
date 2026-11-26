#pragma once
#include "config.h"
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_ALARM_TOGGLE[3] = {
    {SX(228), SY(34),  SX(80), SY(50)},
    {SX(228), SY(92),  SX(80), SY(50)},
    {SX(228), SY(150), SX(80), SY(50)}
};

constexpr Rect RECT_ALARM_EDIT[3] = {
    {SX(12), SY(34),  SX(212), SY(50)},
    {SX(12), SY(92),  SX(212), SY(50)},
    {SX(12), SY(150), SX(212), SY(50)}
};

constexpr Rect RECT_AEDIT_H_DEC = {SX(24),  SY(76),  SX(56),  SY(36)};
constexpr Rect RECT_AEDIT_H_INC = {SX(92),  SY(76),  SX(56),  SY(36)};
constexpr Rect RECT_AEDIT_M_DEC = {SX(172), SY(76),  SX(56),  SY(36)};
constexpr Rect RECT_AEDIT_M_INC = {SX(240), SY(76),  SX(56),  SY(36)};
constexpr Rect RECT_AEDIT_REP   = {SX(24),  SY(126), SX(156), SY(38)};
constexpr Rect RECT_AEDIT_DONE  = {SX(192), SY(126), SX(104), SY(38)};

constexpr Rect RECT_RING_STOP   = {SX(20),  SY(150), SX(132), SY(48)};
constexpr Rect RECT_RING_SNOOZE = {SX(168), SY(150), SX(132), SY(48)};

void uiAlarmDraw(TFT_eSPI& tft, const DeviceConfig& cfg, int8_t editIdx, bool fetching);
void uiAlarmRingingDraw(TFT_eSPI& tft, const AlarmItem& alarm, bool invertPhase);
