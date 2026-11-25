#include "net_services.h"
#include "config.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <time.h>

namespace {
DeviceConfig g_cfg{};
WeatherData g_weather{};
NewsData g_news{};
MarketData g_market{};

SemaphoreHandle_t g_mutex = nullptr;
volatile bool g_reqWeather = true;
volatile bool g_reqNews = true;
volatile bool g_reqMarket = true;
volatile bool g_reqTime = true;
volatile uint8_t g_targetTopic = 0;
volatile bool g_fetching = false;

const char* const NEWS_URLS[static_cast<size_t>(NewsTopic::COUNT)] = {
    "https://news.google.com/rss?hl=tr&gl=TR&ceid=TR:tr",
    "https://news.google.com/rss/headlines/section/topic/TECHNOLOGY?hl=tr&gl=TR&ceid=TR:tr",
    "https://news.google.com/rss/headlines/section/topic/BUSINESS?hl=tr&gl=TR&ceid=TR:tr",
    "https://news.google.com/rss/headlines/section/topic/SCIENCE?hl=tr&gl=TR&ceid=TR:tr",
    "https://news.google.com/rss/headlines/section/topic/SPORTS?hl=tr&gl=TR&ceid=TR:tr"
};

void appendUtf8(String& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out += static_cast<char>(cp);
    } else if (cp <= 0x7FF) {
        out += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        out += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

String decodeXmlText(String s) {
    s.trim();
    if (s.startsWith("<![CDATA[") && s.endsWith("]]>")) {
        s = s.substring(9, s.length() - 3);
        s.trim();
    }

    String out;
    out.reserve(s.length());
    const int n = s.length();
    for (int i = 0; i < n; ++i) {
        if (s[i] == '&') {
            int semi = s.indexOf(';', i + 1);
            if (semi > i && (semi - i) <= 10) {
                String ent = s.substring(i + 1, semi);
                if (ent == "amp") out += '&';
                else if (ent == "quot") out += '"';
                else if (ent == "apos" || ent == "#39") out += '\'';
                else if (ent == "lt") out += '<';
                else if (ent == "gt") out += '>';
                else if (ent == "nbsp") out += ' ';
                else if (ent.startsWith("#x") || ent.startsWith("#X")) {
                    uint32_t cp = static_cast<uint32_t>(strtoul(ent.c_str() + 2, nullptr, 16));
                    if (cp > 0) appendUtf8(out, cp);
                } else if (ent.startsWith("#")) {
                    uint32_t cp = static_cast<uint32_t>(strtoul(ent.c_str() + 1, nullptr, 10));
                    if (cp > 0) appendUtf8(out, cp);
                } else {
                    out += '&';
                    continue;
                }
                i = semi;
                continue;
            }
        }
        out += s[i];
    }
    return out;
}

void formatPubDateTrt(const String& raw, char* dst, size_t dstLen) {
    int colon = raw.indexOf(':');
    if (colon >= 2) {
        int hh = raw.substring(colon - 2, colon).toInt();
        int mm = raw.substring(colon + 1, colon + 3).toInt();
        if (raw.indexOf("GMT") >= 0 || raw.indexOf("+0000") >= 0 || raw.indexOf("UTC") >= 0) {
            hh = (hh + 3) % 24;
        }
        snprintf(dst, dstLen, "%02d:%02d", hh, mm);
        return;
    }
    strlcpy(dst, "--:--", dstLen);
}

bool fetchWeather() {
    if (WiFi.status() != WL_CONNECTED) return false;

    float lat = 0.0f, lon = 0.0f;
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100))) {
        lat = g_cfg.lat;
        lon = g_cfg.lon;
        xSemaphoreGive(g_mutex);
    }

    char url[280];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=4",
             lat, lon);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.useHTTP10(true);
    http.setTimeout(10000);
    if (!http.begin(client, url)) return false;

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100))) {
            g_weather.hasError = true;
            snprintf(g_weather.errorMsg, sizeof(g_weather.errorMsg), "Hava HTTP %d", code);
            xSemaphoreGive(g_mutex);
        }
        return false;
    }

    JsonDocument filter;
    filter["current"]["temperature_2m"] = true;
    filter["current"]["relative_humidity_2m"] = true;
    filter["current"]["apparent_temperature"] = true;
    filter["current"]["is_day"] = true;
    filter["current"]["weather_code"] = true;
    filter["current"]["wind_speed_10m"] = true;
    filter["daily"]["time"] = true;
    filter["daily"]["weather_code"] = true;
    filter["daily"]["temperature_2m_max"] = true;
    filter["daily"]["temperature_2m_min"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();

    if (err) {
        if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100))) {
            g_weather.hasError = true;
            strlcpy(g_weather.errorMsg, "Hava JSON hatası", sizeof(g_weather.errorMsg));
            xSemaphoreGive(g_mutex);
        }
        return false;
    }

    WeatherData next{};
    next.valid = true;
    next.hasError = false;
    next.tempC = doc["current"]["temperature_2m"] | 0.0f;
    next.feelsLikeC = doc["current"]["apparent_temperature"] | 0.0f;
    next.humidity = doc["current"]["relative_humidity_2m"] | 0;
    next.windKmh = doc["current"]["wind_speed_10m"] | 0.0f;
    next.wmoCode = doc["current"]["weather_code"] | 0;
    next.isDay = (doc["current"]["is_day"] | 1) != 0;

    JsonArray times = doc["daily"]["time"].as<JsonArray>();
    JsonArray codes = doc["daily"]["weather_code"].as<JsonArray>();
    JsonArray tmax  = doc["daily"]["temperature_2m_max"].as<JsonArray>();
    JsonArray tmin  = doc["daily"]["temperature_2m_min"].as<JsonArray>();

    for (size_t i = 0; i < 4 && i < times.size(); ++i) {
        const char* ds = times[i] | "";
        strlcpy(next.daily[i].dateIso, ds, sizeof(next.daily[i].dateIso));
        next.daily[i].wmoCode = codes[i] | 0;
        next.daily[i].tempMax = tmax[i] | 0.0f;
        next.daily[i].tempMin = tmin[i] | 0.0f;
    }
    next.updatedAtMs = millis();

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(200))) {
        g_weather = next;
        xSemaphoreGive(g_mutex);
    }
    return true;
}

