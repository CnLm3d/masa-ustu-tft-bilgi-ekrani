#include "audio_bt.h"
#include "config.h"
#include <Arduino.h>
#include <ESP_I2S.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>
#include <string.h>

#if HAS_BT_SPEAKER
#include <esp_a2dp_api.h>
#include <esp_a2dp_legacy_api.h>
#include <esp_avrc_api.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#endif

namespace {
I2SClass g_i2s;
bool g_i2sRunning = false;
volatile bool g_btActive = false;
volatile uint8_t g_volPct = 70;
SemaphoreHandle_t g_audioMux = nullptr;
MusicState g_btState{};

void ensureMux() {
    if (!g_audioMux) g_audioMux = xSemaphoreCreateMutex();
}

bool startI2S(uint32_t rate) {
    if (g_i2sRunning) {
        g_i2s.configureTX(rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
        return true;
    }
    g_i2s.setPins(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
    g_i2sRunning = g_i2s.begin(I2S_MODE_STD, rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    return g_i2sRunning;
}

void stopI2S() {
    if (g_i2sRunning) {
        g_i2s.end();
        g_i2sRunning = false;
    }
}

#if HAS_BT_SPEAKER
uint8_t g_tl = 1;
uint32_t g_lastVuMs = 0;

uint8_t nextTl() {
    g_tl = (g_tl + 1) & 0x0F;
    return g_tl;
}

void requestMetadata() {
    esp_avrc_ct_send_metadata_cmd(
        nextTl(), ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_ALBUM);
}

void onA2dpData(const uint8_t* data, uint32_t len) {
    if (!g_i2sRunning || !data || len < 4) return;

    const int16_t* src = reinterpret_cast<const int16_t*>(data);
    const size_t totalSamples = len / 2;
    const uint32_t v = g_volPct;
    const int32_t gainQ12 = static_cast<int32_t>((v * v * 4096UL) / 10000UL);

    int16_t outBuf[256];
    int32_t peakL = 0;
    int32_t peakR = 0;

    size_t pos = 0;
    while (pos < totalSamples) {
        const size_t n = min<size_t>(256, totalSamples - pos);
        for (size_t i = 0; i < n; ++i) {
            int32_t s = (static_cast<int32_t>(src[pos + i]) * gainQ12) >> 12;
            if (s > 32767) s = 32767;
            else if (s < -32768) s = -32768;
            outBuf[i] = static_cast<int16_t>(s);
            const int32_t a = (s < 0) ? -s : s;
            if ((i & 1) == 0) {
                if (a > peakL) peakL = a;
            } else {
                if (a > peakR) peakR = a;
            }
        }
        g_i2s.write(reinterpret_cast<const uint8_t*>(outBuf), n * 2);
        pos += n;
    }

    const uint32_t now = millis();
    if ((now - g_lastVuMs) >= 40) {
        g_lastVuMs = now;
        const uint8_t vuL = static_cast<uint8_t>(min<int32_t>(100, (peakL * 100) / 24000));
        const uint8_t vuR = static_cast<uint8_t>(min<int32_t>(100, (peakR * 100) / 24000));
        if (g_audioMux && xSemaphoreTake(g_audioMux, 0)) {
            g_btState.vuLeft = vuL;
            g_btState.vuRight = vuR;
            xSemaphoreGive(g_audioMux);
        }
    }
}

void onA2dpEvent(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) {
    if (!param) return;
    ensureMux();

    if (event == ESP_A2D_CONNECTION_STATE_EVT) {
        const bool conn = (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED);
        if (xSemaphoreTake(g_audioMux, pdMS_TO_TICKS(50))) {
            g_btState.btConnected = conn;
            if (!conn) {
                g_btState.isPlaying = false;
                g_btState.vuLeft = 0;
                g_btState.vuRight = 0;
                strlcpy(g_btState.title, "Bluetooth Eşleşme Bekleniyor", sizeof(g_btState.title));
                strlcpy(g_btState.artist, "Telefonunuzdan bağlanın", sizeof(g_btState.artist));
                g_btState.album[0] = '\0';
                strlcpy(g_btState.statusMsg, "Hazır • Eşleşme Bekliyor", sizeof(g_btState.statusMsg));
            } else {
                strlcpy(g_btState.statusMsg, "Bluetooth Bağlı", sizeof(g_btState.statusMsg));
            }
            g_btState.updatedAtMs = millis();
            xSemaphoreGive(g_audioMux);
        }
    } else if (event == ESP_A2D_AUDIO_STATE_EVT) {
        const bool play = (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED);
        if (xSemaphoreTake(g_audioMux, pdMS_TO_TICKS(50))) {
            g_btState.isPlaying = play;
            if (!play) {
                g_btState.vuLeft = 0;
                g_btState.vuRight = 0;
            }
            g_btState.updatedAtMs = millis();
            xSemaphoreGive(g_audioMux);
        }
    } else if (event == ESP_A2D_AUDIO_CFG_EVT) {
        if (param->audio_cfg.mcc.type == ESP_A2D_MCT_SBC) {
            const uint8_t oct0 = param->audio_cfg.mcc.cie.sbc_info.samp_freq;
            uint32_t rate = 44100;
            if (oct0 & 0x08) rate = 16000;
            else if (oct0 & 0x04) rate = 32000;
            else if (oct0 & 0x02) rate = 44100;
            else if (oct0 & 0x01) rate = 48000;
            startI2S(rate);
        }
    }
}

void onAvrcEvent(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param) {
    if (!param) return;
    ensureMux();

    if (event == ESP_AVRC_CT_CONNECTION_STATE_EVT) {
        if (param->conn_stat.connected) {
            requestMetadata();
            esp_avrc_ct_send_register_notification_cmd(nextTl(), ESP_AVRC_RN_TRACK_CHANGE, 0);
            esp_avrc_ct_send_register_notification_cmd(nextTl(), ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0);
        }
    } else if (event == ESP_AVRC_CT_METADATA_RSP_EVT) {
        if (param->meta_rsp.attr_text && param->meta_rsp.attr_length > 0) {
            if (xSemaphoreTake(g_audioMux, pdMS_TO_TICKS(50))) {
                const size_t n = static_cast<size_t>(param->meta_rsp.attr_length);
                const char* src = reinterpret_cast<const char*>(param->meta_rsp.attr_text);
                if (param->meta_rsp.attr_id == ESP_AVRC_MD_ATTR_TITLE) {
                    size_t cp = min(n, sizeof(g_btState.title) - 1);
                    memcpy(g_btState.title, src, cp);
                    g_btState.title[cp] = '\0';
                } else if (param->meta_rsp.attr_id == ESP_AVRC_MD_ATTR_ARTIST) {
                    size_t cp = min(n, sizeof(g_btState.artist) - 1);
                    memcpy(g_btState.artist, src, cp);
                    g_btState.artist[cp] = '\0';
                } else if (param->meta_rsp.attr_id == ESP_AVRC_MD_ATTR_ALBUM) {
                    size_t cp = min(n, sizeof(g_btState.album) - 1);
                    memcpy(g_btState.album, src, cp);
                    g_btState.album[cp] = '\0';
                }
                g_btState.updatedAtMs = millis();
                xSemaphoreGive(g_audioMux);
            }
        }
    } else if (event == ESP_AVRC_CT_CHANGE_NOTIFY_EVT) {
        if (param->change_ntf.event_id == ESP_AVRC_RN_TRACK_CHANGE) {
            requestMetadata();
            esp_avrc_ct_send_register_notification_cmd(nextTl(), ESP_AVRC_RN_TRACK_CHANGE, 0);
        } else if (param->change_ntf.event_id == ESP_AVRC_RN_PLAY_STATUS_CHANGE) {
            if (xSemaphoreTake(g_audioMux, pdMS_TO_TICKS(50))) {
                g_btState.isPlaying =
                    (param->change_ntf.event_parameter.playback == ESP_AVRC_PLAYBACK_PLAYING);
                g_btState.updatedAtMs = millis();
                xSemaphoreGive(g_audioMux);
            }
            esp_avrc_ct_send_register_notification_cmd(nextTl(), ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0);
        }
    }
}
#endif
} // namespace

bool audioBtSupported() {
    return HAS_BT_SPEAKER != 0;
}

bool audioBtStart(const char* deviceName, uint8_t volume) {
#if HAS_BT_SPEAKER
    ensureMux();
    if (g_btActive) return true;

    g_volPct = (volume > 100) ? 100 : volume;
    if (!startI2S(44100)) return false;

    if (!btStart()) {
        stopI2S();
        return false;
    }
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        if (esp_bluedroid_init() != ESP_OK) return false;
    }
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        if (esp_bluedroid_enable() != ESP_OK) return false;
    }

    esp_bt_gap_set_device_name((deviceName && deviceName[0]) ? deviceName : DEFAULT_BT_NAME);
    esp_avrc_ct_init();
    esp_avrc_ct_register_callback(onAvrcEvent);
    esp_a2d_register_callback(onA2dpEvent);
    esp_a2d_sink_register_data_callback(onA2dpData);
    esp_a2d_sink_init();
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    if (xSemaphoreTake(g_audioMux, pdMS_TO_TICKS(100))) {
        g_btState = MusicState{};
        g_btState.btActive = true;
        g_btState.volume = g_volPct;
        g_btState.sourceMode = 0;
        strlcpy(g_btState.title, "Bluetooth Eşleşme Bekleniyor", sizeof(g_btState.title));
        strlcpy(g_btState.artist, (deviceName && deviceName[0]) ? deviceName : DEFAULT_BT_NAME,
                sizeof(g_btState.artist));
        strlcpy(g_btState.statusMsg, "Keşfedilebilir (A2DP + PCM5102A)", sizeof(g_btState.statusMsg));
        g_btState.updatedAtMs = millis();
        xSemaphoreGive(g_audioMux);
    }
    g_btActive = true;
    return true;
#else
    (void)deviceName;
    (void)volume;
    return false;
#endif
}

void audioBtStop() {
#if HAS_BT_SPEAKER
    ensureMux();
    if (!g_btActive) return;

    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    esp_a2d_sink_deinit();
    esp_avrc_ct_deinit();
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
        esp_bluedroid_disable();
    }
    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
        esp_bluedroid_deinit();
    }
    btStop();
    stopI2S();

    g_btActive = false;
    if (xSemaphoreTake(g_audioMux, pdMS_TO_TICKS(100))) {
        g_btState.btActive = false;
        g_btState.btConnected = false;
        g_btState.isPlaying = false;
        g_btState.vuLeft = 0;
        g_btState.vuRight = 0;
        xSemaphoreGive(g_audioMux);
    }
