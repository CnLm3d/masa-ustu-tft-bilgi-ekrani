#include "audio_radio.h"
#include "config.h"
#include <Arduino.h>
#include <ESP_I2S.h>
#include <MP3DecoderHelix.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>

namespace {
I2SClass g_i2s;
bool g_i2sRunning = false;
uint32_t g_i2sRate = 0;

SemaphoreHandle_t g_i2sMux = nullptr;
SemaphoreHandle_t g_stateMux = nullptr;
SemaphoreHandle_t g_stationMux = nullptr;

RadioStation g_stations[MAX_RADIO_STATIONS]{};
uint8_t g_stationCount = 0;
uint8_t g_targetIdx = 0;
volatile uint8_t g_volPct = 70;

volatile bool g_cmdPlay = false;
volatile bool g_cmdStop = false;
volatile bool g_stopReq = false;
volatile bool g_playing = false;

RadioState g_radio{};
libhelix::MP3DecoderHelix* g_mp3 = nullptr;
uint32_t g_lastVuMs = 0;

void ensureMuxes() {
    if (!g_i2sMux) g_i2sMux = xSemaphoreCreateMutex();
    if (!g_stateMux) g_stateMux = xSemaphoreCreateMutex();
    if (!g_stationMux) g_stationMux = xSemaphoreCreateMutex();
}

bool startI2S(uint32_t rate) {
    if (rate < 8000 || rate > 96000) rate = 44100;
    if (g_i2sRunning) {
        if (g_i2sRate != rate) {
            g_i2s.configureTX(rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
            g_i2sRate = rate;
        }
        return true;
    }
    g_i2s.setPins(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
    g_i2sRunning = g_i2s.begin(I2S_MODE_STD, rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    if (g_i2sRunning) g_i2sRate = rate;
    return g_i2sRunning;
}

void stopI2S() {
    if (g_i2sRunning) {
        g_i2s.end();
        g_i2sRunning = false;
        g_i2sRate = 0;
    }
}

void setStatus(bool playing, bool conn, bool err, const char* msg, const char* title = nullptr) {
    ensureMuxes();
    if (xSemaphoreTake(g_stateMux, pdMS_TO_TICKS(50))) {
        g_radio.isPlaying = playing;
        g_radio.connecting = conn;
        g_radio.hasError = err;
        if (msg) strlcpy(g_radio.statusMsg, msg, sizeof(g_radio.statusMsg));
        if (title) strlcpy(g_radio.streamTitle, title, sizeof(g_radio.streamTitle));
        if (!playing) {
            g_radio.vuLeft = 0;
            g_radio.vuRight = 0;
        }
        g_radio.updatedAtMs = millis();
        xSemaphoreGive(g_stateMux);
    }
}

void onMp3Pcm(MP3FrameInfo& info, short* pcm, size_t len, void* ref) {
    (void)ref;
    if (g_stopReq || !pcm || len == 0) return;

    const uint32_t rate = (info.samprate > 0) ? static_cast<uint32_t>(info.samprate) : 44100UL;
    if (!startI2S(rate)) return;

    const uint32_t v = g_volPct;
    const int32_t gainQ12 = static_cast<int32_t>((v * v * 4096UL) / 10000UL);
    const bool mono = (info.nChans == 1);

    int16_t outBuf[256];
    int32_t peakL = 0;
    int32_t peakR = 0;

    size_t pos = 0;
    while (pos < len && !g_stopReq) {
        size_t outSamples = 0;
        if (mono) {
            const size_t n = min<size_t>(128, len - pos);
            for (size_t i = 0; i < n; ++i) {
                int32_t s = (static_cast<int32_t>(pcm[pos + i]) * gainQ12) >> 12;
                if (s > 32767) s = 32767;
                else if (s < -32768) s = -32768;
                const int16_t s16 = static_cast<int16_t>(s);
                outBuf[outSamples++] = s16;
                outBuf[outSamples++] = s16;
                const int32_t a = (s < 0) ? -s : s;
                if (a > peakL) peakL = a;
                if (a > peakR) peakR = a;
            }
            pos += n;
        } else {
            const size_t n = min<size_t>(256, len - pos) & ~static_cast<size_t>(1);
            if (n == 0) break;
            for (size_t i = 0; i < n; ++i) {
                int32_t s = (static_cast<int32_t>(pcm[pos + i]) * gainQ12) >> 12;
                if (s > 32767) s = 32767;
                else if (s < -32768) s = -32768;
                outBuf[outSamples++] = static_cast<int16_t>(s);
                const int32_t a = (s < 0) ? -s : s;
                if ((i & 1) == 0) {
                    if (a > peakL) peakL = a;
                } else {
                    if (a > peakR) peakR = a;
                }
            }
            pos += n;
        }
        g_i2s.write(reinterpret_cast<const uint8_t*>(outBuf), outSamples * sizeof(int16_t));
    }

    const uint32_t now = millis();
    if ((now - g_lastVuMs) >= 38) {
        g_lastVuMs = now;
        const uint8_t vuL = static_cast<uint8_t>(min<int32_t>(100, (peakL * 100) / 22000));
        const uint8_t vuR = static_cast<uint8_t>(min<int32_t>(100, (peakR * 100) / 22000));
        if (xSemaphoreTake(g_stateMux, 0)) {
            g_radio.vuLeft = vuL;
            g_radio.vuRight = vuR;
            g_radio.sampleRate = rate;
            xSemaphoreGive(g_stateMux);
        }
    }
}

struct ParsedUrl {
    bool isHttps;
    String host;
    uint16_t port;
    String path;
};

bool parseHttpUrl(const String& url, ParsedUrl& out) {
    String u = url;
    u.trim();
    if (u.startsWith("https://")) {
        out.isHttps = true;
        out.port = 443;
        u = u.substring(8);
    } else if (u.startsWith("http://")) {
        out.isHttps = false;
        out.port = 80;
        u = u.substring(7);
    } else {
        return false;
    }

    const int slash = u.indexOf('/');
    String hostPort = (slash >= 0) ? u.substring(0, slash) : u;
    out.path = (slash >= 0) ? u.substring(slash) : "/";
    const int colon = hostPort.indexOf(':');
    if (colon > 0) {
        out.host = hostPort.substring(0, colon);
        const int p = hostPort.substring(colon + 1).toInt();
        if (p > 0 && p <= 65535) out.port = static_cast<uint16_t>(p);
    } else {
        out.host = hostPort;
    }
    return out.host.length() > 0;
}

String readHeaderLine(Client& c, uint32_t timeoutMs = 4000) {
    String line;
    line.reserve(128);
    const uint32_t start = millis();
    while ((millis() - start) < timeoutMs && !g_stopReq) {
        if (c.available()) {
            const char ch = static_cast<char>(c.read());
            if (ch == '\r') continue;
            if (ch == '\n') return line;
            if (line.length() < 320) line += ch;
        } else {
            if (!c.connected()) break;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    return line;
}

bool readExactBytes(Client& c, uint8_t* dst, size_t need, uint32_t timeoutMs) {
    size_t got = 0;
    const uint32_t start = millis();
    while (got < need && !g_stopReq) {
        const int avail = c.available();
        if (avail > 0) {
            const int r = c.read(dst + got, need - got);
            if (r > 0) got += static_cast<size_t>(r);
        } else {
            if (!c.connected() || (millis() - start) > timeoutMs) break;
            vTaskDelay(pdMS_TO_TICKS(4));
        }
    }
    return got == need;
}

void parseIcyStreamTitle(const char* meta, char* outTitle, size_t outLen) {
    const char* p = strstr(meta, "StreamTitle='");
    if (!p) return;
    p += 13;
    const char* end = strstr(p, "';");
    if (!end) end = strchr(p, '\'');
    size_t n = end ? static_cast<size_t>(end - p) : strlen(p);
    if (n >= outLen) n = outLen - 1;
    memcpy(outTitle, p, n);
    outTitle[n] = '\0';
}

void streamStation(uint8_t idx) {
    RadioStation st{};
    uint8_t count = 0;
    if (xSemaphoreTake(g_stationMux, pdMS_TO_TICKS(100))) {
        count = g_stationCount;
        if (count > 0) {
            idx %= count;
            st = g_stations[idx];
        }
        xSemaphoreGive(g_stationMux);
    }
    if (count == 0 || st.url[0] == '\0') {
        setStatus(false, false, true, "Kanal listesi boş", "");
        return;
    }

    if (xSemaphoreTake(g_stateMux, pdMS_TO_TICKS(50))) {
        g_radio.stationIdx = idx;
        g_radio.stationCount = count;
        strlcpy(g_radio.stationName, st.name, sizeof(g_radio.stationName));
        g_radio.streamTitle[0] = '\0';
        g_radio.sampleRate = 0;
        xSemaphoreGive(g_stateMux);
    }

    if (WiFi.status() != WL_CONNECTED) {
        setStatus(false, false, true, "Wi-Fi bağlantısı yok");
        return;
    }

    setStatus(false, true, false, "Bağlanıyor...");

    String curUrl = st.url;
    WiFiClient plainClient;
    WiFiClientSecure tlsClient;
    Client* cli = nullptr;

    int icyMetaint = 0;
    String contentType;
    bool connectedOk = false;

    for (int hop = 0; hop < 4 && !g_stopReq; ++hop) {
        ParsedUrl pu{};
        if (!parseHttpUrl(curUrl, pu)) {
            setStatus(false, false, true, "Geçersiz URL");
            return;
        }

        if (cli) {
            cli->stop();
            cli = nullptr;
        }

        if (pu.isHttps) {
            tlsClient.setInsecure();
            tlsClient.setTimeout(6);
            if (!tlsClient.connect(pu.host.c_str(), pu.port)) {
                setStatus(false, false, true, "Sunucuya bağlanılamadı");
                return;
            }
            cli = &tlsClient;
        } else {
            plainClient.setTimeout(6);
            if (!plainClient.connect(pu.host.c_str(), pu.port)) {
                setStatus(false, false, true, "Sunucuya bağlanılamadı");
                return;
            }
            cli = &plainClient;
        }

        cli->print(String("GET ") + pu.path + " HTTP/1.0\r\n");
        cli->print(String("Host: ") + pu.host + "\r\n");
        cli->print("User-Agent: ESP32-DeskHub/5.0\r\n");
        cli->print("Icy-MetaData: 1\r\n");
        cli->print("Connection: close\r\n\r\n");

        const String statusLine = readHeaderLine(*cli, 5000);
        int httpCode = 0;
        const int sp = statusLine.indexOf(' ');
        if (sp > 0) httpCode = statusLine.substring(sp + 1).toInt();

        icyMetaint = 0;
        contentType = "";
        String location;
        String icyName;

        while (!g_stopReq) {
            String h = readHeaderLine(*cli, 3000);
            if (h.length() == 0) break;
            const int col = h.indexOf(':');
            if (col <= 0) continue;
            String k = h.substring(0, col);
            String v = h.substring(col + 1);
            k.toLowerCase();
            k.trim();
            v.trim();
            if (k == "location") location = v;
            else if (k == "icy-metaint") icyMetaint = v.toInt();
            else if (k == "content-type") {
                v.toLowerCase();
                contentType = v;
            } else if (k == "icy-name" && icyName.length() == 0) {
                icyName = v;
            }
        }

        if (httpCode >= 300 && httpCode < 400 && location.length() > 0) {
            if (location.startsWith("/")) {
                curUrl = String(pu.isHttps ? "https://" : "http://") + pu.host + location;
            } else {
                curUrl = location;
            }
            continue;
        }

        if (httpCode != 200 && httpCode != 206) {
            char err[48];
            snprintf(err, sizeof(err), "Yayın hatası (HTTP %d)", httpCode);
            setStatus(false, false, true, err);
            cli->stop();
            return;
        }

        if (contentType.indexOf("aac") >= 0 && contentType.indexOf("mpeg") < 0) {
            setStatus(false, false, true, "AAC desteklenmez (MP3 gerekir)");
            cli->stop();
            return;
        }

        if (icyName.length() > 0 && xSemaphoreTake(g_stateMux, pdMS_TO_TICKS(50))) {
            strlcpy(g_radio.streamTitle, icyName.c_str(), sizeof(g_radio.streamTitle));
            xSemaphoreGive(g_stateMux);
        }

        connectedOk = true;
        break;
    }

    if (!connectedOk || !cli || g_stopReq) {
        if (cli) cli->stop();
        if (!g_stopReq) setStatus(false, false, true, "Yönlendirme hatası");
        return;
    }

    if (!xSemaphoreTake(g_i2sMux, pdMS_TO_TICKS(500))) {
        cli->stop();
        setStatus(false, false, true, "Ses donanımı meşgul");
        return;
    }

    if (!g_mp3) {
        g_mp3 = new libhelix::MP3DecoderHelix(onMp3Pcm);
    }
    if (!g_mp3 || !g_mp3->begin()) {
        xSemaphoreGive(g_i2sMux);
        cli->stop();
        setStatus(false, false, true, "MP3 çözücü başlatılamadı");
        return;
    }

    g_playing = true;
    setStatus(true, false, false, "CANLI MP3 • ICY");

    constexpr size_t RX_BUF_SZ = 1024;
    uint8_t rxBuf[RX_BUF_SZ];
    uint8_t metaBuf[512];
    int audioLeft = icyMetaint;
    uint32_t lastDataMs = millis();

    while (!g_stopReq) {
        if (!cli->connected() && cli->available() <= 0) break;

        if (icyMetaint > 0 && audioLeft == 0) {
            uint8_t lenByte = 0;
            if (!readExactBytes(*cli, &lenByte, 1, 3000)) break;
            const size_t metaBytes = static_cast<size_t>(lenByte) * 16U;
            if (metaBytes > 0) {
                if (metaBytes < sizeof(metaBuf)) {
                    if (!readExactBytes(*cli, metaBuf, metaBytes, 3000)) break;
                    metaBuf[metaBytes] = '\0';
                    char parsedTitle[96] = {0};
                    parseIcyStreamTitle(reinterpret_cast<const char*>(metaBuf), parsedTitle, sizeof(parsedTitle));
                    if (parsedTitle[0] != '\0' && xSemaphoreTake(g_stateMux, pdMS_TO_TICKS(20))) {
                        if (strcmp(g_radio.streamTitle, parsedTitle) != 0) {
                            strlcpy(g_radio.streamTitle, parsedTitle, sizeof(g_radio.streamTitle));
                            g_radio.updatedAtMs = millis();
                        }
                        xSemaphoreGive(g_stateMux);
                    }
                } else {
                    // Drain oversized metadata cleanly
                    size_t rem = metaBytes;
                    while (rem > 0 && !g_stopReq) {
                        const size_t chunk = min(rem, sizeof(metaBuf));
                        if (!readExactBytes(*cli, metaBuf, chunk, 3000)) {
                            rem = 1;
                            break;
                        }
                        rem -= chunk;
                    }
                    if (rem > 0) break;
                }
            }
            audioLeft = icyMetaint;
            lastDataMs = millis();
            continue;
        }

        const int avail = cli->available();
        if (avail > 0) {
            size_t toRead = min<size_t>(static_cast<size_t>(avail), RX_BUF_SZ);
            if (icyMetaint > 0 && audioLeft > 0) {
                toRead = min<size_t>(toRead, static_cast<size_t>(audioLeft));
            }
            const int got = cli->read(rxBuf, toRead);
            if (got > 0) {
                lastDataMs = millis();
                if (icyMetaint > 0) audioLeft -= got;
                g_mp3->write(rxBuf, static_cast<size_t>(got));
            }
        } else {
            if ((millis() - lastDataMs) > 7000) {
                if (!g_stopReq) setStatus(false, false, true, "Yayın zaman aşımı");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    cli->stop();
    g_mp3->end();
    stopI2S();
    g_playing = false;
    xSemaphoreGive(g_i2sMux);

    if (g_stopReq) {
        setStatus(false, false, false, "Duraklatıldı");
    } else if (!g_radio.hasError) {
        setStatus(false, false, true, "Akış kesildi");
    }
}

void radioTask(void* param) {
    (void)param;
    for (;;) {
        if (g_cmdStop) {
            g_cmdStop = false;
            g_stopReq = false;
            setStatus(false, false, false, "Duraklatıldı");
        }
        if (g_cmdPlay) {
            g_cmdPlay = false;
            g_stopReq = false;
            streamStation(g_targetIdx);
        }
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}
} // namespace

void radioInit(const DeviceConfig& cfg) {
    ensureMuxes();
    g_volPct = (cfg.volume > 100) ? 100 : cfg.volume;
    g_targetIdx = cfg.radioActiveIdx;
    radioUpdateStations(cfg);
    setStatus(false, false, false, "Hazır • Çal'a dokunun", "");
}

void radioUpdateStations(const DeviceConfig& cfg) {
    ensureMuxes();
    if (xSemaphoreTake(g_stationMux, pdMS_TO_TICKS(100))) {
        g_stationCount = min<uint8_t>(cfg.radioStationCount, static_cast<uint8_t>(MAX_RADIO_STATIONS));
        for (uint8_t i = 0; i < g_stationCount; ++i) {
            g_stations[i] = cfg.radioStations[i];
        }
        if (g_stationCount > 0 && g_targetIdx >= g_stationCount) {
            g_targetIdx = 0;
        }
        xSemaphoreGive(g_stationMux);
    }
    if (xSemaphoreTake(g_stateMux, pdMS_TO_TICKS(50))) {
        g_radio.stationCount = g_stationCount;
        g_radio.volume = g_volPct;
        if (g_stationCount > 0) {
            const uint8_t idx = g_targetIdx % g_stationCount;
            g_radio.stationIdx = idx;
            strlcpy(g_radio.stationName, g_stations[idx].name, sizeof(g_radio.stationName));
        } else {
            g_radio.stationIdx = 0;
            strlcpy(g_radio.stationName, "Kanal Yok", sizeof(g_radio.stationName));
        }
        g_radio.updatedAtMs = millis();
        xSemaphoreGive(g_stateMux);
    }
}

void radioStartTask() {
    xTaskCreatePinnedToCore(radioTask, "radioTask", 12288, nullptr, 2, nullptr, 0);
}

void radioPlay(uint8_t idx) {
    ensureMuxes();
    if (xSemaphoreTake(g_stationMux, pdMS_TO_TICKS(50))) {
        if (g_stationCount > 0) g_targetIdx = idx % g_stationCount;
        xSemaphoreGive(g_stationMux);
    }
    g_stopReq = true;
    g_cmdPlay = true;
}

void radioStop() {
    g_cmdPlay = false;
    g_stopReq = true;
    g_cmdStop = true;
}

void radioToggle() {
    if (g_playing || g_radio.connecting) radioStop();
    else radioPlay(g_targetIdx);
}

void radioNext() {
    uint8_t next = 0;
    if (xSemaphoreTake(g_stationMux, pdMS_TO_TICKS(50))) {
        if (g_stationCount > 0) next = (g_targetIdx + 1) % g_stationCount;
        xSemaphoreGive(g_stationMux);
    }
    radioPlay(next);
}

void radioPrev() {
    uint8_t prev = 0;
    if (xSemaphoreTake(g_stationMux, pdMS_TO_TICKS(50))) {
        if (g_stationCount > 0) prev = (g_targetIdx + g_stationCount - 1) % g_stationCount;
        xSemaphoreGive(g_stationMux);
    }
    radioPlay(prev);
}

void radioSetVolume(uint8_t vol) {
    g_volPct = (vol > 100) ? 100 : vol;
    ensureMuxes();
    if (xSemaphoreTake(g_stateMux, pdMS_TO_TICKS(25))) {
        g_radio.volume = g_volPct;
        g_radio.updatedAtMs = millis();
        xSemaphoreGive(g_stateMux);
    }
}

bool radioIsPlaying() {
    return g_playing || g_radio.connecting;
}

bool radioCopyState(RadioState& out) {
    ensureMuxes();
    if (xSemaphoreTake(g_stateMux, pdMS_TO_TICKS(25))) {
        out = g_radio;
        xSemaphoreGive(g_stateMux);
        return true;
    }
    return false;
}

void audioPlayChime() {
    ensureMuxes();
    if (g_playing) return;
    if (!xSemaphoreTake(g_i2sMux, pdMS_TO_TICKS(40))) return;
    if (!startI2S(44100)) {
        xSemaphoreGive(g_i2sMux);
        return;
    }

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
    xSemaphoreGive(g_i2sMux);
}