bool fetchGoogleNews(uint8_t topicIdx) {
    if (WiFi.status() != WL_CONNECTED) return false;
    if (topicIdx >= static_cast<uint8_t>(NewsTopic::COUNT)) topicIdx = 0;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.useHTTP10(true);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.setUserAgent("Mozilla/5.0");
    http.setTimeout(12000);

    if (!http.begin(client, NEWS_URLS[topicIdx])) return false;

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100))) {
            g_news.hasError = true;
            g_news.topic = topicIdx;
            snprintf(g_news.errorMsg, sizeof(g_news.errorMsg), "Haber HTTP %d", code);
            xSemaphoreGive(g_mutex);
        }
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    NewsData next{};
    next.topic = topicIdx;

    bool inTag = false;
    bool inItem = false;
    String tagBuf;
    String textBuf;
    String curTitle;
    String curSource;
    String curPubDate;

    uint32_t lastByteMs = millis();
    while ((http.connected() || stream->available() > 0) && (millis() - lastByteMs) < 4000) {
        if (!stream->available()) {
            delay(2);
            continue;
        }
        lastByteMs = millis();
        const char ch = static_cast<char>(stream->read());

        if (ch == '<') {
            inTag = true;
            tagBuf = "";
            continue;
        }

        if (inTag) {
            if (ch == '>') {
                inTag = false;
                String tagName = tagBuf;
                tagName.trim();
                int sp = tagName.indexOf(' ');
                if (sp > 0) tagName = tagName.substring(0, sp);

                if (tagName == "item") {
                    inItem = true;
                    curTitle = "";
                    curSource = "";
                    curPubDate = "";
                } else if (tagName == "/item" && inItem) {
                    inItem = false;
                    if (curTitle.length() > 0 && next.count < MAX_NEWS_ITEMS) {
                        String cleanTitle = decodeXmlText(curTitle);
                        String cleanSource = decodeXmlText(curSource);

                        int sep = cleanTitle.lastIndexOf(" - ");
                        if (sep > 12) {
                            if (cleanSource.length() == 0) {
                                cleanSource = cleanTitle.substring(sep + 3);
                            }
                            cleanTitle = cleanTitle.substring(0, sep);
                        }
                        if (cleanSource.length() == 0) {
                            cleanSource = "Google Haberler";
                        }

                        NewsItem& item = next.items[next.count++];
                        strlcpy(item.title, cleanTitle.c_str(), sizeof(item.title));
                        strlcpy(item.source, cleanSource.c_str(), sizeof(item.source));
                        formatPubDateTrt(curPubDate, item.timeStr, sizeof(item.timeStr));
                    }
                    if (next.count >= MAX_NEWS_ITEMS) break;
                } else if (inItem) {
                    if (tagName == "title" || tagName == "pubDate" || tagName == "source") {
                        textBuf = "";
                    } else if (tagName == "/title") {
                        curTitle = textBuf;
                    } else if (tagName == "/pubDate") {
                        curPubDate = textBuf;
                    } else if (tagName == "/source") {
                        curSource = textBuf;
                    }
                }
            } else if (tagBuf.length() < 96) {
                tagBuf += ch;
            }
        } else if (inItem && textBuf.length() < 420) {
            textBuf += ch;
        }
    }

    http.end();

    if (next.count > 0) {
        next.valid = true;
        next.hasError = false;
        next.updatedAtMs = millis();
        if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(200))) {
            g_news = next;
            xSemaphoreGive(g_mutex);
        }
        return true;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100))) {
        g_news.hasError = true;
        g_news.topic = topicIdx;
        strlcpy(g_news.errorMsg, "Haber alınamadı", sizeof(g_news.errorMsg));
        xSemaphoreGive(g_mutex);
    }
    return false;
}

