#pragma once

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <lvgl.h>
#include "wifi_credentials.h"

static AsyncWebServer webServer(80);
static AsyncWebSocket ws("/ws");

/* Forward declarations – defined in main.cpp */
extern Preferences prefs;
extern int32_t config_value;
extern int32_t default_values[6];
extern bool autoenter;
extern int selected_digit;
extern uint8_t wifi_mode_setting;
extern lv_obj_t * ip_label;
void update_config_value(int32_t val);
void apply_attenuation(void);
void web_update_defaults(void);
void web_update_ae(void);
void web_update_seldigit(void);

/* -------------------------------------------------------
 * FreeRTOS queue for inter-core LVGL command dispatch
 * Core 0 (WebSocket task) enqueues, Core 1 (loop) dequeues
 * ------------------------------------------------------- */
enum WsCmdType : uint8_t { WS_CMD_VAL, WS_CMD_APPLY_DEF, WS_CMD_DEF_SET, WS_CMD_AE, WS_CMD_SELDIGIT, WS_CMD_SET, WS_CMD_WIFI_APPLY };
struct WsCmdMsg { WsCmdType type; int32_t val; int32_t idx; bool bval; };
static QueueHandle_t ws_cmd_queue = nullptr;

/* -------------------------------------------------------
 * HTML page (stored in flash as a raw string)
 * ------------------------------------------------------- */
