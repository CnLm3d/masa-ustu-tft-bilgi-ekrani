#include "rotary_input.h"
#include "config.h"
#include <Arduino.h>

namespace {
portMUX_TYPE g_encMux = portMUX_INITIALIZER_UNLOCKED;
volatile int32_t g_rawSteps = 0;
volatile uint8_t g_prevState = 0;

// Quadrature state transition table: index = (prev << 2) | curr
constexpr int8_t ENC_TABLE[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

bool g_btnLastRaw = true;
bool g_btnStable = true;
uint32_t g_btnChangeMs = 0;
uint32_t g_btnDownMs = 0;
bool g_longFired = false;
bool g_veryLongFired = false;

void IRAM_ATTR onEncoderISR() {
    const uint8_t clk = static_cast<uint8_t>(digitalRead(PIN_ENC_CLK));
    const uint8_t dt  = static_cast<uint8_t>(digitalRead(PIN_ENC_DT));
    const uint8_t curr = (clk << 1) | dt;

    portENTER_CRITICAL_ISR(&g_encMux);
    const uint8_t idx = ((g_prevState & 0x03) << 2) | curr;
    g_rawSteps += ENC_TABLE[idx];
    g_prevState = curr;
    portEXIT_CRITICAL_ISR(&g_encMux);
}
} // namespace

void rotaryInit() {
    pinMode(PIN_ENC_CLK, INPUT_PULLUP);
    pinMode(PIN_ENC_DT, INPUT_PULLUP);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);

    g_prevState = (static_cast<uint8_t>(digitalRead(PIN_ENC_CLK)) << 1) |
                  static_cast<uint8_t>(digitalRead(PIN_ENC_DT));
    g_btnLastRaw = digitalRead(PIN_ENC_SW);
    g_btnStable = g_btnLastRaw;

    attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), onEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_DT), onEncoderISR, CHANGE);
}

int16_t rotaryReadDelta() {
    int16_t detents = 0;
    portENTER_CRITICAL(&g_encMux);
    detents = static_cast<int16_t>(g_rawSteps / 4);
    g_rawSteps -= static_cast<int32_t>(detents) * 4;
    portEXIT_CRITICAL(&g_encMux);
    return detents;
}

bool rotaryIsPressedNow() {
    return digitalRead(PIN_ENC_SW) == LOW;
}

ButtonEvent rotaryPollButton() {
    const uint32_t now = millis();
    const bool raw = digitalRead(PIN_ENC_SW);

    if (raw != g_btnLastRaw) {
        g_btnLastRaw = raw;
        g_btnChangeMs = now;
    }

    if ((now - g_btnChangeMs) < 25) {
        return ButtonEvent::NONE;
    }

    if (raw != g_btnStable) {
        g_btnStable = raw;
        if (!g_btnStable) {
            // Pressed (LOW)
            g_btnDownMs = now;
            g_longFired = false;
            g_veryLongFired = false;
        } else {
            // Released (HIGH)
            if (!g_longFired && !g_veryLongFired && (now - g_btnDownMs) >= 30) {
                return ButtonEvent::SHORT_PRESS;
            }
        }
    }

    if (!g_btnStable) {
        const uint32_t held = now - g_btnDownMs;
        if (!g_veryLongFired && held >= 4000) {
            g_veryLongFired = true;
            g_longFired = true;
            return ButtonEvent::VERY_LONG_PRESS;
        }
        if (!g_longFired && held >= 1100) {
            g_longFired = true;
            return ButtonEvent::LONG_PRESS;
        }
    }

    return ButtonEvent::NONE;
}
