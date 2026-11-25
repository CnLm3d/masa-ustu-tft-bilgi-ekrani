#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include "storage_portal.h"
#include "config.h"
#include "fonts/SegoeUI14TR.h"
#include "fonts/SegoeUIBold18TR.h"

namespace {
constexpr char NVS_NS[] = "deskhub";
constexpr char AP_SSID[] = "ESP32-DeskHub";

String htmlEscape(const char* src) {
    String out;
    for (size_t i = 0; src[i] != '\0'; ++i) {
        char c = src[i];
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out += c;
    }
    return out;
}

void drawPortalScreen(TFT_eSPI& tft, const char* apName, const char* ipStr) {
    tft.fillScreen(COL_BG);
    tft.fillRoundRect(14, 14, SCREEN_W - 28, SCREEN_H - 28, 10, COL_SURFACE);
    tft.drawRoundRect(14, 14, SCREEN_W - 28, SCREEN_H - 28, 10, COL_ACCENT);

    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COL_ACCENT, COL_SURFACE);
    tft.drawString("Wi-Fi & Şehir Kurulumu", SCREEN_W / 2, 28);
    tft.unloadFont();

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString("1. Telefon veya PC ile ağa bağlan:", 28, 66);

    tft.setTextColor(COL_ACCENT_2, COL_SURFACE);
    tft.drawString(String("   Ağ Adı: ") + apName + " (Şifresiz)", 28, 90);

    tft.setTextColor(COL_TEXT, COL_SURFACE);
    tft.drawString("2. Tarayıcı otomatik açılmazsa adres:", 28, 122);

    tft.setTextColor(COL_OK, COL_SURFACE);
    tft.drawString(String("   http://") + ipStr, 28, 146);

    tft.setTextColor(COL_MUTED, COL_SURFACE);
    tft.drawString("3. Wi-Fi, şehir ve haber kategorisini", 28, 178);
    tft.drawString("   kaydettiğinizde cihaz yeniden başlar.", 28, 198);
    tft.unloadFont();
}

