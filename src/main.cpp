#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "net_services.h"
#include "rotary_input.h"
#include "storage_portal.h"
#include "types.h"
#include "ui_clock.h"
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
uint8_t g_newsIdx = 0;
bool g_newsScrollMode = false;
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
        if (rotaryIsPressedNow()) {
            delay(250);
            if (rotaryIsPressedNow()) {
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
    g_newsScrollMode = false;
    g_dirty = true;
    if (g_page == PageId::CLOCK) {
        uiClockEnter(tft, g_cfg, g_wx);
        g_dirty = false;
    } else if (g_page == PageId::NEWS) {
        g_newsCycleMs = millis();
    }
}
} // namespace

void setup() {
    Serial.begin(115200);
    rotaryInit();

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(COL_BG);

    loadConfig(g_cfg);

    // Hold encoder button on boot or unconfigured Wi-Fi -> Setup Portal
    if (needsSetup(g_cfg) || rotaryIsPressedNow()) {
        runSetupPortal(tft, g_cfg);
    }

    connectWifiWithUi();
    netInit(g_cfg);
    netStartTask();

    enterPage(PageId::CLOCK);
}

void loop() {
    const uint32_t now = millis();

    // Handle rotary encoder rotation
    const int16_t delta = rotaryReadDelta();
    if (delta != 0) {
        if (g_page == PageId::NEWS && g_newsScrollMode && g_news.valid && g_news.count > 0) {
            int next = (static_cast<int>(g_newsIdx) + delta) % static_cast<int>(g_news.count);
            if (next < 0) next += g_news.count;
            g_newsIdx = static_cast<uint8_t>(next);
            g_newsCycleMs = now;
            g_dirty = true;
        } else {
            const int total = static_cast<int>(PageId::COUNT);
            int next = (static_cast<int>(g_page) + delta) % total;
            if (next < 0) next += total;
            enterPage(static_cast<PageId>(next));
        }
    }

    // Handle rotary encoder button
    const ButtonEvent btn = rotaryPollButton();
    if (btn == ButtonEvent::VERY_LONG_PRESS) {
        runSetupPortal(tft, g_cfg);
    } else if (btn == ButtonEvent::SHORT_PRESS) {
        if (g_page == PageId::CLOCK) {
            uiClockTriggerDemoFlip(tft);
        } else if (g_page == PageId::WEATHER) {
            netRequestWeatherRefresh();
            g_dirty = true;
        } else if (g_page == PageId::NEWS) {
            g_newsScrollMode = !g_newsScrollMode;
            g_newsCycleMs = now;
            g_dirty = true;
        }
    } else if (btn == ButtonEvent::LONG_PRESS) {
        if (g_page == PageId::CLOCK) {
            netRequestTimeSync();
            netRequestWeatherRefresh();
            netRequestNewsRefresh(g_cfg.newsTopic);
        } else if (g_page == PageId::WEATHER) {
            netRequestWeatherRefresh();
            g_dirty = true;
        } else if (g_page == PageId::NEWS) {
            g_cfg.newsTopic = (g_cfg.newsTopic + 1) % static_cast<uint8_t>(NewsTopic::COUNT);
            saveConfig(g_cfg);
            g_newsIdx = 0;
            g_newsCycleMs = now;
            netRequestNewsRefresh(g_cfg.newsTopic);
            g_dirty = true;
        }
    }

    // Sync shared data snapshots at 10 Hz
    if ((now - g_lastUiPollMs) >= 100) {
        g_lastUiPollMs = now;
        netCopyWeather(g_wx);
        netCopyNews(g_news);
        const bool fetching = netIsFetching();

        if (g_wx.updatedAtMs != g_lastWxMs) {
            g_lastWxMs = g_wx.updatedAtMs;
            if (g_page == PageId::WEATHER) g_dirty = true;
        }
        if (g_news.updatedAtMs != g_lastNewsMs) {
            g_lastNewsMs = g_news.updatedAtMs;
            if (g_page == PageId::NEWS) g_dirty = true;
        }
        if (fetching != g_lastFetching) {
            g_lastFetching = fetching;
            if (g_page != PageId::CLOCK) g_dirty = true;
        }

        if (g_page == PageId::CLOCK) {
            uiClockTick(tft, g_cfg, g_wx, fetching);
        } else if (g_page == PageId::NEWS && !g_newsScrollMode && g_news.valid && g_news.count > 1) {
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
                uiNewsDraw(tft, g_news, g_newsIdx, g_newsScrollMode, fetching);
            }
        }
    }

    delay(5);
}
