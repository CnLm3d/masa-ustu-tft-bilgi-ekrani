#pragma once
#include "config.h"
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_NEWS_PREV  = {SX(12),  SY(173), SX(94), SY(30)};
constexpr Rect RECT_NEWS_TOPIC = {SX(112), SY(173), SX(96), SY(30)};
constexpr Rect RECT_NEWS_NEXT  = {SX(214), SY(173), SX(94), SY(30)};

void uiNewsDraw(TFT_eSPI& tft, const NewsData& news, uint8_t itemIndex, bool fetching);
