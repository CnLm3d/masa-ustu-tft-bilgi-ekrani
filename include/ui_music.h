#pragma once
#include "config.h"
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_MUS_STATION = {SX(12), SY(30), SX(196), SY(26)};

constexpr Rect RECT_MUS_PREV  = {SX(12),  SY(162), SX(52), SY(40)};
constexpr Rect RECT_MUS_PLAY  = {SX(68),  SY(162), SX(72), SY(40)};
constexpr Rect RECT_MUS_NEXT  = {SX(144), SY(162), SX(52), SY(40)};
constexpr Rect RECT_MUS_VOLDN = {SX(202), SY(162), SX(51), SY(40)};
constexpr Rect RECT_MUS_VOLUP = {SX(257), SY(162), SX(51), SY(40)};

void uiMusicDraw(TFT_eSPI& tft, const RadioState& st, bool fetching);
void uiMusicDrawMeter(TFT_eSPI& tft, const RadioState& st);
