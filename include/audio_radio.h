#pragma once
#include "types.h"
#include <stdint.h>

void radioInit(const DeviceConfig& cfg);
void radioUpdateStations(const DeviceConfig& cfg);
void radioStartTask();

void radioPlay(uint8_t idx);
void radioStop();
void radioToggle();
void radioNext();
void radioPrev();
void radioSetVolume(uint8_t vol);

bool radioIsPlaying();
bool radioCopyState(RadioState& out);
void audioPlayChime();
