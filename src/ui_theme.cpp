#include "ui_theme.h"

namespace {
bool g_lightTheme = false;

const UiPalette DARK_PALETTE = {
    0x0842, // bg: #0C0E13
    0x10A3, // surface: #14171D
    0x18E4, // cardTop: #1C2029
    0x18C4, // cardBot: #181B24
    0x2967, // border: #2D333F
    0x0000, // hinge: #000000
    0x3C9F, // accent: #3893FF
    0xFD20, // accent2: #FFA400
    0xF7BE, // text: #F5F7FA
    0x8C71, // muted: #8E95A2
    0xFFFF, // onAccent: #FFFFFF
    0x2E8C, // ok: #2DD264
    0xFD04, // warn: #FFA020
    0xF9E7, // err: #FA3E3E
    0xF629, // gold: #F2C448
    0x273F, // cyan: #24E6FF
    0xB27E, // purple: #B24CF5
    0x9924, // nightRed: #9B2620
    0x1DC9  // spotify: #1DB954
};

const UiPalette LIGHT_PALETTE = {
    0xCE79, // bg: #C8CED8 cool daylight mist (makes white cards pop on TN LCD)
    0xFFFF, // surface: #FFFFFF crisp white card
    0xEF7D, // cardTop: #EBEFF5 silver-white upper card
    0xDEDB, // cardBot: #DCDFE6 shaded lower card
    0x6B4D, // border: #6A6D74 crisp slate border
    0x2945, // hinge: #282B30 hinge slot
    0x01D8, // accent: #003BC4 deep cobalt blue
    0xA280, // accent2: #A05000 deep burnt amber
    0x0000, // text: #000000 jet black ink
    0x2965, // muted: #282C32 dark charcoal ink
    0xFFFF, // onAccent: #FFFFFF
    0x0403, // ok: #008018 deep emerald green
    0xB2A0, // warn: #B05400 deep orange
    0xB000, // err: #B00000 deep crimson red
    0x8AA0, // gold: #885400 deep bronze gold
    0x0331, // cyan: #006688 deep teal
    0x5092, // purple: #501090 deep violet
    0x9924, // nightRed: #9B2620
    0x0403  // spotify: #008018 deep green
};
} // namespace

void uiSetLightTheme(bool light) {
    g_lightTheme = light;
}

bool uiIsLightTheme() {
    return g_lightTheme;
}

const UiPalette& uiPalette() {
    return g_lightTheme ? LIGHT_PALETTE : DARK_PALETTE;
}