float pctChange(float prev, float curr) {
    if (prev <= 0.001f || curr <= 0.001f) return 0.0f;
    return ((curr - prev) / prev) * 100.0f;
}

bool fetchMarket() {
    if (WiFi.status() != WL_CONNECTED) return false;

    MarketData prev{};
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100))) {
        prev = g_market;
        xSemaphoreGive(g_mutex);
    }

    float usd = 0.0f, eur = 0.0f, gbp = 0.0f, xau = 0.0f;
    bool gotFx = false;

    // 1) TCMB official rates XML (streaming parser)
    {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.useHTTP10(true);
        http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        http.setUserAgent("Mozilla/5.0");
        http.setTimeout(10000);

        if (http.begin(client, "https://www.tcmb.gov.tr/kurlar/today.xml")) {
            const int code = http.GET();
            if (code == HTTP_CODE_OK) {
                WiFiClient* stream = http.getStreamPtr();
                bool inTag = false;
                String tagBuf;
                String textBuf;
                char curCode[8] = "";
                float forexSell = 0.0f;
                float bankSell = 0.0f;

                uint32_t lastByteMs = millis();
                while ((http.connected() || stream->available() > 0) && (millis() - lastByteMs) < 4000) {
                    if (!stream->available()) {
                        delay(2);
                        continue;
                    }
                    lastByteMs = millis();
                    const char ch = static_cast<char>(stream->read());
                    if (ch == '<') {
                        inTag = true;
                        tagBuf = "";
                        continue;
                    }
                    if (inTag) {
                        if (ch == '>') {
                            inTag = false;
                            if (tagBuf.startsWith("Currency ") || tagBuf.startsWith("Currency\t")) {
                                curCode[0] = '\0';
                                forexSell = 0.0f;
                                bankSell = 0.0f;
                                int idx = tagBuf.indexOf("CurrencyCode=\"");
                                if (idx >= 0) {
                                    int endQ = tagBuf.indexOf('"', idx + 14);
                                    if (endQ > idx + 14) {
                                        String cc = tagBuf.substring(idx + 14, endQ);
                                        strlcpy(curCode, cc.c_str(), sizeof(curCode));
                                    }
                                }
                            } else if (tagBuf == "/Currency" && curCode[0] != '\0') {
                                float val = (forexSell > 0.0f) ? forexSell : bankSell;
                                if (strcmp(curCode, "USD") == 0) usd = val;
                                else if (strcmp(curCode, "EUR") == 0) eur = val;
                                else if (strcmp(curCode, "GBP") == 0) gbp = val;
                                curCode[0] = '\0';
                                if (usd > 0.0f && eur > 0.0f && gbp > 0.0f) {
                                    gotFx = true;
                                    break;
                                }
                            } else if (curCode[0] != '\0') {
                                if (tagBuf == "ForexSelling" || tagBuf == "BanknoteSelling") {
                                    textBuf = "";
                                } else if (tagBuf == "/ForexSelling") {
                                    textBuf.trim();
                                    forexSell = textBuf.toFloat();
                                } else if (tagBuf == "/BanknoteSelling") {
                                    textBuf.trim();
                                    bankSell = textBuf.toFloat();
                                }
                            }
                        } else if (tagBuf.length() < 160) {
                            tagBuf += ch;
                        }
                    } else if (curCode[0] != '\0' && textBuf.length() < 32) {
                        textBuf += ch;
                    }
                }
            }
            http.end();
        }
    }

    // 2) Live XAU/USD spot price (JSON)
    {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.useHTTP10(true);
        http.setUserAgent("Mozilla/5.0");
        http.setTimeout(8000);
        if (http.begin(client, "https://api.gold-api.com/price/XAU")) {
            if (http.GET() == HTTP_CODE_OK) {
                JsonDocument doc;
                if (!deserializeJson(doc, http.getStream())) {
                    xau = doc["price"] | 0.0f;
                }
            }
            http.end();
        }
    }

    if (gotFx || xau > 0.0f) {
        MarketData next = prev;
        next.valid = true;
        next.hasError = false;
        if (usd > 0.0f) {
            next.usdChg = prev.valid ? pctChange(prev.usdTry, usd) : 0.0f;
            next.usdTry = usd;
        }
        if (eur > 0.0f) {
            next.eurChg = prev.valid ? pctChange(prev.eurTry, eur) : 0.0f;
            next.eurTry = eur;
        }
        if (gbp > 0.0f) {
            next.gbpChg = prev.valid ? pctChange(prev.gbpTry, gbp) : 0.0f;
            next.gbpTry = gbp;
        }
        if (xau > 0.0f) {
            next.onsChg = prev.valid ? pctChange(prev.onsAltinUsd, xau) : 0.0f;
            next.onsAltinUsd = xau;
        }
        if (next.usdTry > 0.0f && next.onsAltinUsd > 0.0f) {
            const float gram = (next.onsAltinUsd / 31.1034768f) * next.usdTry;
            next.gramChg = (prev.valid && prev.gramAltinTry > 0.0f) ? pctChange(prev.gramAltinTry, gram) : 0.0f;
            next.gramAltinTry = gram;
        }
        next.updatedAtMs = millis();

        if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(200))) {
            g_market = next;
            xSemaphoreGive(g_mutex);
        }
        return true;
    }

    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100))) {
        g_market.hasError = true;
        strlcpy(g_market.errorMsg, "Piyasa verisi alınamadı", sizeof(g_market.errorMsg));
        xSemaphoreGive(g_mutex);
    }
    return false;
}

