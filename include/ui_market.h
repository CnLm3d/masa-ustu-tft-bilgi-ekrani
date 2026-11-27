#pragma once
#include "types.h"
#include <TFT_eSPI.h>

void uiMarketDraw(TFT_eSPI& tft, const MarketData& mk, uint8_t focusRow, bool fetching);
void uiMarketUpdateHighlight(TFT_eSPI& tft, const MarketData& mk, uint8_t prevRow, uint8_t newRow);