static const char WEB_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>26.5 GHz Attenuator</title>
<style>
  body{font-family:sans-serif;background:#1a1a2e;color:#eee;margin:0;padding:16px;max-width:480px;margin:auto}
  h1{text-align:center;color:#7ec8e3;font-size:1.3em;margin-bottom:20px}
  .card{background:#16213e;border-radius:10px;padding:16px;margin-bottom:16px}
  h2{margin:0 0 12px;font-size:1em;color:#aaa}
  .digits{display:flex;align-items:flex-end;gap:4px;justify-content:center;margin-bottom:10px;opacity:0;transition:opacity .15s}
  .digits.visible{opacity:1}
  
  .digit{font-size:72px;font-weight:bold;cursor:pointer;padding:0 6px;border-bottom:4px solid transparent;line-height:1;color:#fff}
  .digit.selected{border-bottom-color:#7ec8e3}
  
  .unit{font-size:28px;margin-bottom:8px;color:#aaa}
  .btns{display:flex;gap:8px;justify-content:center;flex-wrap:wrap}
  button{padding:10px 22px;border:none;border-radius:6px;font-size:1em;cursor:pointer;background:#0f3460;color:#fff}
  button:hover{background:#e94560}
  #btnSet{display:none}
  .defaults{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px}
  .defaults button{width:100%}
  .defaults button.active{background:#e94560;outline:2px solid #7ec8e3}
  .switch-row{display:flex;align-items:center;gap:12px}
  input[type=checkbox]{width:40px;height:22px;cursor:pointer;accent-color:#7ec8e3}
  #status{text-align:center;font-size:.85em;color:#888;margin-top:8px;display:flex;align-items:center;justify-content:center;gap:6px}
  .led{width:12px;height:12px;border-radius:50%;display:inline-block;background:#d00}
  .modal-overlay{display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,.7);z-index:100;align-items:center;justify-content:center}
  .modal-overlay.active{display:flex}
  .modal-box{background:#16213e;border-radius:10px;padding:24px;text-align:center;min-width:240px}
  .modal-box h3{margin:0 0 16px;color:#7ec8e3;font-size:1.1em}
  .modal-box input{width:100%;padding:12px;font-size:2em;text-align:center;border:2px solid #0f3460;border-radius:6px;background:#1a1a2e;color:#fff;box-sizing:border-box}
  .modal-btns{display:flex;gap:8px;margin-top:16px;justify-content:center}
  .modal-btns button{flex:1}
</style>
</head>
<body>
<h1>26.5 GHz Attenuator</h1>

<div class="card">
  <h2>Main</h2>
  <div class="digits">
    <span class="digit selected" id="d0" onclick="selectDigit(0)" onpointerdown="digitDown(event,0)" onpointerup="digitUp(0)" onpointerleave="digitUp(0)">0</span>
    <span class="digit" id="d1" onclick="selectDigit(1)" onpointerdown="digitDown(event,1)" onpointerup="digitUp(1)" onpointerleave="digitUp(1)">0</span>
    <span class="digit" id="d2" onclick="selectDigit(2)" onpointerdown="digitDown(event,2)" onpointerup="digitUp(2)" onpointerleave="digitUp(2)">0</span>
    <span class="unit">dB</span>
  </div>
  <div class="btns">
    <button onclick="step(-1)">DOWN</button>
    <button onclick="step(1)">UP</button>
    <button id="btnSet" onclick="sendSet()">Set</button>
  </div>
</div>

<div class="card">
  <h2>Defaults</h2>
  <div class="defaults" id="defaults"></div>
</div>

<div class="card">
  <h2>Config</h2>
  <div class="switch-row">
    <label>Auto-Set</label>
    <input type="checkbox" id="swAE" onchange="sendAE()">
  </div>
</div>

<div class="card" id="wifiSetup" style="display:none">
  <h2>WLAN konfigurieren</h2>
  <button id="btnScan" onclick="scanWifi()" style="width:100%;margin-bottom:10px">Netzwerke suchen</button>
  <div id="scanResults"></div>
  <input type="text" id="wifiSSID" placeholder="SSID" autocomplete="off"
    style="width:100%;padding:10px;margin-bottom:8px;font-size:1em;background:#1a1a2e;color:#fff;border:2px solid #0f3460;border-radius:6px;box-sizing:border-box">
  <div style="position:relative;margin-bottom:8px">
    <input type="password" id="wifiPass" placeholder="Passwort" autocomplete="new-password"
      style="width:100%;padding:10px;padding-right:52px;font-size:1em;background:#1a1a2e;color:#fff;border:2px solid #0f3460;border-radius:6px;box-sizing:border-box">
    <button onclick="togglePass()" title="Passwort anzeigen"
      style="position:absolute;right:6px;top:50%;transform:translateY(-50%);padding:4px 10px;font-size:.9em;background:#0f3460">&#128065;</button>
  </div>
  <button onclick="connectWifi()" style="width:100%;background:#0a6640">Verbinden</button>
  <div id="wifiStatus" style="margin-top:8px;text-align:center;font-size:.85em;min-height:1.2em"></div>
</div>

<div id="status"><span class="led" id="led"></span><span id="stxt">Verbinde...</span></div>

<div class="modal-overlay" id="digitModal">
  <div class="modal-box">
    <h3>Wert eingeben (0–999)</h3>
    <input type="number" id="digitInput" min="0" max="999" inputmode="numeric" pattern="[0-9]*" onkeydown="if(event.key==='Enter')digitModalOk()">
    <div class="modal-btns">
      <button onclick="digitModalCancel()">Abbrechen</button>
      <button onclick="digitModalOk()">OK</button>
    </div>
  </div>
</div>

<script>
let ws;
let selDigit = 2;
let curVal = 0;
const defaults = [0,0,0,0,0,0];
let activeDefIdx = -1;
let lastContact = 0;
let digitLpTimer = [0,0,0];
let digitLpFired = false;
let maxVal = 999;
let digitMax = [9,9,9];

function applyDigitDisable(){
  for(let i=0;i<3;i++){
    const el=document.getElementById('d'+i);
    if(!el) continue;
    if(digitMax[i]===0){
      el.style.cursor='default';
      el.style.color=(i===2)?'#a2a2a2':'#555';
      el.style.borderBottomColor='transparent';
    }
    else{
      el.style.cursor='pointer';
      el.style.color=(i===2)?'#a2a2a2':'#fff';
    }
  }
}

function connectWS(){
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen = ()=>{ lastContact=Date.now(); document.getElementById('stxt').textContent='Verbunden'; };
  ws.onclose= ()=>{ document.getElementById('stxt').textContent='Getrennt \u2013 reconnect...'; setTimeout(connectWS,2000); };
  ws.onerror= ()=>{ ws.close(); };
  ws.onmessage = e => {
    lastContact=Date.now();
    const msg = JSON.parse(e.data);
    if(msg.type === 'state'){
      curVal = msg.val;
      renderDigits();
      document.querySelector('.digits').classList.add('visible');
      for(let i=0;i<6;i++) defaults[i]=msg.def[i];
      renderDefaults();
      document.getElementById('swAE').checked = msg.ae;
      document.getElementById('btnSet').style.display = msg.ae ? 'none' : 'inline-block';
      if(msg.sel !== undefined) applyDigitSelection(msg.sel);
      if(msg.maxVal !== undefined) maxVal=msg.maxVal;
      if(msg.digitMax !== undefined){digitMax=msg.digitMax;applyDigitDisable();}
    }
    if(msg.type === 'val'){
      curVal = msg.val;
      renderDigits();
      renderDefaults();
    }
    if(msg.type === 'def'){
      defaults[msg.idx] = msg.val;
      renderDefaults();
    }
    if(msg.type === 'activedef'){
      renderDefaults();
    }
    if(msg.type === 'seldigit'){
      applyDigitSelection(msg.idx);
    }
    if(msg.type === 'ae'){
      document.getElementById('swAE').checked = msg.val;
      document.getElementById('btnSet').style.display = msg.val ? 'none' : 'inline-block';
    }
  };
}

function renderDigits(){
  document.getElementById('d0').textContent = Math.floor(curVal/100)%10;
  document.getElementById('d1').textContent = Math.floor(curVal/10)%10;
  document.getElementById('d2').textContent = curVal%10;
}

function applyDigitSelection(i){
  selDigit = i;
  document.querySelectorAll('.digit').forEach((el,idx)=>{
    el.classList.toggle('selected', idx===i);
  });
}

function selectDigit(i){
  if(digitLpFired){digitLpFired=false;return;}
  if(digitMax[i]===0) return;
  applyDigitSelection(i);
  ws.send(JSON.stringify({cmd:'seldigit', idx:i}));
}

function step(dir){
  if(digitMax[selDigit]===0) return;
  const m = selDigit===0?100:selDigit===1?10:1;
  curVal += dir*m;
  if(curVal<0) curVal=0;
  if(curVal>maxVal) curVal=maxVal;
  renderDigits();
  ws.send(JSON.stringify({cmd:'upd', val:curVal}));
}

function sendSet(){
  ws.send(JSON.stringify({cmd:'set', val:curVal}));
}

function renderDefaults(){
  const cont = document.getElementById('defaults');
  cont.innerHTML='';
  defaults.forEach((v,i)=>{
    const b = document.createElement('button');
    b.textContent = v+' dB';
    b.classList.toggle('active', v===curVal);
    let lpt,lf=false;
    b.onpointerdown=()=>{lf=false;lpt=setTimeout(()=>{lf=true;lpt=0;
      const nv=prompt('Neuer Wert (0-'+maxVal+'):',v);
      if(nv===null)return;
      let iv=parseInt(nv);
      if(isNaN(iv)||iv<0) return;
      if(iv>maxVal) iv=maxVal;
      if(digitMax[2]===0) iv=Math.floor(iv/10)*10;
      if(digitMax[1]===0) iv=Math.floor(iv/100)*100;
      defaults[i]=iv;curVal=iv;
      renderDigits();renderDefaults();
      ws.send(JSON.stringify({cmd:'setdef',idx:i,val:iv}));
    },600);};
    b.onpointerup=()=>{if(lpt){clearTimeout(lpt);lpt=0;}};
    b.onpointerleave=()=>{if(lpt){clearTimeout(lpt);lpt=0;}};
    b.onclick=(e)=>{if(lf){e.preventDefault();return;}
      curVal = defaults[i];
      renderDigits();
      renderDefaults();
      ws.send(JSON.stringify({cmd:'applydef', idx:i}));
    };
    cont.appendChild(b);
  });
}

function sendAE(){
  const v = document.getElementById('swAE').checked;
  document.getElementById('btnSet').style.display = v ? 'none' : 'inline-block';
  ws.send(JSON.stringify({cmd:'ae', val:v}));
}

function digitDown(evt,i){
  if(digitMax[i]===0) return;
  digitLpFired=false;
  digitLpTimer[i]=setTimeout(()=>{
    digitLpFired=true;
    digitLpTimer[i]=0;
    const modal=document.getElementById('digitModal');
    const inp=document.getElementById('digitInput');
    inp.value=curVal;
    modal.classList.add('active');
    setTimeout(()=>{inp.value='';inp.focus();},50);
  },600);
}
function digitUp(i){
  if(digitLpTimer[i]){clearTimeout(digitLpTimer[i]);digitLpTimer[i]=0;}
}
function digitModalOk(){
  const inp=document.getElementById('digitInput');
  let v=parseInt(inp.value);
  if(isNaN(v)||v<0) v=0;
  if(v>maxVal) v=maxVal;
  if(digitMax[2]===0) v=Math.floor(v/10)*10;
  if(digitMax[1]===0) v=Math.floor(v/100)*100;
  curVal=v;
  renderDigits();
  ws.send(JSON.stringify({cmd:'upd', val:curVal}));
  document.getElementById('digitModal').classList.remove('active');
}
function digitModalCancel(){
  document.getElementById('digitModal').classList.remove('active');
}

// --- WiFi-Setup (nur im AP-Modus sichtbar) ---
let scannedNets=[];
fetch('/api/mode').then(r=>r.json()).then(d=>{
  if(d.mode===1) document.getElementById('wifiSetup').style.display='block';
}).catch(()=>{});

function escH(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}

let scanPollTimer=null;
function stopScanPoll(){if(scanPollTimer){clearInterval(scanPollTimer);scanPollTimer=null;}}

function renderScanNets(nets){
  const cont=document.getElementById('scanResults');
  scannedNets=nets||[];
  if(!scannedNets.length){cont.textContent='Keine Netzwerke gefunden';return;}
  cont.innerHTML='';
  scannedNets.sort((a,b)=>b.rssi-a.rssi).forEach((n,i)=>{
    const row=document.createElement('div');
    row.style.cssText='padding:8px 12px;margin:3px 0;background:#0f3460;border-radius:6px;cursor:pointer;display:flex;justify-content:space-between;align-items:center';
    const nameEl=document.createElement('span'); nameEl.textContent=n.ssid;
    const infoEl=document.createElement('span');
    infoEl.style.cssText='color:#aaa;font-size:.85em;white-space:nowrap';
    infoEl.textContent=(n.enc?'\uD83D\uDD12 ':'')+n.rssi+' dBm';
    row.appendChild(nameEl); row.appendChild(infoEl);
    row.onclick=()=>{
      document.getElementById('wifiSSID').value=scannedNets[i].ssid;
      cont.innerHTML='';
    };
    cont.appendChild(row);
  });
}

function scanWifi(){
  const btn=document.getElementById('btnScan');
  const cont=document.getElementById('scanResults');
  stopScanPoll();
  btn.disabled=true; btn.textContent='Suche l\u00e4uft\u2026';
  cont.innerHTML='';
  fetch('/api/scan').catch(()=>{
    btn.disabled=false; btn.textContent='Netzwerke suchen';
    cont.textContent='Scan fehlgeschlagen';
  });
  /* Poll for results every 1.5 s, give up after 20 s */
  let attempts=0;
  scanPollTimer=setInterval(()=>{
    attempts++;
    if(attempts>13){stopScanPoll();btn.disabled=false;btn.textContent='Netzwerke suchen';cont.textContent='Scan fehlgeschlagen';return;}
    fetch('/api/scanresult').then(r=>r.json()).then(d=>{
      if(d.status==='scanning') return;
      stopScanPoll();
      btn.disabled=false; btn.textContent='Netzwerke suchen';
      if(d.status==='failed'){cont.textContent='Scan fehlgeschlagen \u2013 bitte erneut versuchen';return;}
      renderScanNets(d.nets);
    }).catch(()=>{});
  },1500);
}

function togglePass(){
  const inp=document.getElementById('wifiPass');
  inp.type=inp.type==='password'?'text':'password';
}

let wifiStatusPollTimer=null;
function stopWifiStatusPoll(){if(wifiStatusPollTimer){clearInterval(wifiStatusPollTimer);wifiStatusPollTimer=null;}}

function connectWifi(){
  const ssid=document.getElementById('wifiSSID').value.trim();
  const pass=document.getElementById('wifiPass').value;
  const stat=document.getElementById('wifiStatus');
  if(!ssid){stat.style.color='#e94560';stat.textContent='Bitte SSID eingeben';return;}
  stat.style.color='#aaa'; stat.textContent='Verbinde\u2026';
  const body=new URLSearchParams();
  body.append('ssid',ssid); body.append('pass',pass);
  stopWifiStatusPoll();
  
  fetch('/api/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.toString()})
    .then(r=>r.json()).then(d=>{
      if(d.ok){
        stat.style.color='#aaa';
        stat.innerHTML='Verbindung wird hergestellt...';
        /* Poll WiFi-Status alle 2 Sekunden, max 30 Sekunden */
        let attempts=0;
        wifiStatusPollTimer=setInterval(()=>{
          attempts++;
          if(attempts>15){stopWifiStatusPoll();stat.style.color='#e94560';stat.textContent='Verbindung fehlgeschlagen (Timeout)';return;}
          fetch('/api/wifistatus').then(r=>r.json()).then(s=>{
            if(s.connected){
              stopWifiStatusPoll();
              stat.style.color='#0a0';
              stat.innerHTML='Verbunden mit <b>'+escH(s.ssid)+'</b><br>Client-IP: '+s.ip+' | AP: '+s.ap_ip+'<br>Erreichbar \u00fcber beide IPs und <b>esp32-att.local</b>';
            }
          }).catch(()=>{});
        },2000);
      }
      else{stat.style.color='#e94560';stat.textContent='Fehler beim Verbinden';}
    }).catch(()=>{
      stat.style.color='#0a0'; stat.textContent='Verbindungsaufbau gestartet\u2026';
    });
}

connectWS();
setInterval(()=>{
  const led=document.getElementById('led');
  const alive=ws&&ws.readyState===WebSocket.OPEN&&(Date.now()-lastContact)<2000;
  led.style.background=alive?'#0a0':'#d00';
  if(!alive&&ws&&ws.readyState===WebSocket.OPEN){
    ws.close();
  }
  if(!ws||ws.readyState===WebSocket.CLOSED){
    connectWS();
  }
},500);
</script>
</body>
</html>
)rawhtml";

/* -------------------------------------------------------
 * Send full state to one or all clients
 * ------------------------------------------------------- */
static void ws_send_state(AsyncWebSocketClient * client = nullptr)
{
    char buf[256];
    // Build the def array part
    char defArr[64];
    snprintf(defArr, sizeof(defArr),
        "[%d,%d,%d,%d,%d,%d]",
        (int)default_values[0], (int)default_values[1], (int)default_values[2],
        (int)default_values[3], (int)default_values[4], (int)default_values[5]);

    snprintf(buf, sizeof(buf),
        "{\"type\":\"state\",\"val\":%d,\"def\":%s,\"ae\":%s,\"sel\":%d,"
        "\"maxVal\":%d,\"digitMax\":[%d,%d,%d]}",
        (int)config_value, defArr, autoenter ? "true" : "false", selected_digit,
        DIGIT_MAX_VAL, DIGIT_MAX_0, DIGIT_MAX_1, DIGIT_MAX_2);

    if(client) client->text(buf);
    else        ws.textAll(buf);
}

static void ws_send_val(void)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "{\"type\":\"val\",\"val\":%d}", (int)config_value);
    ws.textAll(buf);
}

static void ws_send_def(int idx)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "{\"type\":\"def\",\"idx\":%d,\"val\":%d}", idx, (int)default_values[idx]);
    ws.textAll(buf);
}

static void ws_send_active_def(int idx)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"type\":\"activedef\",\"idx\":%d}", idx);
    ws.textAll(buf);
}

static void ws_send_seldigit(void)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"type\":\"seldigit\",\"idx\":%d}", selected_digit);
    ws.textAll(buf);
}

static void ws_send_ae(void)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "{\"type\":\"ae\",\"val\":%s}", autoenter ? "true" : "false");
    ws.textAll(buf);
}

/* -------------------------------------------------------
 * WebSocket event handler
 * ------------------------------------------------------- */
static void onWsEvent(AsyncWebSocket * /*server*/, AsyncWebSocketClient * client,
                      AwsEventType type, void * arg, uint8_t * data, size_t len)
{
    if(type == WS_EVT_CONNECT) {
        ws_send_state(client);
    }
    else if(type == WS_EVT_DATA) {
        AwsFrameInfo * info = (AwsFrameInfo *)arg;
        if(info->opcode != WS_TEXT) return;

        // Null-terminate
        char msg[256];
        size_t copyLen = len < sizeof(msg) - 1 ? len : sizeof(msg) - 1;
        memcpy(msg, data, copyLen);
        msg[copyLen] = '\0';

        // Minimal JSON parser – extract "cmd" and numeric fields
        auto getStr = [](const char * json, const char * key, char * out, int outLen) -> bool {
            char pattern[32];
            snprintf(pattern, sizeof(pattern), "\"%s\"", key);
            const char * p = strstr(json, pattern);
            if(!p) return false;
            p += strlen(pattern);
            while(*p==' '||*p==':') p++;
            if(*p == '"') {
                p++;
                int i=0;
                while(*p && *p!='"' && i<outLen-1) out[i++]=*p++;
                out[i]='\0';
                return true;
            }
            return false;
        };
        auto getInt = [](const char * json, const char * key, int32_t & out) -> bool {
            char pattern[32];
            snprintf(pattern, sizeof(pattern), "\"%s\"", key);
            const char * p = strstr(json, pattern);
            if(!p) return false;
            p += strlen(pattern);
            while(*p==' '||*p==':') p++;
            if(*p=='-'||(*p>='0'&&*p<='9')) { out = atoi(p); return true; }
            return false;
        };
        auto getBool = [](const char * json, const char * key, bool & out) -> bool {
            char pattern[32];
            snprintf(pattern, sizeof(pattern), "\"%s\"", key);
            const char * p = strstr(json, pattern);
            if(!p) return false;
            p += strlen(pattern);
            while(*p==' '||*p==':') p++;
            if(strncmp(p,"true",4)==0)  { out=true;  return true; }
            if(strncmp(p,"false",5)==0) { out=false; return true; }
            return false;
        };

        char cmd[16] = "";
        getStr(msg, "cmd", cmd, sizeof(cmd));

        if(strcmp(cmd, "upd") == 0) {
            int32_t v = config_value;
            if(getInt(msg, "val", v)) {
                if(v < 0) v = 0;
                if(v > DIGIT_MAX_VAL) v = DIGIT_MAX_VAL;
                if(DIGIT_MAX_2 == 0) v = (v / 10) * 10;
                if(DIGIT_MAX_1 == 0) v = (v / 100) * 100;
                config_value = v;
                ws_send_val();
                WsCmdMsg m = {WS_CMD_VAL, v, 0, false};
                xQueueSend(ws_cmd_queue, &m, 0);
            }
        }
        else if(strcmp(cmd, "set") == 0) {
            int32_t v = config_value;
            getInt(msg, "val", v);
            if(v < 0) v = 0;
            if(v > DIGIT_MAX_VAL) v = DIGIT_MAX_VAL;
            if(DIGIT_MAX_2 == 0) v = (v / 10) * 10;
            if(DIGIT_MAX_1 == 0) v = (v / 100) * 100;
            config_value = v;
            ws_send_val();
            WsCmdMsg m = {WS_CMD_SET, v, 0, false};
            xQueueSend(ws_cmd_queue, &m, 0);
        }
        else if(strcmp(cmd, "setdef") == 0) {
            int32_t idx = -1, val = 0;
            if(getInt(msg, "idx", idx) && getInt(msg, "val", val) && idx >= 0 && idx < 6) {
                if(val < 0) val = 0;
                if(val > DIGIT_MAX_VAL) val = DIGIT_MAX_VAL;
                if(DIGIT_MAX_2 == 0) val = (val / 10) * 10;
                if(DIGIT_MAX_1 == 0) val = (val / 100) * 100;
                default_values[idx] = val;
                char key[8];
                snprintf(key, sizeof(key), "def%d", (int)idx);
                prefs.putInt(key, val);
                ws_send_def(idx);
                WsCmdMsg m = {WS_CMD_DEF_SET, val, idx, false};
                xQueueSend(ws_cmd_queue, &m, 0);
            }
        }
        else if(strcmp(cmd, "applydef") == 0) {
            int32_t idx = -1;
            if(getInt(msg, "idx", idx) && idx >= 0 && idx < 6) {
                config_value = default_values[idx];
                ws_send_val();
                ws_send_active_def(idx);
                WsCmdMsg m = {WS_CMD_APPLY_DEF, 0, idx, false};
                xQueueSend(ws_cmd_queue, &m, 0);
            }
        }
        else if(strcmp(cmd, "ae") == 0) {
            bool v = autoenter;
            if(getBool(msg, "val", v)) {
                autoenter = v;
                prefs.putBool("ae", autoenter);
                ws_send_ae();
                WsCmdMsg m = {WS_CMD_AE, 0, 0, v};
                xQueueSend(ws_cmd_queue, &m, 0);
            }
        }
        else if(strcmp(cmd, "seldigit") == 0) {
            int32_t idx = 2;
            if(getInt(msg, "idx", idx) && idx >= 0 && idx <= 2) {
                selected_digit = idx;
                ws_send_seldigit();
                WsCmdMsg m = {WS_CMD_SELDIGIT, 0, idx, false};
                xQueueSend(ws_cmd_queue, &m, 0);
            }
        }
    }
}

/* -------------------------------------------------------
 * Public API
 * ------------------------------------------------------- */
static bool webserver_running = false;

/* WiFi connection state machine for non-blocking connect */
static enum { WIFI_IDLE, WIFI_CONNECTING, WIFI_CONNECTED, WIFI_FAILED } wifi_connect_state = WIFI_IDLE;
static unsigned long wifi_connect_start = 0;
static const unsigned long WIFI_CONNECT_TIMEOUT = 20000; // 20 seconds

/* Escape special chars for JSON string values */
static String jsonEscStr(const String & s)
{
    String out;
    out.reserve(s.length() + 4);
    for(unsigned i = 0; i < s.length(); i++) {
        char c = s[i];
        if     (c == '"') out += "\\\"";
        else if(c == '\\') out += "\\\\";
        else if(c == '\n') out += "\\n";
        else if(c == '\r') out += "\\r";
        else               out += c;
    }
    return out;
}

static void start_webserver(void)
{
    if(webserver_running) return;
    if(!ws_cmd_queue) ws_cmd_queue = xQueueCreate(8, sizeof(WsCmdMsg));
    ws.onEvent(onWsEvent);
    webServer.addHandler(&ws);
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest * req){
        req->send_P(200, "text/html", WEB_PAGE);
    });

    /* Returns current WiFi mode: 0=off, 1=AP, 2=Client */
    webServer.on("/api/mode", HTTP_GET, [](AsyncWebServerRequest * req){
        char buf[24];
        snprintf(buf, sizeof(buf), "{\"mode\":%d}", wifi_mode_setting);
        req->send(200, "application/json", buf);
    });

    /* Scan for available networks – starts async scan, returns immediately */
    webServer.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest * req){
        int cur = WiFi.scanComplete();
        if(cur == WIFI_SCAN_RUNNING) {
            req->send(200, "application/json", "{\"status\":\"scanning\"}");
            return;
        }
        /* Always delete old results before starting fresh scan */
        WiFi.scanDelete();
        WiFi.scanNetworks(/*async=*/true);
        req->send(200, "application/json", "{\"status\":\"scanning\"}");
    });

    /* Poll for scan results */
    webServer.on("/api/scanresult", HTTP_GET, [](AsyncWebServerRequest * req){
        static uint8_t scanRetries = 0;
        int n = WiFi.scanComplete();
        if(n == WIFI_SCAN_RUNNING) {
            req->send(200, "application/json", "{\"status\":\"scanning\"}");
            return;
        }
        if(n == WIFI_SCAN_FAILED) {
            /* Scan failed – retry up to 3 times before giving up */
            if(scanRetries < 3) {
                scanRetries++;
                WiFi.scanNetworks(/*async=*/true);
                req->send(200, "application/json", "{\"status\":\"scanning\"}");
            } else {
                scanRetries = 0;
                req->send(200, "application/json", "{\"status\":\"failed\"}");
            }
            return;
        }
        scanRetries = 0;
        if(n > 20) n = 20;
        String json = "{\"status\":\"done\",\"nets\":[";
        for(int i = 0; i < n; i++) {
            if(i > 0) json += ",";
            json += "{\"ssid\":\"" + jsonEscStr(WiFi.SSID(i)) + "\",\"rssi\":" + WiFi.RSSI(i);
            json += ",\"enc\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? 1 : 0) + "}";
        }
        json += "]}";
        WiFi.scanDelete();
        req->send(200, "application/json", json);
    });

    /* Get current WiFi connection status */
    webServer.on("/api/wifistatus", HTTP_GET, [](AsyncWebServerRequest * req){
        String json = "{";
        json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
        if(WiFi.status() == WL_CONNECTED) {
            json += ",\"ssid\":\"" + jsonEscStr(WiFi.SSID()) + "\"";
            json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
            json += ",\"rssi\":" + String(WiFi.RSSI());
        }
        json += ",\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\"";
        json += "}";
        req->send(200, "application/json", json);
    });

    /* Connect to a network: POST ssid= & pass= (URL-encoded) */
    webServer.on("/api/connect", HTTP_POST, [](AsyncWebServerRequest * req){
        if(req->hasParam("ssid", true)) {
            String ssid = req->getParam("ssid", true)->value();
            String pass = req->hasParam("pass", true) ? req->getParam("pass", true)->value() : "";
            /* Validate: SSID 1-32 bytes, password 0-63 bytes (WPA2) */
            if(ssid.length() == 0 || ssid.length() > 32 || pass.length() > 63) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid\"}");
                return;
            }
            prefs.putString("wifi_ssid", ssid);
            prefs.putString("wifi_pass", pass);
            wifi_mode_setting = 2;
            prefs.putUChar("wmode", wifi_mode_setting);
            req->send(200, "application/json", "{\"ok\":true}");
            WsCmdMsg m = {WS_CMD_WIFI_APPLY, 0, 0, false};
            xQueueSend(ws_cmd_queue, &m, 0);
        } else {
            req->send(400, "application/json", "{\"ok\":false}");
        }
    });

    webServer.begin();
    webserver_running = true;
    Serial.println("Webserver gestartet");
}

