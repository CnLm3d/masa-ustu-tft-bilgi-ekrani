#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#ifndef HAS_WIFI_OTA
#define HAS_WIFI_OTA 0
#endif
#if HAS_WIFI_OTA
#include <ArduinoOTA.h>
#include <Update.h>
#endif
#include "web_ui.h"
#include "audio_radio.h"
#include "config.h"
#include "led_strip.h"
#include "net_services.h"
#include "storage_portal.h"

namespace {
WebServer g_web(80);
DeviceConfig* g_cfgPtr = nullptr;
OtaProgressCb g_otaCb = nullptr;
bool g_started = false;
bool g_routesRegistered = false;
PageId g_activePage = PageId::CLOCK;
WebAction g_pending{WebActionType::NONE, 0};
#if HAS_WIFI_OTA
size_t g_otaTotal = 0;
size_t g_otaWritten = 0;
uint8_t g_otaLastPct = 255;
#endif

const char WEB_UI_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 DeskHub v5.0 • Kontrol Paneli</title>
<style>
  :root{--bg:#0b0e14;--card:#141821;--in:#0d1118;--bd:#283040;--acc:#3893ff;--amb:#ffa400;--ok:#2dd264;--err:#fa3e3e;--gold:#f2c448;--tx:#f5f7fa;--mu:#8e95a2;}
  :root[data-theme="light"]{--bg:#e4ebf2;--card:#ffffff;--in:#eef3f8;--bd:#9fb0c4;--acc:#0b5ed7;--amb:#c46a00;--ok:#0e8a38;--err:#c91c1c;--gold:#9a6b00;--tx:#081421;--mu:#4a596b;}
  *{box-sizing:border-box}
  body{margin:0;background:var(--bg);color:var(--tx);font-family:system-ui,-apple-system,Segoe UI,sans-serif;padding:16px;transition:background .2s,color .2s;}
  .wrap{max-width:1180px;margin:0 auto;}
  .hdr{display:flex;flex-wrap:wrap;gap:10px;justify-content:space-between;align-items:center;background:var(--card);border:1px solid var(--bd);border-radius:14px;padding:14px 18px;margin-bottom:14px;}
  .hdr h1{margin:0;font-size:19px;color:var(--acc);}
  .pills{display:flex;flex-wrap:wrap;gap:8px;font-size:12px;}
  .pill{padding:5px 10px;border-radius:8px;background:var(--in);border:1px solid var(--bd);}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:14px;}
  .card{background:var(--card);border:1px solid var(--bd);border-radius:14px;padding:16px;}
  .card h2{margin:0 0 12px;font-size:16px;display:flex;justify-content:space-between;align-items:center;}
  .themes{display:grid;grid-template-columns:repeat(auto-fit,minmax(92px,1fr));gap:8px;}
  .tbtn{padding:11px 6px;border-radius:10px;border:1px solid var(--bd);background:var(--in);color:var(--tx);cursor:pointer;text-align:center;font-size:12px;font-weight:600;}
  .tbtn.on{border-color:var(--amb);color:var(--amb);background:rgba(255,164,0,0.14);}
  .pages{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px;}
  .pbtn{padding:8px 11px;border-radius:8px;border:1px solid var(--bd);background:var(--in);color:var(--tx);cursor:pointer;font-size:12px;font-weight:600;}
  .pbtn.on{background:var(--acc);border-color:var(--acc);color:#fff;}
  label{display:block;font-size:12px;color:var(--mu);margin:10px 0 4px;}
  input,select{width:100%;padding:8px 10px;border-radius:8px;border:1px solid var(--bd);background:var(--in);color:var(--tx);font-size:13px;}
  .row{display:flex;gap:8px;align-items:center;}
  .row>*{flex:1;}
  .arow{display:grid;grid-template-columns:90px 1fr 110px 64px;gap:6px;align-items:center;margin-bottom:8px;background:var(--in);padding:8px;border-radius:9px;border:1px solid var(--bd);}
  .rlist{max-height:270px;overflow-y:auto;display:flex;flex-direction:column;gap:6px;margin:10px 0;}
  .rrow{display:flex;align-items:center;justify-content:space-between;gap:8px;background:var(--in);border:1px solid var(--bd);border-radius:9px;padding:7px 10px;}
  .rrow.on{border-color:var(--ok);}
  .rinfo{min-width:0;flex:1;}
  .rname{font-size:13px;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
  .rurl{font-size:11px;color:var(--mu);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
  .sbtn{padding:6px 10px;border-radius:7px;border:none;font-size:12px;font-weight:700;cursor:pointer;color:#fff;background:var(--acc);}
  .sbtn.ok{background:var(--ok);color:#06160a;}
  .sbtn.err{background:transparent;border:1px solid var(--err);color:var(--err);}
  .btn{width:100%;margin-top:10px;padding:10px 14px;border:none;border-radius:9px;font-size:13px;font-weight:700;cursor:pointer;color:#fff;background:var(--acc);}
  .btn.ok{background:var(--ok);color:#08160c;}
  .btn.amb{background:transparent;border:1px solid var(--amb);color:var(--amb);}
  .mkt{display:grid;grid-template-columns:1fr 1fr;gap:8px;}
  .mbox{background:var(--in);border:1px solid var(--bd);border-radius:10px;padding:10px 12px;}
  .mbox .k{font-size:11px;color:var(--mu);}
  .mbox .v{font-size:18px;font-weight:700;margin-top:3px;}
  .toast{position:fixed;bottom:16px;right:16px;background:var(--ok);color:#07150b;padding:10px 16px;border-radius:9px;font-weight:700;display:none;z-index:99;}
</style>
</head>
<body>
<div class="wrap">
  <div class="hdr">
    <h1>ESP32 DeskHub v5.0 • Kontrol Paneli</h1>
    <div class="pills">
      <span class="pill" id="pIp">IP: ...</span>
      <span class="pill" id="pRssi">Wi-Fi: ...</span>
      <span class="pill" id="pHeap">RAM: ...</span>
    </div>
  </div>

  <div class="grid">
    <div class="card">
      <h2>Saat Teması &amp; Sayfa Kontrolü <span id="curThemeLbl" style="color:var(--amb);font-size:13px;"></span></h2>
      <div class="themes" id="themeList"></div>
      <label style="margin-top:12px;">Ekrandaki Aktif Sayfa</label>
      <div class="pages" id="pageList"></div>
      <div class="row" style="margin-top:10px;">
        <button class="btn" id="btnLight" onclick="toggleUi('light')">Tema: Karanlık</button>
        <button class="btn" id="btnCycle" onclick="toggleUi('cyc')">45sn Döngü: Kapalı</button>
      </div>
      <div class="row">
        <button class="btn amb" onclick="doAct('refresh')">Verileri Yenile</button>
        <button class="btn amb" onclick="doAct('calibrate')">Dokunmatik Kalibre</button>
      </div>
    </div>

    <div class="card">
      <h2>İnternet Radyosu (PCM5102A) <span id="radCntBadge" style="color:var(--ok);font-size:12px;">0/16</span></h2>
      <div style="background:var(--in);border:1px solid var(--bd);border-radius:10px;padding:10px;">
        <div id="radLive" style="font-size:13px;font-weight:700;color:var(--ok);">Hazır</div>
        <div id="radSub" style="font-size:11px;color:var(--mu);margin-top:2px;">--</div>
        <div class="row" style="margin-top:8px;">
          <button class="sbtn" onclick="radAct('prev')">&lt;|</button>
          <button class="sbtn ok" id="radPlayBtn" onclick="radAct('toggle')">Çal</button>
          <button class="sbtn" onclick="radAct('next')">|&gt;</button>
          <input id="radVol" type="range" min="0" max="100" value="70" oninput="document.getElementById('radVolLbl').textContent='%'+this.value" onchange="setVol(this.value)" style="flex:2;">
          <span id="radVolLbl" style="font-size:12px;max-width:38px;text-align:right;">%70</span>
        </div>
      </div>

      <div class="rlist" id="radList"></div>

      <label>+ Yeni MP3 Kanal Ekle (Ad &amp; Doğrudan HTTP/HTTPS MP3 URL)</label>
      <div class="row">
        <input id="radName" maxlength="27" placeholder="Kanal Adı (ör. Rock FM)" style="max-width:140px;">
        <input id="radUrl" maxlength="127" placeholder="http://yayin.com/stream.mp3">
      </div>
      <div class="row">
        <button class="btn ok" onclick="addRad()">+ Listeye Ekle</button>
        <button class="btn amb" onclick="radAct('reset')">Varsayılan 8 Kanal</button>
      </div>
    </div>

    <div class="card">
      <h2>5× Adreslenebilir LED (WS2812B · GPIO 4)</h2>
      <div class="row">
        <div><label>LED Bildirimleri</label><select id="ledEn"><option value="1">Açık</option><option value="0">Kapalı</option></select></div>
        <div><label>Çalışma Modu</label>
          <select id="ledMode">
            <option value="0">Otomatik (Alarm / Sayaç / Radyo / Hava)</option>
            <option value="1">Sadece Radyo VU</option>
            <option value="2">Sabit Renk</option>
            <option value="3">Gece Amber</option>
            <option value="4">Kapalı</option>
          </select>
        </div>
      </div>
      <div class="row" style="margin-top:6px;">
        <div style="flex:2;"><label>Parlaklık Limiti: <span id="ledBriLbl">%40</span></label>
          <input id="ledBri" type="range" min="10" max="80" value="40" oninput="document.getElementById('ledBriLbl').textContent='%'+this.value">
        </div>
        <div><label>Sabit Renk</label><input id="ledCol" type="color" value="#3893ff" style="height:36px;padding:2px;"></div>
      </div>
      <div class="row">
        <button class="btn amb" onclick="testLed()">Gökkuşağı Testi</button>
        <button class="btn ok" onclick="saveLed()">LED Ayarını Kaydet</button>
      </div>

      <h2 style="margin-top:16px;">Anlık Piyasa (TCMB &amp; Altın)</h2>
      <div class="mkt" id="mktBox"></div>
    </div>

    <div class="card">
      <h2>Alarmlar (3 Bağımsız Alarm)</h2>
      <div id="alarmBox"></div>
      <h2 style="margin-top:14px;">Gece Modu &amp; Ekran</h2>
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
      <h2>Hava Durumu, Haber &amp; Dünya Saatleri</h2>
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
      <button class="btn ok" onclick="saveAll()">Tüm Ayarları Kaydet &amp; Uygula</button>
    </div>

    <div class="card">
      <h2>Wi-Fi ile Güncelleme (OTA) &amp; Sistem</h2>
      <label>Derlenmiş Firmware (.bin) Dosyası</label>
      <input type="file" id="otaFile" accept=".bin">
      <div style="margin-top:10px;background:var(--in);border:1px solid var(--bd);border-radius:8px;height:14px;overflow:hidden;">
        <div id="otaBar" style="height:100%;width:0%;background:var(--ok);transition:width .15s;"></div>
      </div>
      <div id="otaMsg" style="font-size:12px;color:var(--mu);margin-top:6px;">Hazır (deskhub.local üzerinden yükle)</div>
      <button class="btn" id="otaBtn" onclick="uploadOta()">Wi-Fi Üzerinden Güncelle</button>
      <div class="row" style="margin-top:6px;">
        <button class="btn amb" onclick="doAct('portal')">Wi-Fi Kurulum Portalı</button>
        <button class="btn amb" onclick="doAct('restart')">Yeniden Başlat</button>
      </div>
    </div>
  </div>
</div>
<div class="toast" id="toast">Kaydedildi!</div>
<script>
const THEMES=['Yaprak','Nixie Tüp','Dijital LED','Rolex','Patek','Mario','Tetris'];
const PAGES=[[0,'Saat'],[1,'Hava'],[2,'Piyasa'],[3,'Dünya'],[4,'Alarm'],[5,'Haber'],[6,'Odak'],[7,'Radyo'],[8,'Menü'],[9,'Ayar'],[10,'Gece']];
let state=null;

function toast(msg){const t=document.getElementById('toast');t.textContent=msg;t.style.display='block';setTimeout(()=>t.style.display='none',2200);}

function renderRadio(s){
  const r=s.radio;
  document.getElementById('radCntBadge').textContent=r.stations.length+'/16';
  document.getElementById('radLive').textContent=(r.playing?'ÇALIYOR: ':(r.conn?'BAĞLANIYOR: ':''))+(r.name||'Hazır');
  document.getElementById('radSub').textContent=r.title||r.status||'--';
  const pb=document.getElementById('radPlayBtn');
  pb.textContent=(r.playing||r.conn)?'Durdur':'Çal';
  pb.className='sbtn'+(r.playing||r.conn?' err':' ok');
  document.getElementById('radVol').value=r.vol;
  document.getElementById('radVolLbl').textContent='%'+r.vol;
  const rl=document.getElementById('radList'); rl.innerHTML='';
  r.stations.forEach((st,i)=>{
    const div=document.createElement('div');
    div.className='rrow'+(r.idx===i?' on':'');
    const info=document.createElement('div'); info.className='rinfo';
    const nm=document.createElement('div'); nm.className='rname'; nm.textContent=(i+1)+'. '+st.n;
    const ur=document.createElement('div'); ur.className='rurl'; ur.textContent=st.u;
    info.appendChild(nm); info.appendChild(ur);
    const bPlay=document.createElement('button');
    bPlay.className='sbtn'+(r.idx===i&&r.playing?' ok':'');
    bPlay.textContent=(r.idx===i&&r.playing)?'Çalıyor':'Çal';
    bPlay.onclick=()=>radAct('play&idx='+i);
    const bDel=document.createElement('button');
    bDel.className='sbtn err'; bDel.textContent='Sil';
    bDel.onclick=()=>radAct('del&idx='+i);
    div.appendChild(info); div.appendChild(bPlay); div.appendChild(bDel);
    rl.appendChild(div);
  });
}

async function loadState(fillForms){
  const r=await fetch('/api/state');
  state=await r.json();
  document.getElementById('pIp').textContent='IP: '+state.ip;
  document.getElementById('pRssi').textContent='Wi-Fi: '+state.rssi+' dBm';
  document.getElementById('pHeap').textContent='Boş RAM: '+Math.round(state.heap/1024)+' KB';
  document.getElementById('curThemeLbl').textContent=THEMES[state.theme]||'';
  document.documentElement.setAttribute('data-theme',state.light?'light':'dark');
  document.getElementById('btnLight').textContent=state.light?'Tema: Aydınlık Mod':'Tema: Karanlık Mod';
  const bc=document.getElementById('btnCycle');
  bc.textContent=state.cyc?'45sn Döngü: AÇIK':'45sn Döngü: KAPALI';
  bc.className='btn'+(state.cyc?' ok':'');

  const tl=document.getElementById('themeList'); tl.innerHTML='';
  THEMES.forEach((n,i)=>{
    const b=document.createElement('button');
    b.className='tbtn'+(state.theme===i?' on':'');
    b.textContent=n; b.onclick=()=>setTheme(i); tl.appendChild(b);
  });

  const pl=document.getElementById('pageList'); pl.innerHTML='';
  PAGES.forEach(([id,n])=>{
    const b=document.createElement('button');
    b.className='pbtn'+(state.page===id?' on':'');
    b.textContent=n; b.onclick=()=>setPage(id); pl.appendChild(b);
  });

  renderRadio(state);

  const m=state.market;
  document.getElementById('mktBox').innerHTML=`
    <div class="mbox"><div class="k">USD / TRY (TCMB)</div><div class="v">${m.valid?m.usd.toFixed(2)+' ₺':'--'}</div></div>
    <div class="mbox"><div class="k">EUR / TRY (TCMB)</div><div class="v">${m.valid?m.eur.toFixed(2)+' ₺':'--'}</div></div>
    <div class="mbox"><div class="k">Gram Altın</div><div class="v" style="color:var(--gold)">${m.valid?Math.round(m.gram)+' ₺':'--'}</div></div>
    <div class="mbox"><div class="k">Ons Altın (XAU)</div><div class="v" style="color:var(--gold)">${m.valid?Math.round(m.ons)+' $':'--'}</div></div>`;

  if(!fillForms) return;

  document.getElementById('ledEn').value=state.led.en?'1':'0';
  document.getElementById('ledMode').value=String(state.led.mode);
  document.getElementById('ledBri').value=state.led.bri;
  document.getElementById('ledBriLbl').textContent='%'+state.led.bri;
  document.getElementById('ledCol').value=state.led.col||'#3893ff';

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

  const wb=document.getElementById('worldBox'); wb.innerHTML='';
  state.world.forEach((w,i)=>{
    wb.innerHTML+=`<div class="row" style="margin-bottom:6px;"><input id="wn${i}" value="${w.n}"><input id="wo${i}" type="number" value="${w.o}" style="max-width:110px;"></div>`;
  });
}

async function setTheme(i){await fetch('/api/theme?id='+i,{method:'POST'});toast('Tema değiştirildi');loadState(false);}
async function setPage(id){await fetch('/api/page?id='+id,{method:'POST'});toast('Sayfa değiştirildi');loadState(false);}
async function doAct(cmd){await fetch('/api/action?cmd='+cmd,{method:'POST'});toast('Komut gönderildi');}
async function toggleUi(k){
  const v=k==='light'?(state&&state.light?0:1):(state&&state.cyc?0:1);
  await fetch('/api/ui?'+k+'='+v,{method:'POST'});
  toast('Güncellendi');loadState(false);
}
async function radAct(a){
  await fetch('/api/radio?act='+a,{method:'POST'});
  setTimeout(()=>loadState(false),250);
}
async function setVol(v){await fetch('/api/radio?act=vol&v='+v,{method:'POST'});}
async function addRad(){
  const name=document.getElementById('radName').value.trim();
  const url=document.getElementById('radUrl').value.trim();
  if(!name||!url){toast('Kanal adı ve URL girin');return;}
  const r=await fetch('/api/radio?act=add',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name,url})});
  if(r.ok){document.getElementById('radName').value='';document.getElementById('radUrl').value='';toast('Kanal eklendi!');loadState(false);}
  else{toast(await r.text());}
}
async function testLed(){await fetch('/api/led?test=1',{method:'POST'});toast('LED testi başladı');}
async function saveLed(){
  const body={
    en:document.getElementById('ledEn').value==='1',
    mode:parseInt(document.getElementById('ledMode').value),
    bri:parseInt(document.getElementById('ledBri').value),
    col:document.getElementById('ledCol').value
  };
  await fetch('/api/led',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  toast('LED ayarları kaydedildi!');loadState(false);
}
function uploadOta(){
  const inp=document.getElementById('otaFile');
  if(!inp.files||!inp.files.length){toast('Önce .bin dosyası seçin');return;}
  const f=inp.files[0];
  const bar=document.getElementById('otaBar'),msg=document.getElementById('otaMsg'),btn=document.getElementById('otaBtn');
  btn.disabled=true; msg.textContent='Yükleniyor: '+f.name;
  const fd=new FormData(); fd.append('firmware',f,f.name);
  const x=new XMLHttpRequest();
  x.open('POST','/api/ota?size='+f.size,true);
  x.upload.onprogress=(e)=>{if(e.lengthComputable){const p=Math.round(e.loaded*100/e.total);bar.style.width=p+'%';msg.textContent='Yükleniyor %'+p;}};
  x.onload=()=>{btn.disabled=false;if(x.status===200){bar.style.width='100%';msg.textContent='Başarılı! ESP32 yeniden başlıyor...';toast('Güncellendi!');setTimeout(()=>location.reload(),7000);}else{msg.textContent='Hata ('+x.status+'): '+x.responseText;}};
  x.onerror=()=>{btn.disabled=false;msg.textContent='Bağlantı kesildi / OTA hatası';};
  x.send(fd);
}
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
    alarms:[0,1,2].map(i=>{
      const p=document.getElementById('at'+i).value.split(':');
      return {h:parseInt(p[0]||0),m:parseInt(p[1]||0),l:document.getElementById('al'+i).value,r:parseInt(document.getElementById('ar'+i).value),e:document.getElementById('ae'+i).value==='1'};
    }),
    world:[0,1,2,3].map(i=>({n:document.getElementById('wn'+i).value,o:parseInt(document.getElementById('wo'+i).value||0)}))
  };
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  toast('Tüm ayarlar kaydedildi!');loadState(true);
}
loadState(true);
setInterval(()=>loadState(false), 4500);
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
    RadioState rd{};
    radioCopyState(rd);

    JsonDocument doc;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = netGetRssi();
    doc["heap"] = ESP.getFreeHeap();
    doc["theme"] = g_cfgPtr->clockTheme;
    doc["page"] = static_cast<uint8_t>(g_activePage);
    doc["light"] = g_cfgPtr->lightTheme;
    doc["cyc"] = g_cfgPtr->autoCycle;
    doc["otaEn"] = (HAS_WIFI_OTA != 0);
    doc["city"] = g_cfgPtr->city;
    doc["lat"] = g_cfgPtr->lat;
    doc["lon"] = g_cfgPtr->lon;
    doc["topic"] = g_cfgPtr->newsTopic;
    doc["rot"] = g_cfgPtr->rotation;
    doc["nightEn"] = g_cfgPtr->nightEnabled;
    doc["nightS"] = g_cfgPtr->nightStartHour;
    doc["nightE"] = g_cfgPtr->nightEndHour;
    doc["buzz"] = g_cfgPtr->buzzerPin;

    JsonObject led = doc["led"].to<JsonObject>();
    led["en"] = g_cfgPtr->ledEnabled;
    led["bri"] = g_cfgPtr->ledBrightness;
    led["mode"] = g_cfgPtr->ledMode;
    char colHex[10];
    snprintf(colHex, sizeof(colHex), "#%06lx", static_cast<unsigned long>(g_cfgPtr->ledColor & 0xFFFFFFUL));
    led["col"] = colHex;

    JsonObject radio = doc["radio"].to<JsonObject>();
    radio["playing"] = rd.isPlaying;
    radio["conn"] = rd.connecting;
    radio["idx"] = rd.stationIdx;
    radio["vol"] = g_cfgPtr->volume;
    radio["name"] = rd.stationName;
    radio["title"] = rd.streamTitle;
    radio["status"] = rd.statusMsg;
    JsonArray sts = radio["stations"].to<JsonArray>();
    for (uint8_t i = 0; i < g_cfgPtr->radioStationCount && i < MAX_RADIO_STATIONS; ++i) {
        JsonObject so = sts.add<JsonObject>();
        so["n"] = g_cfgPtr->radioStations[i].name;
        so["u"] = g_cfgPtr->radioStations[i].url;
    }

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

void handleRadio() {
    if (!g_cfgPtr) return;
    const String act = g_web.arg("act");

    if (act == "play") {
        if (g_cfgPtr->radioStationCount > 0) {
            const uint8_t idx = static_cast<uint8_t>(g_web.arg("idx").toInt()) % g_cfgPtr->radioStationCount;
            g_cfgPtr->radioActiveIdx = idx;
            saveRadioStations(*g_cfgPtr);
            radioPlay(idx);
        }
    } else if (act == "stop") {
        radioStop();
    } else if (act == "toggle") {
        radioToggle();
    } else if (act == "next") {
        radioNext();
        if (g_cfgPtr->radioStationCount > 0) {
            g_cfgPtr->radioActiveIdx = (g_cfgPtr->radioActiveIdx + 1) % g_cfgPtr->radioStationCount;
            saveRadioStations(*g_cfgPtr);
        }
    } else if (act == "prev") {
        radioPrev();
        if (g_cfgPtr->radioStationCount > 0) {
            g_cfgPtr->radioActiveIdx = (g_cfgPtr->radioActiveIdx + g_cfgPtr->radioStationCount - 1) % g_cfgPtr->radioStationCount;
            saveRadioStations(*g_cfgPtr);
        }
    } else if (act == "vol") {
        const uint8_t vol = static_cast<uint8_t>(constrain(g_web.arg("v").toInt(), 0, 100));
        g_cfgPtr->volume = vol;
        saveConfig(*g_cfgPtr);
        radioSetVolume(vol);
    } else if (act == "add") {
        if (g_cfgPtr->radioStationCount >= MAX_RADIO_STATIONS) {
            g_web.send(400, "text/plain", "Maksimum 16 kanal eklenebilir");
            return;
        }
        JsonDocument doc;
        if (deserializeJson(doc, g_web.arg("plain"))) {
            g_web.send(400, "text/plain", "Geçersiz JSON");
            return;
        }
        String name = doc["name"] | "";
        String url = doc["url"] | "";
        name.trim();
        url.trim();
        if (name.length() == 0 || (!url.startsWith("http://") && !url.startsWith("https://"))) {
            g_web.send(400, "text/plain", "Ad ve http(s):// URL gerekli");
            return;
        }
        const uint8_t idx = g_cfgPtr->radioStationCount++;
        strlcpy(g_cfgPtr->radioStations[idx].name, name.c_str(), sizeof(g_cfgPtr->radioStations[idx].name));
        strlcpy(g_cfgPtr->radioStations[idx].url, url.c_str(), sizeof(g_cfgPtr->radioStations[idx].url));
        saveRadioStations(*g_cfgPtr);
        radioUpdateStations(*g_cfgPtr);
    } else if (act == "del") {
        const int idx = g_web.arg("idx").toInt();
        if (idx >= 0 && idx < g_cfgPtr->radioStationCount) {
            for (int i = idx; i + 1 < g_cfgPtr->radioStationCount; ++i) {
                g_cfgPtr->radioStations[i] = g_cfgPtr->radioStations[i + 1];
            }
            g_cfgPtr->radioStationCount--;
            if (g_cfgPtr->radioStationCount == 0) g_cfgPtr->radioActiveIdx = 0;
            else if (g_cfgPtr->radioActiveIdx >= g_cfgPtr->radioStationCount) g_cfgPtr->radioActiveIdx = 0;
            saveRadioStations(*g_cfgPtr);
            radioUpdateStations(*g_cfgPtr);
        }
    } else if (act == "reset") {
        radioStop();
        loadDefaultRadioStations(*g_cfgPtr);
        saveRadioStations(*g_cfgPtr);
        radioUpdateStations(*g_cfgPtr);
    }

    g_pending = {WebActionType::RADIO_UPDATED, 0};
    g_web.send(200, "text/plain", "OK");
}

void handleLed() {
    if (!g_cfgPtr) return;
    if (g_web.hasArg("test")) {
        ledTriggerTest();
        g_web.send(200, "text/plain", "OK");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, g_web.arg("plain"))) {
        g_web.send(400, "text/plain", "Bad JSON");
        return;
    }
    g_cfgPtr->ledEnabled = doc["en"] | g_cfgPtr->ledEnabled;
    g_cfgPtr->ledMode = static_cast<uint8_t>(doc["mode"] | g_cfgPtr->ledMode) % static_cast<uint8_t>(LedMode::COUNT);
    g_cfgPtr->ledBrightness = static_cast<uint8_t>(constrain(static_cast<int>(doc["bri"] | g_cfgPtr->ledBrightness), 10, 80));
    const char* colStr = doc["col"] | "";
    if (colStr[0] == '#' && strlen(colStr) == 7) {
        g_cfgPtr->ledColor = static_cast<uint32_t>(strtoul(colStr + 1, nullptr, 16)) & 0xFFFFFFUL;
    }
    saveConfig(*g_cfgPtr);
    ledUpdateConfig(*g_cfgPtr);
    g_pending = {WebActionType::LED_UPDATED, 0};
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

void handleUiQuick() {
    if (!g_cfgPtr) return;
    if (g_web.hasArg("light")) {
        g_cfgPtr->lightTheme = (g_web.arg("light").toInt() != 0);
        saveConfig(*g_cfgPtr);
        g_pending = {WebActionType::APPLY_UI_THEME, static_cast<uint8_t>(g_cfgPtr->lightTheme ? 1 : 0)};
    }
    if (g_web.hasArg("cyc")) {
        g_cfgPtr->autoCycle = (g_web.arg("cyc").toInt() != 0);
        saveConfig(*g_cfgPtr);
        g_pending = {WebActionType::CONFIG_UPDATED, 0};
    }
    g_web.send(200, "text/plain", "OK");
}

#if HAS_WIFI_OTA
void handleOtaUpload() {
    HTTPUpload& up = g_web.upload();
    if (up.status == UPLOAD_FILE_START) {
        radioStop();
        netPause(true);
        g_otaTotal = g_web.hasArg("size") ? static_cast<size_t>(g_web.arg("size").toInt()) : 0;
        g_otaWritten = 0;
        g_otaLastPct = 255;
        if (g_otaCb) g_otaCb(0, "Güncelleme başlıyor...");
        const size_t space = (g_otaTotal > 0) ? g_otaTotal : UPDATE_SIZE_UNKNOWN;
        if (!Update.begin(space, U_FLASH)) {
            if (g_otaCb) g_otaCb(0, "OTA yer yok (USB gerekir)");
        }
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.isRunning()) {
            if (Update.write(up.buf, up.currentSize) != up.currentSize) {
                Update.abort();
                if (g_otaCb) g_otaCb(0, "Yazma hatası!");
            } else {
                g_otaWritten += up.currentSize;
                uint8_t pct = (g_otaTotal > 0)
                    ? static_cast<uint8_t>(min<size_t>(99, (g_otaWritten * 100ULL) / g_otaTotal))
                    : static_cast<uint8_t>(min<size_t>(95, g_otaWritten / 18000U));
                if (pct != g_otaLastPct && (g_otaLastPct == 255 || pct - g_otaLastPct >= 4)) {
                    g_otaLastPct = pct;
                    if (g_otaCb) g_otaCb(pct, "Web UI OTA yükleniyor...");
                }
            }
        }
    } else if (up.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            if (g_otaCb) g_otaCb(100, "Tamamlandı! Yeniden başlıyor...");
        } else {
            if (g_otaCb) g_otaCb(0, "Doğrulama hatası!");
        }
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        netPause(false);
        if (g_otaCb) g_otaCb(0, "Güncelleme iptal edildi");
    }
}
#endif
} // namespace

void webUiSetOtaCallback(OtaProgressCb cb) {
    g_otaCb = cb;
}

void webUiStart(DeviceConfig& cfg) {
    g_cfgPtr = &cfg;
    if (g_started) return;

    MDNS.begin(MDNS_HOSTNAME);
    if (!g_routesRegistered) {
        g_web.on("/", HTTP_GET, []() { g_web.send_P(200, "text/html; charset=utf-8", WEB_UI_HTML); });
        g_web.on("/api/state", HTTP_GET, handleState);
        g_web.on("/api/theme", HTTP_POST, handleTheme);
        g_web.on("/api/page", HTTP_POST, handlePage);
        g_web.on("/api/ui", HTTP_POST, handleUiQuick);
        g_web.on("/api/radio", HTTP_POST, handleRadio);
        g_web.on("/api/led", HTTP_POST, handleLed);
        g_web.on("/api/config", HTTP_POST, handleConfigPost);
        g_web.on("/api/action", HTTP_POST, handleAction);
#if HAS_WIFI_OTA
        g_web.on("/api/ota", HTTP_POST,
                 []() {
                     const bool ok = !Update.hasError();
                     g_web.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "FAIL");
                     if (ok) {
                         delay(400);
                         ESP.restart();
                     } else {
                         netPause(false);
                     }
                 },
                 handleOtaUpload);
#endif
        g_routesRegistered = true;
    }
    g_web.begin();
#if HAS_WIFI_OTA
    ArduinoOTA.setHostname(MDNS_HOSTNAME);
    ArduinoOTA.onStart([]() {
        radioStop();
        netPause(true);
        if (g_otaCb) g_otaCb(0, "PlatformIO OTA başlıyor...");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        if (total == 0) return;
        const uint8_t pct = static_cast<uint8_t>((progress * 100ULL) / total);
        if (pct != g_otaLastPct && (g_otaLastPct == 255 || pct == 100 || pct - g_otaLastPct >= 4)) {
            g_otaLastPct = pct;
            if (g_otaCb) g_otaCb(pct, "PlatformIO OTA yükleniyor...");
        }
    });
    ArduinoOTA.onEnd([]() {
        if (g_otaCb) g_otaCb(100, "Tamamlandı! Yeniden başlıyor...");
    });
    ArduinoOTA.onError([](ota_error_t) {
        netPause(false);
        if (g_otaCb) g_otaCb(0, "ArduinoOTA hatası!");
    });
    ArduinoOTA.begin();
#endif
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
#if HAS_WIFI_OTA
        ArduinoOTA.handle();
#endif
    }
}

WebAction webUiPollAction() {
    WebAction out = g_pending;
    g_pending = {WebActionType::NONE, 0};
    return out;
}
