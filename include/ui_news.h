#pragma once
#include "touch_input.h"
#include "types.h"
#include <TFT_eSPI.h>

constexpr Rect RECT_NEWS_PREV  = {12, 173, 94, 30};
constexpr Rect RECT_NEWS_TOPIC = {112, 173, 96, 30};
constexpr Rect RECT_NEWS_NEXT  = {214, 173, 94, 30};

void uiNewsDraw(TFT_eSPI& tft, const NewsData& news, uint8_t itemIndex, bool fetching);