static void stop_webserver(void)
{
    if(!webserver_running) return;
    ws.closeAll();
    webServer.end();
    webserver_running = false;
    Serial.println("Webserver gestoppt");
}

static void apply_wifi_mode(void)
{
    /* Reset connection state */
    wifi_connect_state = WIFI_IDLE;
    
    /* Mode 0: WiFi OFF */
    if(wifi_mode_setting == 0) {
        stop_webserver();
        WiFi.softAPdisconnect(true);
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        Serial.println("WiFi AUS");
        if(ip_label) lv_label_set_text(ip_label, "");
        return;
    }
    
    /* Mode 1 & 2: Immer AP_STA Modus für durchgehende Erreichbarkeit */
    wifi_mode_t currentMode = WiFi.getMode();
    
    /* Nur komplett neu starten wenn WiFi OFF war oder Webserver nicht läuft */
    if(currentMode == WIFI_OFF || !webserver_running) {
        stop_webserver();
        WiFi.mode(WIFI_AP_STA);
        delay(200);
        WiFi.softAP("ESP32-ATT", "12345678");
        Serial.printf("AP gestartet, IP: %s\n", WiFi.softAPIP().toString().c_str());
        if(ip_label) lv_label_set_text_fmt(ip_label, "IP: %s", WiFi.softAPIP().toString().c_str());
        start_webserver();
    }
    
    /* Mode 1: Nur AP (keine Client-Verbindung) */
    if(wifi_mode_setting == 1) {
        /* STA-Verbindung trennen falls vorhanden, AP läuft weiter */
        if(WiFi.status() == WL_CONNECTED) {
            WiFi.disconnect();
            Serial.println("Client-Verbindung getrennt, AP läuft weiter");
            if(ip_label) lv_label_set_text_fmt(ip_label, "AP: %s", WiFi.softAPIP().toString().c_str());
        }
        return;
    }
    
    /* Mode 2: AP + Client (zusätzliche Client-Verbindung aufbauen) */
    if(wifi_mode_setting == 2) {
        /* Gespeicherte Zugangsdaten bevorzugen, Fallback auf wifi_credentials.h */
        String _ssid = prefs.getString("wifi_ssid", WIFI_SSID);
        String _pass = prefs.getString("wifi_pass", WIFI_PASSWORD);
        
        /* Nur neu verbinden wenn noch nicht verbunden oder andere SSID */
        if(WiFi.status() != WL_CONNECTED || WiFi.SSID() != _ssid) {
            Serial.printf("WiFi verbinde mit SSID: %s (non-blocking)\n", _ssid.c_str());
            if(ip_label) lv_label_set_text_fmt(ip_label, "Verbinde: %s | AP: %s", 
                _ssid.c_str(), WiFi.softAPIP().toString().c_str());
            
            /* Start non-blocking connection */
            WiFi.begin(_ssid.c_str(), _pass.c_str());
            wifi_connect_state = WIFI_CONNECTING;
            wifi_connect_start = millis();
        } else {
            Serial.printf("WiFi bereits verbunden mit %s\n", WiFi.SSID().c_str());
            wifi_connect_state = WIFI_CONNECTED;
        }
    }
}

