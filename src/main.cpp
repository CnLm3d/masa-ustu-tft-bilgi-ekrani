#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "net_services.h"
#include "storage_portal.h"
#include "touch_input.h"
#include "types.h"
#include "ui_clock.h"
#include "ui_common.h"
#include "ui_news.h"
#include "ui_weather.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>

namespace {
TFT_eSPI tft;
DeviceConfig g_cfg{};
WeatherData g_wx{};
NewsData g_news{};

PageId g_page = PageId::CLOCK;
bool g_inSettings = false;
uint8_t g_newsIdx = 0;
uint32_t g_newsCycleMs = 0;
uint32_t g_lastUiPollMs = 0;
uint32_t g_lastWxMs = 0;
uint32_t g_lastNewsMs = 0;
bool g_lastFetching = false;
bool g_dirty = true;

void showSplash(const char* subText) {
    tft.fillScreen(COL_BG);
    tft.fillRoundRect(24, 52, SCREEN_W - 48, 136, 12, COL_SURFACE);
    tft.drawRoundRect(24, 52, SCREEN_W - 48, 136, 12, COL_ACCENT);

    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_ACCENT, COL_SURFACE);
    tft.drawString("ESP32 Masaüstü Bilgi Ekranı", SCREEN_W / 2, 102);
    tft.unloadFont();

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(subText, SCREEN_W / 2, 138);
    tft.unloadFont();
}

void connectWifiWithUi() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(g_cfg.ssid, g_cfg.pass);

    showSplash((String("Wi-Fi bağlanıyor: ") + g_cfg.ssid).c_str());

    const uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < WIFI_CONNECT_TIMEOUT_MS) {
        if (touchIsPressedNow(tft)) {
            delay(350);
            if (touchIsPressedNow(tft)) {
                runSetupPortal(tft, g_cfg);
            }
        }
        delay(120);
    }

    if (WiFi.status() == WL_CONNECTED) {
        showSplash((String("Bağlandı • ") + WiFi.localIP().toString()).c_str());
        delay(600);
    } else {
        showSplash("Wi-Fi bekleniyor (arka planda denenecek)");
        delay(900);
    }
}

void enterPage(PageId nextPage) {
    g_page = nextPage;
    g_inSettings = false;
    g_dirty = true;
    if (g_page == PageId::CLOCK) {
        uiClockEnter(tft, g_cfg, g_wx);
        g_dirty = false;
    } else if (g_page == PageId::NEWS) {
        g_newsCycleMs = millis();
    }
}

void stepPage(int delta) {
    const int total = static_cast<int>(PageId::COUNT);
    int next = (static_cast<int>(g_page) + delta) % total;
    if (next < 0) next += total;
    enterPage(static_cast<PageId>(next));
}

void stepNews(int delta, uint32_t now) {
    if (!g_news.valid || g_news.count == 0) return;
    int next = (static_cast<int>(g_newsIdx) + delta) % static_cast<int>(g_news.count);
    if (next < 0) next += g_news.count;
    g_newsIdx = static_cast<uint8_t>(next);
    g_newsCycleMs = now;
    g_dirty = true;
}

