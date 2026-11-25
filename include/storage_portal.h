#pragma once
#include "types.h"
#include <TFT_eSPI.h>

void loadConfig(DeviceConfig& cfg);
void saveConfig(const DeviceConfig& cfg);
bool needsSetup(const DeviceConfig& cfg);
void runSetupPortal(TFT_eSPI& tft, DeviceConfig& cfg);
