#pragma once
#include "types.h"
#include <stdint.h>

enum class BtMediaCmd : uint8_t {
    PLAY_PAUSE = 0,
    NEXT,
    PREV
};

bool audioBtSupported();
bool audioBtStart(const char* deviceName, uint8_t volume);
void audioBtStop();
bool audioBtIsActive();
void audioBtSetVolume(uint8_t volume);
void audioBtSendCmd(BtMediaCmd cmd);
bool audioBtCopyState(MusicState& out);
void audioPlayChime();
