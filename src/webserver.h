#pragma once

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "wifi_credentials.h"

static AsyncWebServer webServer(80);
static AsyncWebSocket ws("/ws");

/* Forward declarations – defined in main.cpp */
extern Preferences prefs;
extern int32_t config_value;
extern int32_t default_values[6];
extern bool autoenter;
void update_config_value(int32_t val);
void web_update_defaults(void);
void web_update_ae(void);

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
  .digits{display:flex;align-items:flex-end;gap:4px;justify-content:center;margin-bottom:10px}
  .digit{font-size:72px;font-weight:bold;cursor:pointer;padding:0 6px;border-bottom:4px solid transparent;line-height:1;color:#fff}
  .digit.selected{border-bottom-color:#7ec8e3}
  .unit{font-size:28px;margin-bottom:8px;color:#aaa}
  .btns{display:flex;gap:8px;justify-content:center;flex-wrap:wrap}
  button{padding:10px 22px;border:none;border-radius:6px;font-size:1em;cursor:pointer;background:#0f3460;color:#fff}
  button:hover{background:#e94560}
  #btnSet{display:none}
  .defaults{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px}
  .defaults button{width:100%}
  .switch-row{display:flex;align-items:center;gap:12px}
  input[type=checkbox]{width:40px;height:22px;cursor:pointer;accent-color:#7ec8e3}
  #status{text-align:center;font-size:.85em;color:#888;margin-top:8px}
</style>
</head>
<body>
<h1>26.5 GHz Attenuator</h1>

<div class="card">
  <h2>Main</h2>
  <div class="digits">
    <span class="digit selected" id="d0" onclick="selectDigit(0)">0</span>
    <span class="digit" id="d1" onclick="selectDigit(1)">0</span>
    <span class="digit" id="d2" onclick="selectDigit(2)">0</span>
    <span class="unit">dB</span>
  </div>
  <div class="btns">
    <button onclick="step(1)">UP</button>
    <button onclick="step(-1)">DOWN</button>
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
    <label>Auto-Enter</label>
    <input type="checkbox" id="swAE" onchange="sendAE()">
  </div>
</div>

<div id="status">Verbinde...</div>

<script>
let ws;
let selDigit = 2;
let curVal = 0;
const defaults = [0,0,0,0,0,0];

function connectWS(){
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen = ()=>{ document.getElementById('status').textContent='Verbunden'; };
  ws.onclose= ()=>{ document.getElementById('status').textContent='Getrennt – reconnect...'; setTimeout(connectWS,2000); };
  ws.onerror= ()=>{ ws.close(); };
  ws.onmessage = e => {
    const msg = JSON.parse(e.data);
    if(msg.type === 'state'){
      curVal = msg.val;
      renderDigits();
      for(let i=0;i<6;i++) defaults[i]=msg.def[i];
      renderDefaults();
      document.getElementById('swAE').checked = msg.ae;
      document.getElementById('btnSet').style.display = msg.ae ? 'none' : 'inline-block';
    }
    if(msg.type === 'val'){
      curVal = msg.val;
      renderDigits();
    }
    if(msg.type === 'def'){
      defaults[msg.idx] = msg.val;
      renderDefaults();
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

function selectDigit(i){
  selDigit = i;
  document.querySelectorAll('.digit').forEach((el,idx)=>{
    el.classList.toggle('selected', idx===i);
  });
}

function step(dir){
  const m = selDigit===0?100:selDigit===1?10:1;
  curVal += dir*m;
  if(curVal<0) curVal=0;
  if(curVal>999) curVal=999;
  renderDigits();
  ws.send(JSON.stringify({cmd:'val', val:curVal}));
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
    b.ondblclick = ()=>{
      const nv = prompt('Neuer Wert (0-999):', v);
      if(nv===null) return;
      const iv=parseInt(nv);
      if(isNaN(iv)||iv<0||iv>999) return;
      ws.send(JSON.stringify({cmd:'setdef', idx:i, val:iv}));
    };
    b.onclick = ()=>{
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

connectWS();
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
        "{\"type\":\"state\",\"val\":%d,\"def\":%s,\"ae\":%s}",
        (int)config_value, defArr, autoenter ? "true" : "false");

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

        if(strcmp(cmd, "val") == 0) {
            int32_t v = config_value;
            if(getInt(msg, "val", v)) {
                if(v < 0) v = 0;
                if(v > 999) v = 999;
                update_config_value(v);          // updates display + NVS
                ws_send_val();                   // broadcast back to other clients
            }
        }
        else if(strcmp(cmd, "set") == 0) {
            int32_t v = config_value;
            getInt(msg, "val", v);
            if(v < 0) v = 0;
            if(v > 999) v = 999;
            update_config_value(v);
            ws_send_val();
        }
        else if(strcmp(cmd, "setdef") == 0) {
            int32_t idx = -1, val = 0;
            if(getInt(msg, "idx", idx) && getInt(msg, "val", val) && idx >= 0 && idx < 6) {
                if(val < 0) val = 0;
                if(val > 999) val = 999;
                default_values[idx] = val;
                char key[8];
                snprintf(key, sizeof(key), "def%d", (int)idx);
                prefs.putInt(key, val);
                // update display label via flag (main loop will call web_update_defaults)
                ws_send_def(idx);
                web_update_defaults();
            }
        }
        else if(strcmp(cmd, "applydef") == 0) {
            int32_t idx = -1;
            if(getInt(msg, "idx", idx) && idx >= 0 && idx < 6) {
                update_config_value(default_values[idx]);
                ws_send_val();
            }
        }
        else if(strcmp(cmd, "ae") == 0) {
            bool v = autoenter;
            if(getBool(msg, "val", v)) {
                autoenter = v;
                prefs.putBool("ae", autoenter);
                ws_send_ae();
                web_update_ae();        // update display switch
            }
        }
    }
}

/* -------------------------------------------------------
 * Public API
 * ------------------------------------------------------- */
static void webserver_setup(void)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("WiFi verbinde");
    for(int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        Serial.print('.');
    }
    if(WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi OK, IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\nWiFi nicht verbunden – Webserver deaktiviert");
        return;
    }

    ws.onEvent(onWsEvent);
    webServer.addHandler(&ws);

    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest * req){
        req->send_P(200, "text/html", WEB_PAGE);
    });

    webServer.begin();
    Serial.println("Webserver gestartet");
}

static void webserver_loop(void)
{
    ws.cleanupClients();
}

/* Called from main.cpp whenever config_value changes from LVGL side */
static void ws_broadcast_val(void)  { ws_send_val(); }
static void ws_broadcast_def(int i) { ws_send_def(i); }
static void ws_broadcast_ae(void)   { ws_send_ae(); }
