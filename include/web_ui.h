#pragma once
#include "types.h"

enum class WebActionType : uint8_t {
    NONE = 0,
    SWITCH_PAGE,
    APPLY_THEME,
    APPLY_ROTATION,
    CONFIG_UPDATED,
    REFRESH_ALL,
    START_CALIBRATION,
    START_PORTAL
};

struct WebAction {
    WebActionType type;
    uint8_t value;
};

void webUiStart(DeviceConfig& cfg);
void webUiSetActivePage(PageId page);
void webUiLoop();
WebAction webUiPollAction();
