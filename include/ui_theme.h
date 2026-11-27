#pragma once
#include <stdint.h>

struct UiPalette {
    uint16_t bg;
    uint16_t surface;
    uint16_t cardTop;
    uint16_t cardBot;
    uint16_t border;
    uint16_t hinge;
    uint16_t accent;
    uint16_t accent2;
    uint16_t text;
    uint16_t muted;
    uint16_t onAccent;
    uint16_t ok;
    uint16_t warn;
    uint16_t err;
    uint16_t gold;
    uint16_t cyan;
    uint16_t purple;
    uint16_t nightRed;
    uint16_t spotify;
};

void uiSetLightTheme(bool light);
bool uiIsLightTheme();
const UiPalette& uiPalette();
