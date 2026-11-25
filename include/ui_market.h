#pragma once
#include "types.h"
#include <TFT_eSPI.h>

void uiMarketDraw(TFT_eSPI& tft, const MarketData& mk, bool fetching);