void netTask(void* param) {
    (void)param;
    uint32_t lastWeatherMs = 0;
    uint32_t lastNewsMs = 0;
    uint32_t lastMarketMs = 0;
    uint32_t lastWifiCheckMs = 0;

    for (;;) {
        const uint32_t now = millis();

        if (WiFi.status() != WL_CONNECTED && (now - lastWifiCheckMs) > 15000) {
            lastWifiCheckMs = now;
            WiFi.reconnect();
        }

        if (WiFi.status() == WL_CONNECTED) {
            if (g_reqTime) {
                g_reqTime = false;
                configTzTime(DEFAULT_TZ, NTP_SERVER_1, NTP_SERVER_2);
            }

            if (g_reqWeather || (lastWeatherMs != 0 && (now - lastWeatherMs) >= WEATHER_REFRESH_MS)) {
                g_reqWeather = false;
                g_fetching = true;
                if (fetchWeather()) {
                    lastWeatherMs = millis();
                } else if (lastWeatherMs == 0) {
                    lastWeatherMs = millis() - WEATHER_REFRESH_MS + 30000UL;
                }
                g_fetching = false;
            }

            if (g_reqMarket || (lastMarketMs != 0 && (now - lastMarketMs) >= MARKET_REFRESH_MS)) {
                g_reqMarket = false;
                g_fetching = true;
                if (fetchMarket()) {
                    lastMarketMs = millis();
                } else if (lastMarketMs == 0) {
                    lastMarketMs = millis() - MARKET_REFRESH_MS + 30000UL;
                }
                g_fetching = false;
            }

            if (g_reqNews || (lastNewsMs != 0 && (now - lastNewsMs) >= NEWS_REFRESH_MS)) {
                g_reqNews = false;
                const uint8_t topic = g_targetTopic;
                g_fetching = true;
                if (fetchGoogleNews(topic)) {
                    lastNewsMs = millis();
                } else if (lastNewsMs == 0) {
                    lastNewsMs = millis() - NEWS_REFRESH_MS + 30000UL;
                }
                g_fetching = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
} // namespace

void netInit(const DeviceConfig& cfg) {
    if (!g_mutex) {
        g_mutex = xSemaphoreCreateMutex();
    }
    g_cfg = cfg;
    g_targetTopic = cfg.newsTopic;
    g_news.topic = cfg.newsTopic;
}

void netUpdateConfig(const DeviceConfig& cfg) {
    if (!g_mutex) return;
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100))) {
        g_cfg = cfg;
        g_targetTopic = cfg.newsTopic;
        xSemaphoreGive(g_mutex);
    }
}