void handleTouchEvent(const TouchEvent& ev, uint32_t now) {
    if (ev.type == TouchEventType::NONE) return;

    if (g_inSettings) {
        if (ev.type != TouchEventType::TAP) return;
        if (RECT_SET_WIFI.contains(ev.x, ev.y)) {
            runSetupPortal(tft, g_cfg);
        } else if (RECT_SET_CAL.contains(ev.x, ev.y)) {
            runTouchCalibration(tft);
            uiSettingsDraw(tft, g_cfg);
        } else if (RECT_SET_BACK.contains(ev.x, ev.y)) {
            enterPage(g_page);
        }
        return;
    }

    if (ev.type == TouchEventType::SWIPE_LEFT) {
        if (g_page == PageId::NEWS && g_news.valid && g_news.count > 1) {
            stepNews(1, now);
        } else {
            stepPage(1);
        }
        return;
    }

    if (ev.type == TouchEventType::SWIPE_RIGHT) {
        if (g_page == PageId::NEWS && g_news.valid && g_news.count > 1) {
            stepNews(-1, now);
        } else {
            stepPage(-1);
        }
        return;
    }

    // TouchEventType::TAP
    if (RECT_TOP_SETTINGS.contains(ev.x, ev.y)) {
        g_inSettings = true;
        uiSettingsDraw(tft, g_cfg);
        return;
    }
    if (RECT_TAB_CLOCK.contains(ev.x, ev.y)) {
        enterPage(PageId::CLOCK);
        return;
    }
    if (RECT_TAB_WEATHER.contains(ev.x, ev.y)) {
        enterPage(PageId::WEATHER);
        return;
    }
    if (RECT_TAB_NEWS.contains(ev.x, ev.y)) {
        enterPage(PageId::NEWS);
        return;
    }

    if (g_page == PageId::CLOCK) {
        if (RECT_CLOCK_MIN.contains(ev.x, ev.y)) {
            uiClockTriggerDemoFlip(tft);
        } else if (RECT_CLOCK_HOUR.contains(ev.x, ev.y)) {
            netRequestTimeSync();
            netRequestWeatherRefresh();
            netRequestNewsRefresh(g_cfg.newsTopic);
        } else if (RECT_CLOCK_DATE_WX.contains(ev.x, ev.y)) {
            enterPage(PageId::WEATHER);
        }
    } else if (g_page == PageId::WEATHER) {
        if (RECT_WEATHER_REFRESH.contains(ev.x, ev.y)) {
            netRequestWeatherRefresh();
            g_dirty = true;
        }
    } else if (g_page == PageId::NEWS) {
        if (RECT_NEWS_PREV.contains(ev.x, ev.y)) {
            stepNews(-1, now);
        } else if (RECT_NEWS_NEXT.contains(ev.x, ev.y)) {
            stepNews(1, now);
        } else if (RECT_NEWS_TOPIC.contains(ev.x, ev.y)) {
            g_cfg.newsTopic = (g_cfg.newsTopic + 1) % static_cast<uint8_t>(NewsTopic::COUNT);
            saveConfig(g_cfg);
            g_newsIdx = 0;
            g_newsCycleMs = now;
            netRequestNewsRefresh(g_cfg.newsTopic);
            g_dirty = true;
        }
    }
}
} // namespace

void setup() {
    Serial.begin(115200);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(COL_BG);

    loadConfig(g_cfg);

    uint16_t cal[5] = {0};
    if (loadTouchCal(cal)) {
        tft.setTouch(cal);
    } else {
        runTouchCalibration(tft);
    }

    if (needsSetup(g_cfg)) {
        runSetupPortal(tft, g_cfg);
    }

    connectWifiWithUi();
    netInit(g_cfg);
    netStartTask();

    enterPage(PageId::CLOCK);
}

void loop() {
    const uint32_t now = millis();

    handleTouchEvent(touchPoll(tft), now);

    if ((now - g_lastUiPollMs) >= 100) {
        g_lastUiPollMs = now;
        netCopyWeather(g_wx);
        netCopyNews(g_news);
        const bool fetching = netIsFetching();

        if (g_wx.updatedAtMs != g_lastWxMs) {
            g_lastWxMs = g_wx.updatedAtMs;
            if (!g_inSettings && g_page == PageId::WEATHER) g_dirty = true;
        }
        if (g_news.updatedAtMs != g_lastNewsMs) {
            g_lastNewsMs = g_news.updatedAtMs;
            if (!g_inSettings && g_page == PageId::NEWS) g_dirty = true;
        }
        if (fetching != g_lastFetching) {
            g_lastFetching = fetching;
            if (!g_inSettings && g_page != PageId::CLOCK) g_dirty = true;
        }

        if (!g_inSettings) {
            if (g_page == PageId::CLOCK) {
                uiClockTick(tft, g_cfg, g_wx, fetching);
            } else if (g_page == PageId::NEWS && g_news.valid && g_news.count > 1) {
                if ((now - g_newsCycleMs) >= NEWS_AUTO_CYCLE_MS) {
                    g_newsCycleMs = now;
                    g_newsIdx = (g_newsIdx + 1) % g_news.count;
                    g_dirty = true;
                }
            }

            if (g_dirty) {
                g_dirty = false;
                if (g_page == PageId::WEATHER) {
                    uiWeatherDraw(tft, g_cfg, g_wx, fetching);
                } else if (g_page == PageId::NEWS) {
                    uiNewsDraw(tft, g_news, g_newsIdx, fetching);
                }
            }
        }
    }

    delay(5);
}
