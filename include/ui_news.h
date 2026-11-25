#pragma once
#include "types.h"
#include <TFT_eSPI.h>

void uiNewsDraw(TFT_eSPI& tft, const NewsData& news, uint8_t itemIndex,
                bool scrollMode, bool fetching);
