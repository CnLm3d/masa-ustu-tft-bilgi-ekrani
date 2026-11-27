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
    0xF7BE, // bg: #F3F6FA
    0xFFFF, // surface: #FFFFFF
    0xEF7D, // cardTop: #EBEFF5
    0xE73C, // cardBot: #E3E7EF
    0xC65A, // border: #C5CBD4
    0x9CF3, // hinge: #9CA0A8
    0x1B9F, // accent: #1B72FF
    0xDC60, // accent2: #DB8C00
    0x10A3, // text: #12171F
    0x5AEB, // muted: #585D66
    0xFFFF, // onAccent: #FFFFFF
    0x1548, // ok: #14A842
    0xDC40, // warn: #DB8800
    0xD945, // err: #DA2828
    0xB440, // gold: #B48800
    0x04D5, // cyan: #009AA8
    0x79BA, // purple: #7A34D0
    0x9924, // nightRed: #9B2620
    0x1548  // spotify: #14A842
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
