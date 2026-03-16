#pragma once

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
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
void web_update_defaults(void);
void web_update_ae(void);
void web_update_seldigit(void);

/* -------------------------------------------------------
 * FreeRTOS queue for inter-core LVGL command dispatch
 * Core 0 (WebSocket task) enqueues, Core 1 (loop) dequeues
 * ------------------------------------------------------- */
enum WsCmdType : uint8_t { WS_CMD_VAL, WS_CMD_APPLY_DEF, WS_CMD_DEF_SET, WS_CMD_AE, WS_CMD_SELDIGIT };
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
  .defaults button.active{background:#e94560;outline:2px solid #7ec8e3}
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
let activeDefIdx = -1;

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
      if(msg.sel !== undefined) applyDigitSelection(msg.sel);
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
  applyDigitSelection(i);
  ws.send(JSON.stringify({cmd:'seldigit', idx:i}));
}

function step(dir){
  const m = selDigit===0?100:selDigit===1?10:1;
  curVal += dir*m;
  if(curVal<0) curVal=0;
  if(curVal>999) curVal=999;
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
      const nv=prompt('Neuer Wert (0-999):',v);
      if(nv===null)return;
      const iv=parseInt(nv);
      if(isNaN(iv)||iv<0||iv>999)return;
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
        "{\"type\":\"state\",\"val\":%d,\"def\":%s,\"ae\":%s,\"sel\":%d}",
        (int)config_value, defArr, autoenter ? "true" : "false", selected_digit);

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
                if(v > 999) v = 999;
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
            if(v > 999) v = 999;
            config_value = v;
            ws_send_val();
            WsCmdMsg m = {WS_CMD_VAL, v, 0, false};
            xQueueSend(ws_cmd_queue, &m, 0);
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

static void start_webserver(void)
{
    if(webserver_running) return;
    if(!ws_cmd_queue) ws_cmd_queue = xQueueCreate(8, sizeof(WsCmdMsg));
    ws.onEvent(onWsEvent);
    webServer.addHandler(&ws);
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest * req){
        req->send_P(200, "text/html", WEB_PAGE);
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
    stop_webserver();
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    delay(100);

    if(wifi_mode_setting == 0) {
        WiFi.mode(WIFI_OFF);
        Serial.println("WiFi AUS");
        if(ip_label) lv_label_set_text(ip_label, "");
    }
    else if(wifi_mode_setting == 1) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32-ATT", "12345678");
        Serial.printf("AP gestartet, IP: %s\\n", WiFi.softAPIP().toString().c_str());
        if(ip_label) lv_label_set_text_fmt(ip_label, "IP: %s", WiFi.softAPIP().toString().c_str());
        start_webserver();
    }
    else {
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        Serial.print("WiFi verbinde");
        for(int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
            delay(500);
            Serial.print('.');
        }
        if(WiFi.status() == WL_CONNECTED) {
            Serial.printf("\\nWiFi OK, IP: %s\\n", WiFi.localIP().toString().c_str());
            if(ip_label) lv_label_set_text_fmt(ip_label, "IP: %s", WiFi.localIP().toString().c_str());
            start_webserver();
        } else {
            Serial.println("\\nWiFi nicht verbunden");
            if(ip_label) lv_label_set_text(ip_label, "Nicht verbunden");
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
        }
    }
}

/* Called from main.cpp whenever config_value changes from LVGL side */
static void ws_broadcast_val(void)        { ws_send_val(); }
static void ws_broadcast_def(int i)       { ws_send_def(i); }
static void ws_broadcast_ae(void)         { ws_send_ae(); }
static void ws_broadcast_active_def(int i){ ws_send_active_def(i); }
static void ws_broadcast_seldigit(int i)  { selected_digit = i; ws_send_seldigit(); }
