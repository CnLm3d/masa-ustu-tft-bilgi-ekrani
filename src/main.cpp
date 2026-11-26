#include "audio_bt.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"
#include "net_services.h"
#include "storage_portal.h"
#include "touch_input.h"
#include "types.h"
#include "ui_alarm.h"
#include "ui_clock.h"
#include "ui_common.h"
#include "ui_market.h"
#include "ui_music.h"
#include "ui_news.h"
#include "ui_pomodoro.h"
#include "ui_weather.h"
#include "ui_world.h"
#include "web_ui.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>

namespace {
TFT_eSPI tft;
DeviceConfig g_cfg{};
WeatherData g_wx{};
NewsData g_news{};
MarketData g_mk{};
PomodoroState g_pomo{};
MusicState g_music{};

PageId g_page = PageId::CLOCK;
int8_t g_alarmEditIdx = -1;
uint8_t g_newsIdx = 0;
uint32_t g_newsCycleMs = 0;
uint32_t g_lastUiPollMs = 0;
uint32_t g_lastWxMs = 0;
uint32_t g_lastNewsMs = 0;
uint32_t g_lastMkMs = 0;
uint32_t g_lastSpotifyPollMs = 0;
uint32_t g_lastMusicUpdatedMs = 0;
uint32_t g_lastChimeMs = 0;
int g_lastWorldMin = -1;
int g_lastNightMin = -1;
bool g_lastFetching = false;
bool g_dirty = true;
bool g_webStarted = false;
bool g_btSpeakerMode = false;

// Alarm ringing & snooze state
int8_t g_ringingIdx = -1;
int g_lastAlarmCheckMin = -1;
int8_t g_snoozeIdx = -1;
uint32_t g_snoozeUntilMs = 0;
uint32_t g_lastRingFlashMs = 0;
bool g_ringInvert = false;

// Night mode wake override (touch wakes for 2 minutes)
uint32_t g_nightWakeUntilMs = 0;

void showSplash(const char* subText) {
    tft.fillScreen(COL_BG);
    tft.fillRoundRect(SX(24), SY(52), SCREEN_W - SX(48), SY(136), SR(12), COL_SURFACE);
    tft.drawRoundRect(SX(24), SY(52), SCREEN_W - SX(48), SY(136), SR(12), COL_ACCENT);

    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_ACCENT, COL_SURFACE);
    tft.drawString("ESP32 DeskHub v3.0", SCREEN_W / 2, SY(102));
    tft.unloadFont();

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString(subText, SCREEN_W / 2, SY(138));
    tft.unloadFont();
}

void switchToBtMode() {
    if (g_btSpeakerMode || !audioBtSupported()) return;
    g_btSpeakerMode = true;
    g_music.btActive = true;
    g_music.sourceMode = 0;
    g_music.volume = g_cfg.volume;
    strlcpy(g_music.statusMsg, "Bluetooth baslatiliyor...", sizeof(g_music.statusMsg));
    if (g_page == PageId::MUSIC) {
        uiMusicDraw(tft, g_music, true, false);
    }

    webUiStop();
    g_webStarted = false;
    netPause(true);
    WiFi.disconnect(false);
    WiFi.mode(WIFI_OFF);
    delay(80);

    audioBtStart(g_cfg.btName, g_cfg.volume);
    audioBtCopyState(g_music);
    g_dirty = true;
}

void switchToWifiMode() {
    if (!g_btSpeakerMode) return;
    g_btSpeakerMode = false;
    g_music.btActive = false;
    g_music.btConnected = false;
    g_music.sourceMode = 1;
    g_music.volume = g_cfg.volume;
    strlcpy(g_music.statusMsg, "Wi-Fi yeniden baglaniyor...", sizeof(g_music.statusMsg));
    if (g_page == PageId::MUSIC) {
        uiMusicDraw(tft, g_music, audioBtSupported(), true);
    }

    audioBtStop();
    delay(80);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(g_cfg.ssid, g_cfg.pass);
    netPause(false);
    netRequestSpotifyRefresh();
    g_lastSpotifyPollMs = millis();
    g_dirty = true;
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
        showSplash((String("deskhub.local • ") + WiFi.localIP().toString()).c_str());
        delay(800);
    } else {
        showSplash("Wi-Fi bekleniyor (arka planda denenecek)");
        delay(900);
    }
}

