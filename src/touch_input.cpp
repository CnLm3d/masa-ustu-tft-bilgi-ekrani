#include "touch_input.h"
#include "config.h"
#include <Arduino.h>
#include <stdlib.h>

namespace {
bool g_touching = false;
bool g_sliderDrag = false;
int16_t g_startX = 0;
int16_t g_startY = 0;
int16_t g_lastX  = 0;
int16_t g_lastY  = 0;
int16_t g_lastDragX = -999;
uint32_t g_lastTouchMs   = 0;
uint32_t g_cooldownEndMs = 0;
} // namespace

bool touchIsPressedNow(TFT_eSPI& tft) {
    uint16_t tx = 0, ty = 0;
    return tft.getTouch(&tx, &ty, TOUCH_THRESHOLD);
}

TouchEvent touchPoll(TFT_eSPI& tft) {
    const uint32_t now = millis();
    if (!g_sliderDrag && now < g_cooldownEndMs) {
        return {TouchEventType::NONE, 0, 0};
    }

    uint16_t rx = 0, ry = 0;
    const bool down = tft.getTouch(&rx, &ry, TOUCH_THRESHOLD);

    if (down) {
        const int16_t px = constrain(static_cast<int16_t>(rx), 0, SCREEN_W - 1);
        const int16_t py = constrain(static_cast<int16_t>(ry), 0, SCREEN_H - 1);
        if (!g_touching) {
            g_touching = true;
            g_startX = px;
            g_startY = py;
            g_lastX = px;
            g_lastY = py;
            g_lastTouchMs = now;
            if (py >= (NAV_BAR_Y - SY(10))) {
                g_sliderDrag = true;
                g_lastDragX = px;
                return {TouchEventType::DRAG, px, py};
            }
        } else {
            g_lastX = px;
            g_lastY = py;
            g_lastTouchMs = now;
            if (g_sliderDrag && abs(px - g_lastDragX) >= SX(4)) {
                g_lastDragX = px;
                return {TouchEventType::DRAG, px, py};
            }
        }
        return {TouchEventType::NONE, 0, 0};
    }

    if (g_touching && (now - g_lastTouchMs) >= 45) {
        g_touching = false;
        if (g_sliderDrag) {
            g_sliderDrag = false;
            g_cooldownEndMs = now + 90;
            return {TouchEventType::DRAG_END, g_lastX, g_lastY};
        }

        g_cooldownEndMs = now + TOUCH_COOLDOWN_MS;
        const int16_t dx = g_lastX - g_startX;
        const int16_t dy = g_lastY - g_startY;
        if (abs(dx) >= SWIPE_MIN_PX && abs(dx) * 10 > abs(dy) * 13) {
            return {dx < 0 ? TouchEventType::SWIPE_LEFT : TouchEventType::SWIPE_RIGHT, g_lastX, g_lastY};
        }
        return {TouchEventType::TAP, g_lastX, g_lastY};
    }

    return {TouchEventType::NONE, 0, 0};
}
