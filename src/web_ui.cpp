#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include "web_ui.h"
#include "config.h"
#include "net_services.h"
#include "storage_portal.h"

namespace {
WebServer g_web(80);
DeviceConfig* g_cfgPtr = nullptr;
bool g_started = false;
bool g_routesRegistered = false;
PageId g_activePage = PageId::CLOCK;
WebAction g_pending{WebActionType::NONE, 0};

const char WEB_UI_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 DeskHub v3.0 • Kontrol Paneli</title>
<style>
  :root{--bg:#0b0e14;--card:#141821;--in:#0d1118;--bd:#283040;--acc:#3893ff;--amb:#ffa400;--ok:#2dd264;--err:#fa3e3e;--gold:#f2c448;--tx:#f5f7fa;--mu:#8e95a2;}
  *{box-sizing:border-box}
  body{margin:0;background:var(--bg);color:var(--tx);font-family:system-ui,-apple-system,Segoe UI,sans-serif;padding:16px;}
  .wrap{max-width:1120px;margin:0 auto;}
  .hdr{display:flex;flex-wrap:wrap;gap:10px;justify-content:space-between;align-items:center;background:var(--card);border:1px solid var(--bd);border-radius:14px;padding:14px 18px;margin-bottom:14px;}
  .hdr h1{margin:0;font-size:19px;color:var(--acc);}
  .pills{display:flex;flex-wrap:wrap;gap:8px;font-size:12px;}
  .pill{padding:5px 10px;border-radius:8px;background:var(--in);border:1px solid var(--bd);}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(330px,1fr));gap:14px;}
  .card{background:var(--card);border:1px solid var(--bd);border-radius:14px;padding:16px;}
  .card h2{margin:0 0 12px;font-size:16px;display:flex;justify-content:space-between;align-items:center;}
  .themes{display:grid;grid-template-columns:repeat(auto-fit,minmax(92px,1fr));gap:8px;}
  .tbtn{padding:12px 6px;border-radius:10px;border:1px solid var(--bd);background:var(--in);color:var(--tx);cursor:pointer;text-align:center;font-size:12px;font-weight:600;}
  .tbtn.on{border-color:var(--amb);color:var(--amb);background:#24190a;}
  .pages{display:flex;flex-wrap:wrap;gap:6px;margin-top:12px;}
  .pbtn{padding:8px 12px;border-radius:8px;border:1px solid var(--bd);background:var(--in);color:var(--tx);cursor:pointer;font-size:12px;font-weight:600;}
  .pbtn.on{background:var(--acc);border-color:var(--acc);}
  label{display:block;font-size:12px;color:var(--mu);margin:10px 0 4px;}
  input,select{width:100%;padding:8px 10px;border-radius:8px;border:1px solid var(--bd);background:var(--in);color:var(--tx);font-size:13px;}
  .row{display:flex;gap:8px;align-items:center;}
  .row>*{flex:1;}
  .arow{display:grid;grid-template-columns:90px 1fr 110px 64px;gap:6px;align-items:center;margin-bottom:8px;background:var(--in);padding:8px;border-radius:9px;border:1px solid var(--bd);}
  .btn{width:100%;margin-top:12px;padding:10px 14px;border:none;border-radius:9px;font-size:13px;font-weight:700;cursor:pointer;color:#fff;background:var(--acc);}
  .btn.ok{background:var(--ok);color:#08160c;}
  .btn.amb{background:transparent;border:1px solid var(--amb);color:var(--amb);}
  .mkt{display:grid;grid-template-columns:1fr 1fr;gap:8px;}
  .mbox{background:var(--in);border:1px solid var(--bd);border-radius:10px;padding:10px 12px;}
  .mbox .k{font-size:11px;color:var(--mu);}
  .mbox .v{font-size:19px;font-weight:700;margin-top:3px;}
  .toast{position:fixed;bottom:16px;right:16px;background:var(--ok);color:#07150b;padding:10px 16px;border-radius:9px;font-weight:700;display:none;}
</style>
</head>
<body>
<div class="wrap">
  <div class="hdr">
        <h1>ESP32 DeskHub v3.0 • Kontrol Paneli</h1>
    <div class="pills">
      <span class="pill" id="pIp">IP: ...</span>
      <span class="pill" id="pRssi">Wi-Fi: ...</span>
      <span class="pill" id="pHeap">RAM: ...</span>
    </div>
  </div>

  <div class="grid">
    <div class="card">
      <h2>Saat Teması & Sayfa Kontrolü <span id="curThemeLbl" style="color:var(--amb);font-size:13px;"></span></h2>
      <div class="themes" id="themeList"></div>
      <label style="margin-top:14px;">Ekrandaki Aktif Sayfa</label>
      <div class="pages" id="pageList"></div>
      <div class="row" style="margin-top:10px;">
        <button class="btn amb" onclick="doAct('refresh')">Verileri Şimdi Yenile</button>
        <button class="btn amb" onclick="doAct('calibrate')">Dokunmatik Kalibre</button>
      </div>
    </div>

    <div class="card">
      <h2>Alarmlar (3 Bağımsız Alarm)</h2>
      <div id="alarmBox"></div>
      <h2 style="margin-top:14px;">Gece Modu & Ekran</h2>
      <div class="row">
        <div><label>Gece Modu</label><select id="nightEn"><option value="0">Kapalı</option><option value="1">Açık</option></select></div>
        <div><label>Başlangıç Saat</label><input id="nightS" type="number" min="0" max="23"></div>
        <div><label>Bitiş Saat</label><input id="nightE" type="number" min="0" max="23"></div>
      </div>
      <div class="row">
        <div><label>Ekran Yönü</label><select id="rot"><option value="1">Yatay 0°</option><option value="3">Yatay 180° (Ters)</option></select></div>
        <div><label>Buzzer GPIO (0=Yok)</label><input id="buzz" type="number" min="0" max="39"></div>
      </div>
    </div>

    <div class="card">
      <h2>Hava Durumu, Haber & Dünya Saatleri</h2>
      <div class="row">
        <div><label>Şehir Adı</label><input id="city"></div>
        <div><label>Enlem</label><input id="lat"></div>
        <div><label>Boylam</label><input id="lon"></div>
      </div>
      <label>Google Haber Kategorisi</label>
      <select id="topic">
        <option value="0">Gündem</option><option value="1">Teknoloji</option>
        <option value="2">Ekonomi</option><option value="3">Bilim</option><option value="4">Spor</option>
      </select>
      <label>4 Dünya Şehri (Ad / UTC Dakika Farkı, ör. İstanbul=180)</label>
      <div id="worldBox"></div>
      <button class="btn ok" onclick="saveAll()">Tüm Ayarları Kaydet & Uygula</button>
    </div>

    <div class="card">
      <h2>Spotify &amp; Bluetooth Hoparlör (PCM5102A I2S)</h2>
      <div class="row">
        <div><label>Bluetooth Hoparlör Adı</label><input id="btName" maxlength="23"></div>
        <div><label>Varsayılan Ses (%0-100)</label><input id="vol" type="number" min="0" max="100"></div>
      </div>
      <div class="row">
        <div><label>Spotify Client ID</label><input id="spId" maxlength="39" placeholder="Opsiyonel"></div>
        <div><label>Spotify Client Secret</label><input id="spSec" type="password" maxlength="39" placeholder="Opsiyonel"></div>
      </div>
      <label>Spotify Refresh Token (OAuth)</label>
      <input id="spRef" type="password" maxlength="159" placeholder="Opsiyonel — BT hoparlör için boş bırakabilirsiniz">
    </div>

    <div class="card">
      <h2>Anlık Piyasa (TCMB &amp; Altın) &amp; Sistem</h2>
      <div class="mkt" id="mktBox"></div>
      <div class="row" style="margin-top:12px;">
        <button class="btn amb" onclick="doAct('portal')">Wi-Fi Kurulum Portalı</button>
        <button class="btn amb" onclick="doAct('restart')">Yeniden Başlat</button>
      </div>
    </div>
  </div>
</div>
<div class="toast" id="toast">Kaydedildi!</div>
<script>
const THEMES=['Yaprak','Nixie Tüp','Dijital LED','Rolex','Patek','Mario','Tetris'];
const PAGES=[[0,'Saat'],[1,'Hava'],[2,'Piyasa'],[3,'Dünya'],[4,'Alarm'],[5,'Haber'],[6,'Odak'],[7,'Müzik'],[8,'Menü'],[9,'Ayar'],[10,'Gece']];
let state=null;

function toast(msg){const t=document.getElementById('toast');t.textContent=msg;t.style.display='block';setTimeout(()=>t.style.display='none',2200);}

async function load(){
  const r=await fetch('/api/state');
  state=await r.json();
  document.getElementById('pIp').textContent='IP: '+state.ip;
  document.getElementById('pRssi').textContent='Wi-Fi: '+state.rssi+' dBm';
  document.getElementById('pHeap').textContent='Boş RAM: '+Math.round(state.heap/1024)+' KB';
  document.getElementById('curThemeLbl').textContent=THEMES[state.theme]||'';

  const tl=document.getElementById('themeList'); tl.innerHTML='';
  THEMES.forEach((n,i)=>{
    const b=document.createElement('button');
    b.className='tbtn'+(state.theme===i?' on':'');
    b.textContent=n;
    b.onclick=()=>setTheme(i);
    tl.appendChild(b);
  });

  const pl=document.getElementById('pageList'); pl.innerHTML='';
  PAGES.forEach(([id,n])=>{
    const b=document.createElement('button');
    b.className='pbtn'+(state.page===id?' on':'');
    b.textContent=n;
    b.onclick=()=>setPage(id);
    pl.appendChild(b);
  });

  const ab=document.getElementById('alarmBox'); ab.innerHTML='';
  state.alarms.forEach((a,i)=>{
    const hh=String(a.h).padStart(2,'0'), mm=String(a.m).padStart(2,'0');
    ab.innerHTML+=`<div class="arow">
      <input type="time" id="at${i}" value="${hh}:${mm}">
      <input id="al${i}" maxlength="22" value="${a.l}">
      <select id="ar${i}"><option value="0"${a.r==0?' selected':''}>Her Gün</option><option value="1"${a.r==1?' selected':''}>Hafta İçi</option><option value="2"${a.r==2?' selected':''}>Hafta Sonu</option></select>
      <select id="ae${i}"><option value="1"${a.e?' selected':''}>Açık</option><option value="0"${!a.e?' selected':''}>Kapalı</option></select>
    </div>`;
  });

  document.getElementById('nightEn').value=state.nightEn?'1':'0';
  document.getElementById('nightS').value=state.nightS;
  document.getElementById('nightE').value=state.nightE;
  document.getElementById('rot').value=String(state.rot);
  document.getElementById('buzz').value=state.buzz;
  document.getElementById('city').value=state.city;
  document.getElementById('lat').value=state.lat;
  document.getElementById('lon').value=state.lon;
  document.getElementById('topic').value=String(state.topic);
  document.getElementById('btName').value=state.btName||'ESP32-DeskHub-BT';
  document.getElementById('vol').value=state.vol??75;
  document.getElementById('spId').value=state.spId||'';
  document.getElementById('spSec').value=state.spSec||'';
  document.getElementById('spRef').value=state.spRef||'';

  const wb=document.getElementById('worldBox'); wb.innerHTML='';
  state.world.forEach((w,i)=>{
    wb.innerHTML+=`<div class="row" style="margin-bottom:6px;"><input id="wn${i}" value="${w.n}"><input id="wo${i}" type="number" value="${w.o}" style="max-width:110px;"></div>`;
  });

  const m=state.market;
  document.getElementById('mktBox').innerHTML=`
    <div class="mbox"><div class="k">USD / TRY (TCMB)</div><div class="v">${m.valid?m.usd.toFixed(2)+' ₺':'--'}</div></div>
    <div class="mbox"><div class="k">EUR / TRY (TCMB)</div><div class="v">${m.valid?m.eur.toFixed(2)+' ₺':'--'}</div></div>
    <div class="mbox"><div class="k">Gram Altın</div><div class="v" style="color:var(--gold)">${m.valid?Math.round(m.gram)+' ₺':'--'}</div></div>
    <div class="mbox"><div class="k">Ons Altın (XAU)</div><div class="v" style="color:var(--gold)">${m.valid?Math.round(m.ons)+' $':'--'}</div></div>`;
}

async function setTheme(i){await fetch('/api/theme?id='+i,{method:'POST'});toast('Tema değiştirildi');load();}
async function setPage(id){await fetch('/api/page?id='+id,{method:'POST'});toast('Sayfa değiştirildi');load();}
async function doAct(cmd){await fetch('/api/action?cmd='+cmd,{method:'POST'});toast('Komut gönderildi');}

async function saveAll(){
  const body={
    city:document.getElementById('city').value,
    lat:parseFloat(document.getElementById('lat').value),
    lon:parseFloat(document.getElementById('lon').value),
    topic:parseInt(document.getElementById('topic').value),
    rot:parseInt(document.getElementById('rot').value),
    nightEn:document.getElementById('nightEn').value==='1',
    nightS:parseInt(document.getElementById('nightS').value),
    nightE:parseInt(document.getElementById('nightE').value),
    buzz:parseInt(document.getElementById('buzz').value),
    btName:document.getElementById('btName').value,
    vol:parseInt(document.getElementById('vol').value||75),
    spId:document.getElementById('spId').value,
    spSec:document.getElementById('spSec').value,
    spRef:document.getElementById('spRef').value,
    alarms:[0,1,2].map(i=>{
      const p=document.getElementById('at'+i).value.split(':');
      return {h:parseInt(p[0]||0),m:parseInt(p[1]||0),l:document.getElementById('al'+i).value,r:parseInt(document.getElementById('ar'+i).value),e:document.getElementById('ae'+i).value==='1'};
    }),
    world:[0,1,2,3].map(i=>({n:document.getElementById('wn'+i).value,o:parseInt(document.getElementById('wo'+i).value||0)}))
  };
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  toast('Tüm ayarlar kaydedildi!');
  load();
}
load();
setInterval(load, 8000);
</script>
</body>
</html>)rawliteral";

void handleState() {
    if (!g_cfgPtr) {
        g_web.send(500, "text/plain", "No config");
        return;
    }
    MarketData mk{};
    netCopyMarket(mk);

    JsonDocument doc;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = netGetRssi();
    doc["heap"] = ESP.getFreeHeap();
    doc["theme"] = g_cfgPtr->clockTheme;
    doc["page"] = static_cast<uint8_t>(g_activePage);
    doc["city"] = g_cfgPtr->city;
    doc["lat"] = g_cfgPtr->lat;
    doc["lon"] = g_cfgPtr->lon;
    doc["topic"] = g_cfgPtr->newsTopic;
    doc["rot"] = g_cfgPtr->rotation;
    doc["nightEn"] = g_cfgPtr->nightEnabled;
    doc["nightS"] = g_cfgPtr->nightStartHour;
    doc["nightE"] = g_cfgPtr->nightEndHour;
    doc["buzz"] = g_cfgPtr->buzzerPin;
    doc["btName"] = g_cfgPtr->btName;
    doc["vol"] = g_cfgPtr->volume;
    doc["spId"] = g_cfgPtr->spClientId;
    doc["spSec"] = g_cfgPtr->spClientSecret;
    doc["spRef"] = g_cfgPtr->spRefreshToken;

    JsonArray alarms = doc["alarms"].to<JsonArray>();
    for (int i = 0; i < 3; ++i) {
        JsonObject ao = alarms.add<JsonObject>();
        ao["h"] = g_cfgPtr->alarms[i].hour;
        ao["m"] = g_cfgPtr->alarms[i].minute;
        ao["e"] = g_cfgPtr->alarms[i].enabled;
        ao["r"] = g_cfgPtr->alarms[i].repeatMode;
        ao["l"] = g_cfgPtr->alarms[i].label;
    }

    JsonArray world = doc["world"].to<JsonArray>();
    for (int i = 0; i < 4; ++i) {
        JsonObject wo = world.add<JsonObject>();
        wo["n"] = g_cfgPtr->worldCities[i].name;
        wo["o"] = g_cfgPtr->worldCities[i].utcOffsetMin;
    }

    JsonObject m = doc["market"].to<JsonObject>();
    m["valid"] = mk.valid;
    m["usd"] = mk.usdTry;
    m["eur"] = mk.eurTry;
    m["gbp"] = mk.gbpTry;
    m["gram"] = mk.gramAltinTry;
    m["ons"] = mk.onsAltinUsd;

    String out;
    serializeJson(doc, out);
    g_web.send(200, "application/json", out);
}

void handleTheme() {
    if (!g_cfgPtr) return;
    int id = g_web.arg("id").toInt();
    if (id >= 0 && id < static_cast<int>(ClockTheme::COUNT)) {
        g_cfgPtr->clockTheme = static_cast<uint8_t>(id);
        saveConfig(*g_cfgPtr);
        g_pending = {WebActionType::APPLY_THEME, static_cast<uint8_t>(id)};
    }
    g_web.send(200, "text/plain", "OK");
}

void handlePage() {
    int id = g_web.arg("id").toInt();
    if (id >= 0 && id < static_cast<int>(PageId::COUNT)) {
        g_pending = {WebActionType::SWITCH_PAGE, static_cast<uint8_t>(id)};
    }
    g_web.send(200, "text/plain", "OK");
}

void handleConfigPost() {
    if (!g_cfgPtr) return;
    JsonDocument doc;
    if (deserializeJson(doc, g_web.arg("plain"))) {
        g_web.send(400, "text/plain", "Bad JSON");
        return;
    }

    const uint8_t oldRot = g_cfgPtr->rotation;
    const char* city = doc["city"] | g_cfgPtr->city;
    strlcpy(g_cfgPtr->city, city, sizeof(g_cfgPtr->city));
    g_cfgPtr->lat = doc["lat"] | g_cfgPtr->lat;
    g_cfgPtr->lon = doc["lon"] | g_cfgPtr->lon;
    g_cfgPtr->newsTopic = (doc["topic"] | g_cfgPtr->newsTopic) % static_cast<uint8_t>(NewsTopic::COUNT);

    uint8_t rot = doc["rot"] | g_cfgPtr->rotation;
    g_cfgPtr->rotation = (rot == 3) ? 3 : 1;
    g_cfgPtr->nightEnabled = doc["nightEn"] | g_cfgPtr->nightEnabled;
    g_cfgPtr->nightStartHour = (doc["nightS"] | g_cfgPtr->nightStartHour) % 24;
    g_cfgPtr->nightEndHour = (doc["nightE"] | g_cfgPtr->nightEndHour) % 24;
    g_cfgPtr->buzzerPin = doc["buzz"] | g_cfgPtr->buzzerPin;
    strlcpy(g_cfgPtr->btName, doc["btName"] | g_cfgPtr->btName, sizeof(g_cfgPtr->btName));
    int vol = doc["vol"] | g_cfgPtr->volume;
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    g_cfgPtr->volume = static_cast<uint8_t>(vol);
    strlcpy(g_cfgPtr->spClientId, doc["spId"] | g_cfgPtr->spClientId, sizeof(g_cfgPtr->spClientId));
    strlcpy(g_cfgPtr->spClientSecret, doc["spSec"] | g_cfgPtr->spClientSecret, sizeof(g_cfgPtr->spClientSecret));
    strlcpy(g_cfgPtr->spRefreshToken, doc["spRef"] | g_cfgPtr->spRefreshToken, sizeof(g_cfgPtr->spRefreshToken));

    JsonArray alarms = doc["alarms"].as<JsonArray>();
    for (int i = 0; i < 3 && i < static_cast<int>(alarms.size()); ++i) {
        g_cfgPtr->alarms[i].hour = (alarms[i]["h"] | 0) % 24;
        g_cfgPtr->alarms[i].minute = (alarms[i]["m"] | 0) % 60;
        g_cfgPtr->alarms[i].enabled = alarms[i]["e"] | false;
        g_cfgPtr->alarms[i].repeatMode = (alarms[i]["r"] | 0) % 3;
        const char* lbl = alarms[i]["l"] | "Alarm";
        strlcpy(g_cfgPtr->alarms[i].label, lbl, sizeof(g_cfgPtr->alarms[i].label));
    }

    JsonArray world = doc["world"].as<JsonArray>();
    for (int i = 0; i < 4 && i < static_cast<int>(world.size()); ++i) {
        const char* wn = world[i]["n"] | "Şehir";
        strlcpy(g_cfgPtr->worldCities[i].name, wn, sizeof(g_cfgPtr->worldCities[i].name));
        g_cfgPtr->worldCities[i].utcOffsetMin = world[i]["o"] | 0;
    }

    saveConfig(*g_cfgPtr);
    netUpdateConfig(*g_cfgPtr);
    netRequestWeatherRefresh();
    netRequestNewsRefresh(g_cfgPtr->newsTopic);

    g_pending = {
        (g_cfgPtr->rotation != oldRot) ? WebActionType::APPLY_ROTATION : WebActionType::CONFIG_UPDATED,
        g_cfgPtr->rotation
    };
    g_web.send(200, "text/plain", "OK");
}

void handleAction() {
    String cmd = g_web.arg("cmd");
    if (cmd == "refresh") g_pending = {WebActionType::REFRESH_ALL, 0};
    else if (cmd == "calibrate") g_pending = {WebActionType::START_CALIBRATION, 0};
    else if (cmd == "portal") g_pending = {WebActionType::START_PORTAL, 0};
    else if (cmd == "restart") {
        g_web.send(200, "text/plain", "Restarting");
        delay(200);
        ESP.restart();
        return;
    }
    g_web.send(200, "text/plain", "OK");
}
} // namespace

void webUiStart(DeviceConfig& cfg) {
    g_cfgPtr = &cfg;
    if (g_started) return;

    MDNS.begin(MDNS_HOSTNAME);
    if (!g_routesRegistered) {
        g_web.on("/", HTTP_GET, []() { g_web.send_P(200, "text/html; charset=utf-8", WEB_UI_HTML); });
        g_web.on("/api/state", HTTP_GET, handleState);
        g_web.on("/api/theme", HTTP_POST, handleTheme);
        g_web.on("/api/page", HTTP_POST, handlePage);
        g_web.on("/api/config", HTTP_POST, handleConfigPost);
        g_web.on("/api/action", HTTP_POST, handleAction);
        g_routesRegistered = true;
    }
    g_web.begin();
    g_started = true;
}

void webUiStop() {
    if (!g_started) return;
    g_web.stop();
    MDNS.end();
    g_started = false;
}

void webUiSetActivePage(PageId page) { g_activePage = page; }

void webUiLoop() {
    if (g_started) {
        g_web.handleClient();
    }
}

WebAction webUiPollAction() {
    WebAction out = g_pending;
    g_pending = {WebActionType::NONE, 0};
    return out;
}