#endif
}

bool audioBtIsActive() {
    return g_btActive;
}

void audioBtSetVolume(uint8_t volume) {
    g_volPct = (volume > 100) ? 100 : volume;
    if (g_audioMux && xSemaphoreTake(g_audioMux, pdMS_TO_TICKS(25))) {
        g_btState.volume = g_volPct;
        xSemaphoreGive(g_audioMux);
    }
}

void audioBtSendCmd(BtMediaCmd cmd) {
#if HAS_BT_SPEAKER
    if (!g_btActive) return;
    uint8_t code = ESP_AVRC_PT_CMD_PLAY;
    if (cmd == BtMediaCmd::PLAY_PAUSE) {
        code = g_btState.isPlaying ? ESP_AVRC_PT_CMD_PAUSE : ESP_AVRC_PT_CMD_PLAY;
    } else if (cmd == BtMediaCmd::NEXT) {
        code = ESP_AVRC_PT_CMD_FORWARD;
    } else if (cmd == BtMediaCmd::PREV) {
        code = ESP_AVRC_PT_CMD_BACKWARD;
    }
    esp_avrc_ct_send_passthrough_cmd(nextTl(), code, ESP_AVRC_PT_CMD_STATE_PRESSED);
    delay(40);
    esp_avrc_ct_send_passthrough_cmd(nextTl(), code, ESP_AVRC_PT_CMD_STATE_RELEASED);
#else
    (void)cmd;
#endif
}

bool audioBtCopyState(MusicState& out) {
    ensureMux();
    if (xSemaphoreTake(g_audioMux, pdMS_TO_TICKS(25))) {
        out = g_btState;
        xSemaphoreGive(g_audioMux);
        return true;
    }
    return false;
}

void audioPlayChime() {
    if (g_btActive) return;
    if (!startI2S(44100)) return;

    int16_t buf[256];
    const float freqs[2] = {880.0f, 1318.5f};
    for (int note = 0; note < 2; ++note) {
        const float f = freqs[note];
        const int totalFrames = 4410; // 100 ms per note
        int idx = 0;
        while (idx < totalFrames) {
            const int chunk = min(128, totalFrames - idx);
            for (int i = 0; i < chunk; ++i) {
                const float t = static_cast<float>(idx + i) / 44100.0f;
                const float env = 1.0f - static_cast<float>(idx + i) / totalFrames;
                const int16_t s = static_cast<int16_t>(sinf(2.0f * 3.1415926f * f * t) * env * 9000.0f);
                buf[i * 2] = s;
                buf[i * 2 + 1] = s;
            }
            g_i2s.write(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(chunk) * 4);
            idx += chunk;
        }
    }
    stopI2S();
}