void enterPage(PageId nextPage) {
    g_page = nextPage;
    g_alarmEditIdx = -1;
    g_dirty = true;
    webUiSetActivePage(g_page);
    if (g_page == PageId::CLOCK) {
        uiClockEnter(tft, g_cfg, g_wx);
        g_dirty = false;
    } else if (g_page == PageId::NEWS) {
        g_newsCycleMs = millis();
    } else if (g_page == PageId::WORLD) {
        g_lastWorldMin = -1;
    } else if (g_page == PageId::NIGHT) {
        g_lastNightMin = -1;
    } else if (g_page == PageId::MUSIC) {
        g_music.volume = g_cfg.volume;
        g_music.btActive = g_btSpeakerMode;
        g_music.sourceMode = g_btSpeakerMode ? 0 : 1;
        if (!g_btSpeakerMode) {
            g_lastSpotifyPollMs = millis();
            netRequestSpotifyRefresh();
        }
    }
}

void stepNews(int delta, uint32_t now) {
    if (!g_news.valid || g_news.count == 0) return;
    int next = (static_cast<int>(g_newsIdx) + delta) % static_cast<int>(g_news.count);
    if (next < 0) next += g_news.count;
    g_newsIdx = static_cast<uint8_t>(next);
    g_newsCycleMs = now;
    g_dirty = true;
}

bool alarmMatchesDay(uint8_t repeatMode, int wday) {
    if (repeatMode == 1) return wday >= 1 && wday <= 5;
    if (repeatMode == 2) return wday == 0 || wday == 6;
    return true;
}

void stopBuzzer() {
    if (g_cfg.buzzerPin > 0) {
        noTone(g_cfg.buzzerPin);
    }
}

void checkAlarmsAndNight(uint32_t now) {
    if (g_snoozeIdx >= 0 && now >= g_snoozeUntilMs) {
        g_ringingIdx = g_snoozeIdx;
        g_snoozeIdx = -1;
        g_ringInvert = false;
        uiAlarmRingingDraw(tft, g_cfg.alarms[g_ringingIdx], false);
        return;
    }

    struct tm ti{};
    if (!getLocalTime(&ti, 2)) return;

    const int curMinKey = ti.tm_hour * 60 + ti.tm_min;
    if (curMinKey != g_lastAlarmCheckMin) {
        g_lastAlarmCheckMin = curMinKey;
        for (int8_t i = 0; i < 3; ++i) {
            const AlarmItem& a = g_cfg.alarms[i];
            if (a.enabled && a.hour == ti.tm_hour && a.minute == ti.tm_min &&
                alarmMatchesDay(a.repeatMode, ti.tm_wday)) {
                g_ringingIdx = i;
                g_ringInvert = false;
                uiAlarmRingingDraw(tft, a, false);
                return;
            }
        }
    }

    // Auto Night Mode check (when idle on Clock)
    if (g_cfg.nightEnabled && g_ringingIdx < 0 && now >= g_nightWakeUntilMs) {
        const uint8_t h = static_cast<uint8_t>(ti.tm_hour);
        const uint8_t s = g_cfg.nightStartHour;
        const uint8_t e = g_cfg.nightEndHour;
        const bool inNight = (s < e) ? (h >= s && h < e) : (h >= s || h < e);
        if (inNight && g_page == PageId::CLOCK) {
            enterPage(PageId::NIGHT);
        } else if (!inNight && g_page == PageId::NIGHT) {
            enterPage(PageId::CLOCK);
        }
    }
}

