#pragma once
#include "types.h"
#include <TFT_eSPI.h>

void loadDefaultRadioStations(DeviceConfig& cfg);
void saveRadioStations(const DeviceConfig& cfg);

void loadConfig(DeviceConfig& cfg);
void saveConfig(const DeviceConfig& cfg);
bool needsSetup(const DeviceConfig& cfg);
void runSetupPortal(TFT_eSPI& tft, DeviceConfig& cfg);

bool loadTouchCal(uint16_t cal[5]);
void saveTouchCal(const uint16_t cal[5]);
void runTouchCalibration(TFT_eSPI& tft);
