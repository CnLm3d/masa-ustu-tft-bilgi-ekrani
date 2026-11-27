#pragma once
#include "types.h"

enum class WebActionType : uint8_t {
    NONE = 0,
    SWITCH_PAGE,
    APPLY_THEME,
    APPLY_UI_THEME,
    APPLY_ROTATION,
    CONFIG_UPDATED,
    RADIO_UPDATED,
    LED_UPDATED,
    REFRESH_ALL,
    START_CALIBRATION,
    START_PORTAL
};

struct WebAction {
    WebActionType type;
    uint8_t value;
};

using OtaProgressCb = void (*)(uint8_t pct, const char* msg);

void webUiStart(DeviceConfig& cfg);
void webUiStop();
void webUiSetOtaCallback(OtaProgressCb cb);
void webUiSetActivePage(PageId page);
void webUiLoop();
WebAction webUiPollAction();