void handleTouchEvent(const TouchEvent& ev, uint32_t now) {
    if (ev.type == TouchEventType::NONE) return;

    // 1) Ringing alarm has highest priority
    if (g_ringingIdx >= 0) {
        if (ev.type == TouchEventType::TAP) {
            if (RECT_RING_STOP.contains(ev.x, ev.y)) {
                stopBuzzer();
                g_ringingIdx = -1;
                enterPage(g_page);
            } else if (RECT_RING_SNOOZE.contains(ev.x, ev.y)) {
                stopBuzzer();
                g_snoozeIdx = g_ringingIdx;
                g_snoozeUntilMs = now + 5UL * 60UL * 1000UL;
                g_ringingIdx = -1;
                enterPage(g_page);
            }
        }
        return;
    }

    // 2) Night Mode wakes up on any tap for 2 minutes
    if (g_page == PageId::NIGHT) {
        g_nightWakeUntilMs = now + 120000UL;
        enterPage(PageId::CLOCK);
        return;
    }

    // 3) Settings sub-page
    if (g_page == PageId::SETTINGS) {
        if (ev.type != TouchEventType::TAP) return;
        if (RECT_SET_WIFI.contains(ev.x, ev.y)) {
            if (g_btSpeakerMode) switchToWifiMode();
            runSetupPortal(tft, g_cfg);
        } else if (RECT_SET_CAL.contains(ev.x, ev.y)) {
            runTouchCalibration(tft);
            uiSettingsDraw(tft, g_cfg);
        } else if (RECT_SET_BACK.contains(ev.x, ev.y)) {
            enterPage(PageId::MENU);
        }
        return;
    }

    // 4) Horizontal swipes
    if (ev.type == TouchEventType::SWIPE_LEFT) {
        if (g_page == PageId::NEWS && g_news.valid && g_news.count > 1) stepNews(1, now);
        else if (g_page == PageId::CLOCK) enterPage(PageId::MUSIC);
        else if (g_page == PageId::MUSIC) enterPage(PageId::MARKET);
        else if (g_page == PageId::MARKET) enterPage(PageId::MENU);
        else enterPage(PageId::CLOCK);
        return;
    }
    if (ev.type == TouchEventType::SWIPE_RIGHT) {
        if (g_page == PageId::NEWS && g_news.valid && g_news.count > 1) stepNews(-1, now);
        else if (g_page == PageId::MENU) enterPage(PageId::MARKET);
        else if (g_page == PageId::MARKET) enterPage(PageId::MUSIC);
        else if (g_page == PageId::MUSIC) enterPage(PageId::CLOCK);
        else enterPage(PageId::MENU);
        return;
    }

    // 5) Shared Bottom 4-Tab Nav Bar
    if (RECT_NAV_CLOCK.contains(ev.x, ev.y))  { enterPage(PageId::CLOCK);  return; }
    if (RECT_NAV_MUSIC.contains(ev.x, ev.y))  { enterPage(PageId::MUSIC);  return; }
    if (RECT_NAV_MARKET.contains(ev.x, ev.y)) { enterPage(PageId::MARKET); return; }
    if (RECT_NAV_MENU.contains(ev.x, ev.y))   { enterPage(PageId::MENU);   return; }

    // 6) Top bar contextual right pill
    if (RECT_TOP_PILL.contains(ev.x, ev.y)) {
        if (g_page == PageId::CLOCK) {
            g_cfg.clockTheme = (g_cfg.clockTheme + 1) % static_cast<uint8_t>(ClockTheme::COUNT);
            saveConfig(g_cfg);
            enterPage(PageId::CLOCK);
        } else if (g_page == PageId::WEATHER) {
            netRequestWeatherRefresh();
            g_dirty = true;
        } else if (g_page == PageId::MARKET) {
            netRequestMarketRefresh();
            g_dirty = true;
        } else if (g_page == PageId::NEWS) {
            netRequestNewsRefresh(g_cfg.newsTopic);
            g_dirty = true;
        } else if (g_page == PageId::POMODORO) {
            g_pomo.longPreset = !g_pomo.longPreset;
            g_pomo.running = false;
            g_pomo.onBreak = false;
            g_pomo.totalSec = g_pomo.longPreset ? (50 * 60) : (25 * 60);
            g_pomo.remainingSec = g_pomo.totalSec;
            g_dirty = true;
        } else if (g_page == PageId::MUSIC) {
            if (g_btSpeakerMode) switchToWifiMode();
            else switchToBtMode();
        } else {
            enterPage(PageId::SETTINGS);
        }
        return;
    }

    // 7) Page-specific touch actions
    if (g_page == PageId::MENU) {
        const PageId targets[8] = {
            PageId::CLOCK, PageId::WEATHER, PageId::MARKET, PageId::WORLD,
            PageId::ALARM, PageId::NEWS, PageId::POMODORO, PageId::MUSIC
        };
        for (int i = 0; i < 8; ++i) {
            if (RECT_MENU_TILES[i].contains(ev.x, ev.y)) {
                enterPage(targets[i]);
                return;
            }
        }
    } else if (g_page == PageId::CLOCK) {
        if (ev.y >= SY(170) && ev.y < NAV_BAR_Y) {
            enterPage(PageId::WEATHER);
        } else if (ev.x >= SX(160)) {
            uiClockTriggerDemo(tft, g_cfg, g_wx);
        } else {
            netRequestTimeSync();
            netRequestWeatherRefresh();
            netRequestMarketRefresh();
        }
    } else if (g_page == PageId::MUSIC) {
        if (RECT_MUS_MODE_BT.contains(ev.x, ev.y)) {
            switchToBtMode();
        } else if (RECT_MUS_MODE_WIFI.contains(ev.x, ev.y)) {
            switchToWifiMode();
        } else if (RECT_MUS_PREV.contains(ev.x, ev.y)) {
            if (g_btSpeakerMode) audioBtSendCmd(BtMediaCmd::PREV);
            else netSendSpotifyCmd(SpotifyCmd::PREV, g_cfg.volume);
        } else if (RECT_MUS_PLAY.contains(ev.x, ev.y)) {
            if (g_btSpeakerMode) {
                audioBtSendCmd(BtMediaCmd::PLAY_PAUSE);
            } else {
                g_music.isPlaying = !g_music.isPlaying;
                netSendSpotifyCmd(SpotifyCmd::PLAY_PAUSE, g_cfg.volume);
            }
            g_dirty = true;
        } else if (RECT_MUS_NEXT.contains(ev.x, ev.y)) {
            if (g_btSpeakerMode) audioBtSendCmd(BtMediaCmd::NEXT);
            else netSendSpotifyCmd(SpotifyCmd::NEXT, g_cfg.volume);
        } else if (RECT_MUS_VOLDN.contains(ev.x, ev.y)) {
            g_cfg.volume = (g_cfg.volume >= 10) ? static_cast<uint8_t>(g_cfg.volume - 10) : 0;
            g_music.volume = g_cfg.volume;
            saveConfig(g_cfg);
            if (g_btSpeakerMode) audioBtSetVolume(g_cfg.volume);
            else netSendSpotifyCmd(SpotifyCmd::SET_VOLUME, g_cfg.volume);
            g_dirty = true;
        } else if (RECT_MUS_VOLUP.contains(ev.x, ev.y)) {
            g_cfg.volume = (g_cfg.volume <= 90) ? static_cast<uint8_t>(g_cfg.volume + 10) : 100;
            g_music.volume = g_cfg.volume;
            saveConfig(g_cfg);
            if (g_btSpeakerMode) audioBtSetVolume(g_cfg.volume);
            else netSendSpotifyCmd(SpotifyCmd::SET_VOLUME, g_cfg.volume);
            g_dirty = true;
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
    } else if (g_page == PageId::ALARM) {
        if (g_alarmEditIdx >= 0 && g_alarmEditIdx < 3) {
            AlarmItem& a = g_cfg.alarms[g_alarmEditIdx];
            if (RECT_AEDIT_H_DEC.contains(ev.x, ev.y)) a.hour = (a.hour + 23) % 24;
            else if (RECT_AEDIT_H_INC.contains(ev.x, ev.y)) a.hour = (a.hour + 1) % 24;
            else if (RECT_AEDIT_M_DEC.contains(ev.x, ev.y)) a.minute = (a.minute + 55) % 60;
            else if (RECT_AEDIT_M_INC.contains(ev.x, ev.y)) a.minute = (a.minute + 5) % 60;
            else if (RECT_AEDIT_REP.contains(ev.x, ev.y)) a.repeatMode = (a.repeatMode + 1) % 3;
            else if (RECT_AEDIT_DONE.contains(ev.x, ev.y)) {
                saveConfig(g_cfg);
                g_alarmEditIdx = -1;
            }
            g_dirty = true;
        } else {
            for (int i = 0; i < 3; ++i) {
                if (RECT_ALARM_TOGGLE[i].contains(ev.x, ev.y)) {
                    g_cfg.alarms[i].enabled = !g_cfg.alarms[i].enabled;
                    saveConfig(g_cfg);
                    g_dirty = true;
                    return;
                }
                if (RECT_ALARM_EDIT[i].contains(ev.x, ev.y)) {
                    g_alarmEditIdx = i;
                    g_dirty = true;
                    return;
                }
            }
        }
    } else if (g_page == PageId::POMODORO) {
        if (RECT_POMO_TOGGLE.contains(ev.x, ev.y)) {
            g_pomo.running = !g_pomo.running;
            g_pomo.lastTickMs = now;
            g_dirty = true;
        } else if (RECT_POMO_RESET.contains(ev.x, ev.y)) {
            g_pomo.running = false;
            g_pomo.onBreak = false;
            g_pomo.totalSec = g_pomo.longPreset ? (50 * 60) : (25 * 60);
            g_pomo.remainingSec = g_pomo.totalSec;
            g_dirty = true;
        } else if (RECT_POMO_PRESET.contains(ev.x, ev.y)) {
            g_pomo.longPreset = !g_pomo.longPreset;
            g_pomo.running = false;
            g_pomo.onBreak = false;
            g_pomo.totalSec = g_pomo.longPreset ? (50 * 60) : (25 * 60);
            g_pomo.remainingSec = g_pomo.totalSec;
            g_dirty = true;
        }
    }
}

void handleWebAction(const WebAction& act) {
    if (act.type == WebActionType::NONE) return;
    if (act.type == WebActionType::SWITCH_PAGE) {
        enterPage(static_cast<PageId>(act.value));
    } else if (act.type == WebActionType::APPLY_THEME) {
        if (g_page == PageId::CLOCK) enterPage(PageId::CLOCK);
    } else if (act.type == WebActionType::APPLY_ROTATION) {
        tft.setRotation(g_cfg.rotation);
        enterPage(g_page);
    } else if (act.type == WebActionType::CONFIG_UPDATED) {
        g_dirty = true;
        if (g_page == PageId::CLOCK) enterPage(PageId::CLOCK);
    } else if (act.type == WebActionType::REFRESH_ALL) {
        netRequestTimeSync();
        netRequestWeatherRefresh();
        netRequestMarketRefresh();
        netRequestNewsRefresh(g_cfg.newsTopic);
        g_dirty = true;
    } else if (act.type == WebActionType::START_CALIBRATION) {
        runTouchCalibration(tft);
        enterPage(g_page);
    } else if (act.type == WebActionType::START_PORTAL) {
        runSetupPortal(tft, g_cfg);
    }
}
} // namespace