String buildSetupPage(const DeviceConfig& cfg) {
    String html = R"rawliteral(<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 DeskHub Kurulum</title>
<style>
  body{font-family:system-ui,-apple-system,Segoe UI,sans-serif;background:#0c0e13;color:#f5f7fa;margin:0;padding:18px;}
  .card{max-width:440px;margin:0 auto;background:#14171d;border:1px solid #2d333f;border-radius:14px;padding:20px;}
  h1{margin:0 0 6px;font-size:20px;color:#3893ff;}
  p.sub{margin:0 0 18px;font-size:13px;color:#8e95a2;}
  label{display:block;font-size:13px;margin:12px 0 4px;color:#cbd5e1;}
  input,select{width:100%;box-sizing:border-box;padding:10px 12px;border-radius:8px;border:1px solid #2d333f;background:#0c0e13;color:#f5f7fa;font-size:14px;}
  .row{display:flex;gap:10px;}
  .row > div{flex:1;}
  button{width:100%;margin-top:20px;padding:12px;border:none;border-radius:10px;background:#3893ff;color:#fff;font-size:15px;font-weight:600;cursor:pointer;}
</style>
</head>
<body>
<div class="card">
  <h1>ESP32 DeskHub Kurulum</h1>
  <p class="sub">Wi-Fi bilgilerini ve hava durumu şehrini ayarlayın.</p>
  <form method="POST" action="/save">
    <label>Wi-Fi Ağ Adı (SSID)</label>
    <input name="ssid" required maxlength="32" value=")rawliteral";
    html += htmlEscape(cfg.ssid);
    html += R"rawliteral(">
    <label>Wi-Fi Şifresi</label>
    <input name="pass" type="password" maxlength="64" value=")rawliteral";
    html += htmlEscape(cfg.pass);
    html += R"rawliteral(">
    <label>Hazır Şehir Seçimi</label>
    <select id="preset" onchange="pickCity(this.value)">
      <option value="">-- Listeden Seç veya Elle Gir --</option>
      <option value="İstanbul|41.0082|28.9784">İstanbul</option>
      <option value="Ankara|39.9334|32.8597">Ankara</option>
      <option value="İzmir|38.4237|27.1428">İzmir</option>
      <option value="Bursa|40.1885|29.0610">Bursa</option>
      <option value="Antalya|36.8969|30.7133">Antalya</option>
      <option value="Adana|37.0000|35.3213">Adana</option>
      <option value="Konya|37.8746|32.4932">Konya</option>
      <option value="Trabzon|41.0027|39.7168">Trabzon</option>
      <option value="Gaziantep|37.0662|37.3833">Gaziantep</option>
      <option value="Eskişehir|39.7767|30.5206">Eskişehir</option>
    </select>
    <label>Şehir Adı</label>
    <input id="city" name="city" required maxlength="31" value=")rawliteral";
    html += htmlEscape(cfg.city);
    html += R"rawliteral(">
    <div class="row">
      <div><label>Enlem (Lat)</label><input id="lat" name="lat" required value=")rawliteral";
    html += String(cfg.lat, 4);
    html += R"rawliteral("></div>
      <div><label>Boylam (Lon)</label><input id="lon" name="lon" required value=")rawliteral";
    html += String(cfg.lon, 4);
    html += R"rawliteral("></div>
    </div>
    <label>Varsayılan Google Haber Kategorisi</label>
    <select name="topic">
      <option value="0")rawliteral";
    if (cfg.newsTopic == 0) html += " selected";
    html += R"rawliteral(>Gündem</option><option value="1")rawliteral";
    if (cfg.newsTopic == 1) html += " selected";
    html += R"rawliteral(>Teknoloji</option><option value="2")rawliteral";
    if (cfg.newsTopic == 2) html += " selected";
    html += R"rawliteral(>Ekonomi</option><option value="3")rawliteral";
    if (cfg.newsTopic == 3) html += " selected";
    html += R"rawliteral(>Bilim</option><option value="4")rawliteral";
    if (cfg.newsTopic == 4) html += " selected";
    html += R"rawliteral(>Spor</option>
    </select>
    <button type="submit">Kaydet ve Yeniden Başlat</button>
  </form>
</div>
<script>
function pickCity(v){
  if(!v) return;
  var p=v.split('|');
  document.getElementById('city').value=p[0];
  document.getElementById('lat').value=p[1];
  document.getElementById('lon').value=p[2];
}
</script>
</body>
</html>)rawliteral";
    return html;
}
} // namespace

void loadConfig(DeviceConfig& cfg) {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    String ssid = prefs.getString("ssid", DEFAULT_WIFI_SSID);
    String pass = prefs.getString("pass", DEFAULT_WIFI_PASS);
    String city = prefs.getString("city", DEFAULT_CITY_NAME);
    cfg.lat = prefs.getFloat("lat", DEFAULT_CITY_LAT);
    cfg.lon = prefs.getFloat("lon", DEFAULT_CITY_LON);
    cfg.newsTopic = prefs.getUChar("topic", 0);
    cfg.clockTheme = prefs.getUChar("theme", 0);
    cfg.rotation = prefs.getUChar("rot", 1);
    cfg.nightEnabled = prefs.getBool("nightEn", false);
    cfg.nightStartHour = prefs.getUChar("nightS", 23);
    cfg.nightEndHour = prefs.getUChar("nightE", 7);
    cfg.buzzerPin = prefs.getUChar("buzz", DEFAULT_BUZZER_PIN);

    const uint8_t defH[3] = {7, 9, 18};
    const uint8_t defM[3] = {30, 0, 45};
    const bool defEn[3] = {false, false, false};
    const uint8_t defRep[3] = {1, 2, 0};
    const char* const defLbl[3] = {"Hafta İçi", "Hafta Sonu", "Mola / İlaç"};

    for (int i = 0; i < 3; ++i) {
        char kh[8], km[8], ke[8], kr[8], kl[8];
        snprintf(kh, sizeof(kh), "a%dh", i);
        snprintf(km, sizeof(km), "a%dm", i);
        snprintf(ke, sizeof(ke), "a%de", i);
        snprintf(kr, sizeof(kr), "a%dr", i);
        snprintf(kl, sizeof(kl), "a%dl", i);
        cfg.alarms[i].hour = prefs.getUChar(kh, defH[i]) % 24;
        cfg.alarms[i].minute = prefs.getUChar(km, defM[i]) % 60;
        cfg.alarms[i].enabled = prefs.getBool(ke, defEn[i]);
        cfg.alarms[i].repeatMode = prefs.getUChar(kr, defRep[i]) % 3;
        String lbl = prefs.getString(kl, defLbl[i]);
        strlcpy(cfg.alarms[i].label, lbl.c_str(), sizeof(cfg.alarms[i].label));
    }

    const char* const defCity[4] = {"İstanbul", "Londra", "New York", "Tokyo"};
    const int16_t defOff[4] = {180, 0, -300, 540};
    for (int i = 0; i < 4; ++i) {
        char kn[8], ko[8];
        snprintf(kn, sizeof(kn), "w%dn", i);
        snprintf(ko, sizeof(ko), "w%do", i);
        String wn = prefs.getString(kn, defCity[i]);
        strlcpy(cfg.worldCities[i].name, wn.c_str(), sizeof(cfg.worldCities[i].name));
        cfg.worldCities[i].utcOffsetMin = prefs.getShort(ko, defOff[i]);
    }
    prefs.end();

    strlcpy(cfg.ssid, ssid.c_str(), sizeof(cfg.ssid));
    strlcpy(cfg.pass, pass.c_str(), sizeof(cfg.pass));
    strlcpy(cfg.city, city.c_str(), sizeof(cfg.city));
    if (cfg.newsTopic >= static_cast<uint8_t>(NewsTopic::COUNT)) cfg.newsTopic = 0;
    if (cfg.clockTheme >= static_cast<uint8_t>(ClockTheme::COUNT)) cfg.clockTheme = 0;
    if (cfg.rotation != 1 && cfg.rotation != 3) cfg.rotation = 1;
    cfg.nightStartHour %= 24;
    cfg.nightEndHour %= 24;
}

void saveConfig(const DeviceConfig& cfg) {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putString("ssid", cfg.ssid);
    prefs.putString("pass", cfg.pass);
    prefs.putString("city", cfg.city);
    prefs.putFloat("lat", cfg.lat);
    prefs.putFloat("lon", cfg.lon);
    prefs.putUChar("topic", cfg.newsTopic);
    prefs.putUChar("theme", cfg.clockTheme);
    prefs.putUChar("rot", cfg.rotation);
    prefs.putBool("nightEn", cfg.nightEnabled);
    prefs.putUChar("nightS", cfg.nightStartHour);
    prefs.putUChar("nightE", cfg.nightEndHour);
    prefs.putUChar("buzz", cfg.buzzerPin);

    for (int i = 0; i < 3; ++i) {
        char kh[8], km[8], ke[8], kr[8], kl[8];
        snprintf(kh, sizeof(kh), "a%dh", i);
        snprintf(km, sizeof(km), "a%dm", i);
        snprintf(ke, sizeof(ke), "a%de", i);
        snprintf(kr, sizeof(kr), "a%dr", i);
        snprintf(kl, sizeof(kl), "a%dl", i);
        prefs.putUChar(kh, cfg.alarms[i].hour);
        prefs.putUChar(km, cfg.alarms[i].minute);
        prefs.putBool(ke, cfg.alarms[i].enabled);
        prefs.putUChar(kr, cfg.alarms[i].repeatMode);
        prefs.putString(kl, cfg.alarms[i].label);
    }

    for (int i = 0; i < 4; ++i) {
        char kn[8], ko[8];
        snprintf(kn, sizeof(kn), "w%dn", i);
        snprintf(ko, sizeof(ko), "w%do", i);
        prefs.putString(kn, cfg.worldCities[i].name);
        prefs.putShort(ko, cfg.worldCities[i].utcOffsetMin);
    }
    prefs.end();
}

bool needsSetup(const DeviceConfig& cfg) {
    return (cfg.ssid[0] == '\0') || (strcmp(cfg.ssid, "WIFI_ADINIZ") == 0);
}

void runSetupPortal(TFT_eSPI& tft, DeviceConfig& cfg) {
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    delay(150);

    IPAddress ip = WiFi.softAPIP();
    char ipBuf[20];
    snprintf(ipBuf, sizeof(ipBuf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    drawPortalScreen(tft, AP_SSID, ipBuf);

    DNSServer dns;
    dns.start(53, "*", ip);

    WebServer server(80);
    bool saved = false;

    server.on("/", HTTP_GET, [&]() {
        server.send(200, "text/html; charset=utf-8", buildSetupPage(cfg));
    });

    server.on("/save", HTTP_POST, [&]() {
        String s = server.arg("ssid");
        String p = server.arg("pass");
        String c = server.arg("city");
        float la = server.arg("lat").toFloat();
        float lo = server.arg("lon").toFloat();
        int tp   = server.arg("topic").toInt();

        s.trim();
        c.trim();
        if (s.length() > 0) {
            strlcpy(cfg.ssid, s.c_str(), sizeof(cfg.ssid));
            strlcpy(cfg.pass, p.c_str(), sizeof(cfg.pass));
            if (c.length() > 0) strlcpy(cfg.city, c.c_str(), sizeof(cfg.city));
            if (la != 0.0f || lo != 0.0f) {
                cfg.lat = la;
                cfg.lon = lo;
            }
            if (tp >= 0 && tp < static_cast<int>(NewsTopic::COUNT)) {
                cfg.newsTopic = static_cast<uint8_t>(tp);
            }
            saveConfig(cfg);
            saved = true;
            server.send(200, "text/html; charset=utf-8",
                        "<html><body style='background:#0c0e13;color:#2dd264;font-family:sans-serif;text-align:center;padding:40px;'>"
                        "<h2>Ayarlar kaydedildi!</h2><p style='color:#f5f7fa;'>Cihaz yeniden başlatılıyor...</p></body></html>");
        } else {
            server.send(400, "text/plain; charset=utf-8", "SSID boş olamaz.");
        }
    });

    server.onNotFound([&]() {
        server.sendHeader("Location", String("http://") + ipBuf + "/", true);
        server.send(302, "text/plain", "");
    });

    server.begin();
    while (!saved) {
        dns.processNextRequest();
        server.handleClient();
        delay(5);
    }

    delay(1200);
    server.stop();
    dns.stop();
    WiFi.softAPdisconnect(true);
    ESP.restart();
}

bool loadTouchCal(uint16_t cal[5]) {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    const bool ok = prefs.getBool("tcalOk", false);
    if (ok) {
        for (int i = 0; i < 5; ++i) {
            char key[8];
            snprintf(key, sizeof(key), "tcal%d", i);
            cal[i] = prefs.getUShort(key, 0);
        }
    }
    prefs.end();
    return ok;
}

void saveTouchCal(const uint16_t cal[5]) {
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    for (int i = 0; i < 5; ++i) {
        char key[8];
        snprintf(key, sizeof(key), "tcal%d", i);
        prefs.putUShort(key, cal[i]);
    }
    prefs.putBool("tcalOk", true);
    prefs.end();
}

void runTouchCalibration(TFT_eSPI& tft) {
    uint16_t cal[5] = {0};

    tft.fillScreen(COL_BG);
    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_ACCENT, COL_BG);
    tft.drawString("Dokunmatik Kalibrasyon", SCREEN_W / 2, 96);
    tft.unloadFont();

    tft.loadFont(SegoeUI14TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("Köşelerde çıkan oklara sırayla dokunun", SCREEN_W / 2, 130);
    tft.unloadFont();

    tft.calibrateTouch(cal, COL_ACCENT_2, COL_BG, 16);
    tft.setTouch(cal);
    saveTouchCal(cal);

    tft.fillScreen(COL_BG);
    tft.loadFont(SegoeUIBold18TR);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_OK, COL_BG);
    tft.drawString("Kalibrasyon kaydedildi!", SCREEN_W / 2, SCREEN_H / 2);
    tft.unloadFont();
    delay(700);
}
