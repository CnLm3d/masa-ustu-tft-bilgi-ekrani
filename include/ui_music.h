#pragma once
#include "config.h"
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_MUS_MODE_BT   = {SX(12),  SY(30), SX(144), SY(26)};
constexpr Rect RECT_MUS_MODE_WIFI = {SX(164), SY(30), SX(144), SY(26)};

constexpr Rect RECT_MUS_PREV  = {SX(12),  SY(160), SX(50), SY(40)};
constexpr Rect RECT_MUS_PLAY  = {SX(68),  SY(160), SX(76), SY(40)};
constexpr Rect RECT_MUS_NEXT  = {SX(150), SY(160), SX(50), SY(40)};
constexpr Rect RECT_MUS_VOLDN = {SX(210), SY(160), SX(44), SY(40)};
constexpr Rect RECT_MUS_VOLUP = {SX(264), SY(160), SX(44), SY(40)};

void uiMusicDraw(TFT_eSPI& tft, const MusicState& st, bool btSupported, bool fetching);
void uiMusicDrawMeter(TFT_eSPI& tft, const MusicState& st);