void setup() {
    Serial.begin(115200);
    loadConfig(g_cfg);

    tft.init();
    tft.setRotation(g_cfg.rotation);
    tft.fillScreen(COL_BG);

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
    uiPomodoroInit(g_pomo);

    if (WiFi.status() == WL_CONNECTED) {
        webUiStart(g_cfg);
        g_webStarted = true;
    }

    enterPage(PageId::CLOCK);
}

void loop() {
    const uint32_t now = millis();

    if (!g_webStarted && !g_btSpeakerMode && WiFi.status() == WL_CONNECTED) {
        webUiStart(g_cfg);
        g_webStarted = true;
    }
    webUiLoop();
    handleWebAction(webUiPollAction());

    handleTouchEvent(touchPoll(tft), now);

    // Alarm ringing flash, PCM5102A chime & optional buzzer pulse
    if (g_ringingIdx >= 0) {
        if ((now - g_lastRingFlashMs) >= 550) {
            g_lastRingFlashMs = now;
            g_ringInvert = !g_ringInvert;
            uiAlarmRingingDraw(tft, g_cfg.alarms[g_ringingIdx], g_ringInvert);
            if (g_cfg.buzzerPin > 0 && g_ringInvert) {
                tone(g_cfg.buzzerPin, 2200, 200);
            }
            if (!g_btSpeakerMode && (now - g_lastChimeMs) >= 1800) {
                g_lastChimeMs = now;
                audioPlayChime();
            }
        }
        delay(5);
        return;
    }

    if (uiPomodoroTick(g_pomo, now) && g_page == PageId::POMODORO) {
        g_dirty = true;
    }

    if ((now - g_lastUiPollMs) >= 100) {
        g_lastUiPollMs = now;
        netCopyWeather(g_wx);
        netCopyNews(g_news);
        netCopyMarket(g_mk);
        if (g_btSpeakerMode) {
            audioBtCopyState(g_music);
        } else {
            netCopySpotify(g_music);
            g_music.volume = g_cfg.volume;
            g_music.btActive = false;
        }
        const bool fetching = netIsFetching();

        checkAlarmsAndNight(now);

        if (g_wx.updatedAtMs != g_lastWxMs) {
            g_lastWxMs = g_wx.updatedAtMs;
            if (g_page == PageId::WEATHER) g_dirty = true;
        }
        if (g_news.updatedAtMs != g_lastNewsMs) {
            g_lastNewsMs = g_news.updatedAtMs;
            if (g_page == PageId::NEWS) g_dirty = true;
        }
        if (g_mk.updatedAtMs != g_lastMkMs) {
            g_lastMkMs = g_mk.updatedAtMs;
            if (g_page == PageId::MARKET) g_dirty = true;
        }
        if (g_music.updatedAtMs != g_lastMusicUpdatedMs) {
            g_lastMusicUpdatedMs = g_music.updatedAtMs;
            if (g_page == PageId::MUSIC) g_dirty = true;
        }
        if (fetching != g_lastFetching) {
            g_lastFetching = fetching;
            if (g_page != PageId::CLOCK && g_page != PageId::NIGHT && g_page != PageId::SETTINGS) {
                g_dirty = true;
            }
        }

        if (g_page == PageId::CLOCK) {
            uiClockTick(tft, g_cfg, g_wx, fetching);
        } else if (g_page == PageId::WORLD) {
            struct tm ti{};
            if (getLocalTime(&ti, 2) && ti.tm_min != g_lastWorldMin) {
                g_lastWorldMin = ti.tm_min;
                g_dirty = true;
            }
        } else if (g_page == PageId::NIGHT) {
            struct tm ti{};
            if (getLocalTime(&ti, 2) && ti.tm_min != g_lastNightMin) {
                g_lastNightMin = ti.tm_min;
                g_dirty = true;
            }
        } else if (g_page == PageId::NEWS && g_news.valid && g_news.count > 1) {
            if ((now - g_newsCycleMs) >= NEWS_AUTO_CYCLE_MS) {
                g_newsCycleMs = now;
                g_newsIdx = (g_newsIdx + 1) % g_news.count;
                g_dirty = true;
            }
        } else if (g_page == PageId::MUSIC) {
            if (!g_btSpeakerMode && (now - g_lastSpotifyPollMs) >= SPOTIFY_REFRESH_MS) {
                g_lastSpotifyPollMs = now;
                netRequestSpotifyRefresh();
            } else if (g_btSpeakerMode && !g_dirty) {
                uiMusicDrawMeter(tft, g_music);
            }
        }

        if (g_dirty) {
            g_dirty = false;
            switch (g_page) {
                case PageId::WEATHER:  uiWeatherDraw(tft, g_cfg, g_wx, fetching); break;
                case PageId::MARKET:   uiMarketDraw(tft, g_mk, fetching); break;
                case PageId::WORLD:    uiWorldDraw(tft, g_cfg, fetching); break;
                case PageId::ALARM:    uiAlarmDraw(tft, g_cfg, g_alarmEditIdx, fetching); break;
                case PageId::NEWS:     uiNewsDraw(tft, g_news, g_newsIdx, fetching); break;
                case PageId::POMODORO: uiPomodoroDraw(tft, g_pomo, fetching); break;
                case PageId::MUSIC:    uiMusicDraw(tft, g_music, audioBtSupported(), !g_btSpeakerMode && fetching); break;
                case PageId::MENU:     uiMenuDraw(tft); break;
                case PageId::SETTINGS: uiSettingsDraw(tft, g_cfg); break;
                case PageId::NIGHT:    uiNightDraw(tft, g_cfg, g_wx); break;
                default: break;
            }
        }
    }

    delay(5);
}