void netStartTask() {
    xTaskCreatePinnedToCore(netTask, "netTask", 10240, nullptr, 1, nullptr, 0);
}

void netRequestWeatherRefresh() { g_reqWeather = true; }

void netRequestNewsRefresh(uint8_t topicIdx) {
    g_targetTopic = topicIdx;
    g_reqNews = true;
}

void netRequestMarketRefresh() { g_reqMarket = true; }

void netRequestTimeSync() { g_reqTime = true; }

bool netCopyWeather(WeatherData& out) {
    if (!g_mutex) return false;
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(25))) {
        out = g_weather;
        xSemaphoreGive(g_mutex);
        return true;
    }
    return false;
}

bool netCopyNews(NewsData& out) {
    if (!g_mutex) return false;
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(25))) {
        out = g_news;
        xSemaphoreGive(g_mutex);
        return true;
    }
    return false;
}

bool netCopyMarket(MarketData& out) {
    if (!g_mutex) return false;
    if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(25))) {
        out = g_market;
        xSemaphoreGive(g_mutex);
        return true;
    }
    return false;
}

bool netIsFetching() { return g_fetching; }

int netGetRssi() {
    return (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -100;
}

bool netIsWifiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

const char* wmoToTurkish(int code) {
    if (code == 0) return "Açık";
    if (code == 1) return "Az Bulutlu";
    if (code == 2) return "Parçalı Bulutlu";
    if (code == 3) return "Çok Bulutlu";
    if (code == 45 || code == 48) return "Sisli";
    if (code >= 51 && code <= 57) return "Çisenti";
    if (code >= 61 && code <= 67) return "Yağmurlu";
    if (code >= 71 && code <= 77) return "Kar Yağışlı";
    if (code >= 80 && code <= 82) return "Sağanak Yağışlı";
    if (code >= 85 && code <= 86) return "Kar Sağanağı";
    if (code >= 95) return "Gök Gürültülü";
    return "Değişken";
}
