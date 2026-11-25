#pragma once
#include <stdint.h>

enum class ButtonEvent : uint8_t {
    NONE = 0,
    SHORT_PRESS,
    LONG_PRESS,      // >= 1100 ms
    VERY_LONG_PRESS  // >= 4000 ms
};

void rotaryInit();
int16_t rotaryReadDelta();
ButtonEvent rotaryPollButton();
bool rotaryIsPressedNow();
