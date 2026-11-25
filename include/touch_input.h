#pragma once
#include <TFT_eSPI.h>
#include <stdint.h>

enum class TouchEventType : uint8_t {
    NONE = 0,
    TAP,
    SWIPE_LEFT,
    SWIPE_RIGHT
};

struct TouchEvent {
    TouchEventType type;
    int16_t x;
    int16_t y;
};

struct Rect {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;

    constexpr bool contains(int16_t px, int16_t py) const {
        return px >= x && px < (x + w) && py >= y && py < (y + h);
    }
};

TouchEvent touchPoll(TFT_eSPI& tft);
bool touchIsPressedNow(TFT_eSPI& tft);