static void webserver_setup(void)
{
    apply_wifi_mode();
}

static void webserver_loop(void)
{
    ws.cleanupClients();

    /* Non-blocking WiFi connection state machine */
    if(wifi_connect_state == WIFI_CONNECTING) {
        wl_status_t status = WiFi.status();
        if(status == WL_CONNECTED) {
            wifi_connect_state = WIFI_CONNECTED;
            Serial.printf("\nWiFi OK, Client-IP: %s, AP-IP: %s\n", 
                WiFi.localIP().toString().c_str(), 
                WiFi.softAPIP().toString().c_str());
            if(ip_label) lv_label_set_text_fmt(ip_label, "IP: %s | AP: %s", 
                WiFi.localIP().toString().c_str(), WiFi.softAPIP().toString().c_str());
            
            /* mDNS starten für Erreichbarkeit über esp32-att.local */
            if(!MDNS.begin("esp32-att")) {
                Serial.println("mDNS Start fehlgeschlagen");
            } else {
                MDNS.addService("http", "tcp", 80);
                Serial.println("mDNS gestartet: esp32-att.local");
            }
        }
        else if(millis() - wifi_connect_start > WIFI_CONNECT_TIMEOUT) {
            wifi_connect_state = WIFI_FAILED;
            Serial.println("\nWiFi Client-Verbindung fehlgeschlagen (Timeout)");
            if(ip_label) lv_label_set_text_fmt(ip_label, "AP: %s (Client Timeout)",
                WiFi.softAPIP().toString().c_str());
        }
        else if(status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
            wifi_connect_state = WIFI_FAILED;
            Serial.printf("\nWiFi Client-Verbindung fehlgeschlagen (Status: %d)\n", status);
            if(ip_label) lv_label_set_text_fmt(ip_label, "AP: %s (Client fehlgeschlagen)",
                WiFi.softAPIP().toString().c_str());
        }
    }

    /* Send periodic heartbeat so client LED stays green */
    static unsigned long lastPing = 0;
    if(millis() - lastPing > 1000) {
        lastPing = millis();
        ws.textAll("{\"type\":\"hb\"}");
    }

    if(!ws_cmd_queue) return;
    WsCmdMsg m;
    while(xQueueReceive(ws_cmd_queue, &m, 0) == pdTRUE) {
        switch(m.type) {
            case WS_CMD_VAL:
                update_config_value(m.val);
                break;
            case WS_CMD_APPLY_DEF:
                update_config_value(default_values[m.idx]);
                break;
            case WS_CMD_DEF_SET:
                update_config_value(default_values[m.idx]);
                web_update_defaults();
                break;
            case WS_CMD_AE:
                autoenter = m.bval;
                web_update_ae();
                break;
            case WS_CMD_SELDIGIT:
                selected_digit = m.idx;
                web_update_seldigit();
                break;
            case WS_CMD_SET:
                update_config_value(m.val);
                apply_attenuation();
                break;
            case WS_CMD_WIFI_APPLY:
                apply_wifi_mode();
                break;
        }
    }
}

/* Called from main.cpp whenever config_value changes from LVGL side */
static void ws_broadcast_val(void)        { ws_send_val(); }
static void ws_broadcast_def(int i)       { ws_send_def(i); }
static void ws_broadcast_ae(void)         { ws_send_ae(); }
static void ws_broadcast_active_def(int i){ ws_send_active_def(i); }
static void ws_broadcast_seldigit(int i)  { selected_digit = i; ws_send_seldigit(); }
