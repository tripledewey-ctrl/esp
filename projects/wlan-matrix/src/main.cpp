/******************************************************************************
 * wlan-matrix – WLAN-Signalstärke-Matrix mit Weboberfläche
 * -----------------------------------------------------------------------------
 * Misst die WLAN-Feldstärke und zeigt sie farbig auf einer RGB-LED-Matrix an.
 * Gedacht für die Ausleuchtungsprüfung in Mesh-Netzen (z. B. FRITZ!Box +
 * Repeater/Access Points mit gleicher SSID).
 *
 * - Erster Start: Hotspot AP_NAME öffnet ein Portal für SSID + Passwort
 *   (WiFiManager). Die Zugangsdaten bleiben im Flash.
 * - Live-Modus: Balken für die (geglättete) Signalstärke, weißer Punkt oben =
 *   Kennung des verbundenen Mesh-Knotens.
 * - Mesh-Modus: eine Zeile je Knoten mit gleicher SSID, stärkster oben,
 *   der verbundene Knoten blinkt.
 * - WLAN-Erkennung: Verbindungsabbrüche mit Ursache, AP-Wechsel (Roaming),
 *   automatische Wiederverbindung, Ereignisprotokoll.
 * - Weboberfläche unter http://HOSTNAME.local/ mit Status, Knotenliste,
 *   Ereignissen und JSON-API (/api/status).
 *
 * Taster (GPIO0 / BOOT):
 *   kurz  (< 1 s)  -> Live-Modus <-> Mesh-Modus
 *   lang  (> 3 s)  -> WLAN-Zugangsdaten löschen, Portal neu starten
 *
 * Farbcode:  grün >= -55 · hellgrün >= -65 · gelb >= -72 · orange >= -80 · rot
 *****************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <FastLED.h>
#include <ArduinoJson.h>

// ============================ KONFIGURATION =================================
// Alle Werte lassen sich per build_flags in platformio.ini überschreiben.
#ifndef LED_PIN
  #ifdef MATRIX_PIN
    #define LED_PIN MATRIX_PIN
  #else
    #define LED_PIN 13                 // Datenpin zur Matrix (DIN)
  #endif
#endif
#ifndef MATRIX_W
  #ifdef MATRIX_WIDTH
    #define MATRIX_W MATRIX_WIDTH
  #else
    #define MATRIX_W 8
  #endif
#endif
#ifndef MATRIX_H
  #ifdef MATRIX_HEIGHT
    #define MATRIX_H MATRIX_HEIGHT
  #else
    #define MATRIX_H 8
  #endif
#endif
#ifndef SERPENTINE
#define SERPENTINE true                // Zickzack-Verdrahtung
#endif
#ifndef LED_COLOR_ORDER
#define LED_COLOR_ORDER GRB            // bei falschen Farben RGB/BRG testen
#endif
#ifndef BRIGHTNESS
#define BRIGHTNESS 40                  // 0..255 – bewusst niedrig (Strom!)
#endif
#ifndef MAX_MILLIAMPS
#define MAX_MILLIAMPS 1500             // Strombegrenzung der Matrix
#endif
#ifndef BUTTON_PIN
#define BUTTON_PIN 0                   // BOOT-Taster
#endif
#ifndef AP_NAME
#define AP_NAME "ESP32-WLAN-Matrix"    // Konfigurations-Hotspot
#endif
#ifndef AP_PASS
#define AP_PASS "12345678"             // mind. 8 Zeichen – bitte ändern
#endif
#ifndef HOSTNAME
#define HOSTNAME "wlan-matrix"         // -> http://wlan-matrix.local/
#endif
#ifndef SCAN_INTERVAL
#define SCAN_INTERVAL 10000UL          // ms zwischen zwei Umgebungs-Scans
#endif
#ifndef MONITOR_SPEED
#define MONITOR_SPEED 115200
#endif

static constexpr uint32_t LINK_CHECK_MS     = 500;     // Verbindung prüfen
static constexpr uint32_t RECONNECT_AFTER   = 30000;   // eigener Reconnect-Versuch
static constexpr uint32_t RESTART_AFTER     = 300000;  // Neustart nach 5 min offline
static constexpr float    RSSI_SMOOTHING    = 0.3f;    // Gewicht neuer Messwerte
static constexpr int      MAX_NODES         = 16;
static constexpr int      MAX_EVENTS        = 20;
// ============================================================================

#define NUM_LEDS (MATRIX_W * MATRIX_H)
static CRGB leds[NUM_LEDS];
static WebServer server(80);

enum Mode : uint8_t { MODE_LIVE = 0, MODE_MESH = 1 };
static Mode mode = MODE_LIVE;

struct MeshNode {
  uint8_t bssid[6];
  int32_t rssi;
  int32_t ch;
};
static MeshNode nodes[MAX_NODES];
static int nodeCount = 0;
static bool scanning = false;
static bool scanRequested = false;
static uint32_t lastScan = 0;
static String targetSsid;

// Zustand der Verbindung
static bool linkUp = false;
static uint8_t curBssid[6] = {0};
static bool haveBssid = false;
static float rssiAvg = -100;
static int32_t rssiNow = -100;
static uint32_t drops = 0, roams = 0;
static uint32_t downSince = 0, lastReconnect = 0;
static volatile uint8_t lastDiscReason = 0;   // aus dem WLAN-Ereignis-Task

struct Event {
  uint32_t t;
  char msg[72];
};
static Event events[MAX_EVENTS];
static int eventHead = 0, eventCount = 0;

// ------------------------------ Hilfsfunktionen -----------------------------
static String bssidStr(const uint8_t* b) {
  char s[18];
  snprintf(s, sizeof(s), "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  return s;
}

static void addEvent(const char* fmt, ...) {
  Event& e = events[eventHead];
  e.t = millis() / 1000;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(e.msg, sizeof(e.msg), fmt, ap);
  va_end(ap);
  eventHead = (eventHead + 1) % MAX_EVENTS;
  if (eventCount < MAX_EVENTS) eventCount++;
  Serial.printf("[%6lus] %s\n", (unsigned long)e.t, e.msg);
}

// Häufige Abbruchgründe (esp_wifi_types.h) in Klartext
static const char* reasonText(uint8_t r) {
  switch (r) {
    case 2:   return "Authentifizierung abgelaufen";
    case 3:   return "vom AP abgemeldet";
    case 4:   return "Inaktivität";
    case 8:   return "Station hat Verbindung beendet";
    case 15:  return "4-Way-Handshake Timeout (Passwort?)";
    case 200: return "Beacon-Timeout (Signal verloren)";
    case 201: return "kein AP mit dieser SSID gefunden";
    case 202: return "Authentifizierung fehlgeschlagen";
    case 203: return "Assoziierung fehlgeschlagen";
    case 204: return "Handshake Timeout";
    default:  return "sonstiger Grund";
  }
}

static const char* rssiLabel(int32_t rssi) {
  if (rssi >= -55) return "sehr gut";
  if (rssi >= -65) return "gut";
  if (rssi >= -72) return "brauchbar";
  if (rssi >= -80) return "schwach";
  return "kritisch";
}

static CRGB rssiColor(int32_t rssi) {
  if (rssi >= -55) return CRGB(0, 255, 0);      // grün
  if (rssi >= -65) return CRGB(128, 255, 0);    // hellgrün
  if (rssi >= -72) return CRGB(255, 200, 0);    // gelb
  if (rssi >= -80) return CRGB(255, 70, 0);     // orange
  return CRGB(255, 0, 0);                       // rot
}

// -90 dBm -> 1 Balken, -45 dBm -> MATRIX_H Balken
static uint8_t rssiBars(int32_t rssi) {
  if (rssi >= -45) return MATRIX_H;
  if (rssi <= -90) return 1;
  return (uint8_t)(1 + ((rssi + 90) * (MATRIX_H - 1)) / 45);
}

// --------------------------- Matrix-Hilfsfunktionen -------------------------
// Ursprung: x=0 links, y=0 UNTEN. Erscheint das Bild gespiegelt, SERPENTINE
// umschalten oder die Matrix um 180 Grad drehen.
static uint16_t XY(int x, int y) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return NUM_LEDS - 1;
  const int row = MATRIX_H - 1 - y;
  if (SERPENTINE && (row & 1)) return row * MATRIX_W + (MATRIX_W - 1 - x);
  return row * MATRIX_W + x;
}

static void fillAll(CRGB c) {
  fill_solid(leds, NUM_LEDS, c);
  FastLED.show();
}

static void bootAnimation() {
  for (int y = 0; y < MATRIX_H; y++) {
    FastLED.clear();
    for (int x = 0; x < MATRIX_W; x++) leds[XY(x, y)] = CRGB(0, 0, 120);
    FastLED.show();
    delay(60);
  }
  FastLED.clear();
  FastLED.show();
}

// ------------------------------ Anzeigemodi ---------------------------------
static void showLive() {
  FastLED.clear();
  if (!linkUp) {                                    // rotes X = keine Verbindung
    for (int i = 0; i < MATRIX_H && i < MATRIX_W; i++) {
      leds[XY(i, i)] = CRGB::Red;
      leds[XY(i, MATRIX_H - 1 - i)] = CRGB::Red;
    }
    FastLED.show();
    return;
  }

  const int32_t r = (int32_t)lroundf(rssiAvg);
  const uint8_t b = rssiBars(r);
  const CRGB c = rssiColor(r);
  for (int y = 0; y < b; y++)
    for (int x = 0; x < MATRIX_W; x++) leds[XY(x, y)] = c;

  // Weißer Punkt oben = Kennung des Mesh-Knotens (letztes BSSID-Byte). Springt
  // er beim Rundgang, hat ein AP-Wechsel stattgefunden.
  leds[XY(curBssid[5] % MATRIX_W, MATRIX_H - 1)] = CRGB::White;
  FastLED.show();
}

static void showMesh() {
  FastLED.clear();
  if (nodeCount == 0) {                             // noch kein Scanergebnis
    leds[XY(0, 0)] = CRGB(0, 0, 120);
    FastLED.show();
    return;
  }
  for (int i = 0; i < nodeCount && i < MATRIX_H; i++) {
    const int y = MATRIX_H - 1 - i;                 // stärkster Knoten oben
    const uint8_t b = rssiBars(nodes[i].rssi);
    const CRGB c = rssiColor(nodes[i].rssi);
    for (int x = 0; x < b && x < MATRIX_W; x++) leds[XY(x, y)] = c;
    if (linkUp && memcmp(nodes[i].bssid, curBssid, 6) == 0)  // verbundener Knoten blinkt
      leds[XY(MATRIX_W - 1, y)] = ((millis() / 400) % 2) ? CRGB::White : c;
  }
  FastLED.show();
}

// --------------------------- WLAN-Erkennung ---------------------------------
static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  // läuft im WLAN-Task: nur Werte merken, ausgewertet wird in loop()
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    lastDiscReason = info.wifi_sta_disconnected.reason;
}

static void updateLink() {
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck < LINK_CHECK_MS) return;
  lastCheck = millis();

  const bool up = WiFi.status() == WL_CONNECTED;
  if (up != linkUp) {
    linkUp = up;
    if (up) {
      rssiAvg = WiFi.RSSI();
      addEvent("Verbunden: %s, Kanal %d, %d dBm, IP %s", bssidStr(WiFi.BSSID()).c_str(),
               (int)WiFi.channel(), (int)WiFi.RSSI(), WiFi.localIP().toString().c_str());
    } else {
      drops++;
      downSince = lastReconnect = millis();
      addEvent("Verbindung verloren (%u: %s)", lastDiscReason, reasonText(lastDiscReason));
    }
  }

  if (up) {
    const uint8_t* b = WiFi.BSSID();
    if (haveBssid && memcmp(b, curBssid, 6) != 0) {
      roams++;
      addEvent("AP-Wechsel: %s -> %s", bssidStr(curBssid).c_str(), bssidStr(b).c_str());
    }
    memcpy(curBssid, b, 6);
    haveBssid = true;
    rssiNow = WiFi.RSSI();
    rssiAvg = rssiAvg + RSSI_SMOOTHING * (rssiNow - rssiAvg);   // gleitender Mittelwert
    return;
  }

  // Offline: der ESP32 versucht selbst neu zu verbinden (autoReconnect). Hilft das
  // nicht, alle 30 s selbst anstoßen und nach 5 min neu starten.
  if (millis() - lastReconnect > RECONNECT_AFTER) {
    lastReconnect = millis();
    addEvent("Neuer Verbindungsversuch");
    WiFi.reconnect();
  }
  if (millis() - downSince > RESTART_AFTER) {
    addEvent("Seit 5 min offline – Neustart");
    delay(200);
    ESP.restart();
  }
}

// Gezielt mit einem bestimmten Mesh-Knoten verbinden (Test des besten APs)
static bool connectToNode(int idx) {
  if (idx < 0 || idx >= nodeCount) return false;
  const String pass = WiFi.psk();
  addEvent("Wechsel zu %s angefordert", bssidStr(nodes[idx].bssid).c_str());
  WiFi.disconnect(false, false);                    // WLAN an lassen, nichts löschen
  WiFi.begin(targetSsid.c_str(), pass.c_str(), nodes[idx].ch, nodes[idx].bssid);
  return true;
}

// ------------------------------ WLAN-Scan -----------------------------------
static void handleScan() {
  if (!scanning) {
    if (!linkUp) return;                            // Scan stört die Wiederverbindung
    if (!scanRequested && millis() - lastScan < SCAN_INTERVAL) return;
    scanRequested = false;
    WiFi.scanNetworks(true, false);                 // asynchron
    scanning = true;
    return;
  }

  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  if (n < 0) {                                      // WIFI_SCAN_FAILED
    scanning = false;
    lastScan = millis();
    return;
  }

  nodeCount = 0;
  for (int i = 0; i < n && nodeCount < MAX_NODES; i++) {
    if (WiFi.SSID(i) != targetSsid) continue;
    memcpy(nodes[nodeCount].bssid, WiFi.BSSID(i), 6);
    nodes[nodeCount].rssi = WiFi.RSSI(i);
    nodes[nodeCount].ch = WiFi.channel(i);
    nodeCount++;
  }
  for (int i = 1; i < nodeCount; i++) {             // stärkstes Signal zuerst
    const MeshNode key = nodes[i];
    int j = i - 1;
    while (j >= 0 && nodes[j].rssi < key.rssi) { nodes[j + 1] = nodes[j]; j--; }
    nodes[j + 1] = key;
  }

  Serial.printf("\n--- Scan: %d Knoten für SSID \"%s\" ---\n", nodeCount, targetSsid.c_str());
  for (int i = 0; i < nodeCount; i++) {
    Serial.printf("  %s  %4d dBm  Kanal %2d%s\n", bssidStr(nodes[i].bssid).c_str(),
                  (int)nodes[i].rssi, (int)nodes[i].ch,
                  (linkUp && memcmp(nodes[i].bssid, curBssid, 6) == 0) ? "   <== verbunden" : "");
  }

  WiFi.scanDelete();
  scanning = false;
  lastScan = millis();
}

// ------------------------------ Zugangsdaten --------------------------------
static void resetWifiAndRestart() {
  addEvent("Zugangsdaten werden gelöscht");
  fillAll(CRGB(0, 0, 120));
  WiFi.persistent(true);                            // Löschen dauerhaft speichern
  WiFiManager wm;
  wm.resetSettings();
  delay(500);
  ESP.restart();
}

// ------------------------------- Taster -------------------------------------
static void handleButton() {
  static bool pressed = false;
  static uint32_t tDown = 0;
  const bool down = digitalRead(BUTTON_PIN) == LOW;

  if (down && !pressed) { pressed = true; tDown = millis(); }
  if (down && pressed && millis() - tDown > 3000) resetWifiAndRestart();
  if (!down && pressed) {
    if (millis() - tDown < 1000) {
      mode = (mode == MODE_LIVE) ? MODE_MESH : MODE_LIVE;
      Serial.printf("Modus: %s\n", mode == MODE_LIVE ? "LIVE" : "MESH");
    }
    pressed = false;
  }
}

// ------------------------------ Webserver -----------------------------------
static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WLAN-Matrix</title>
<style>
:root{--bg:#fff;--fg:#1f2328;--mut:#59636e;--line:#d1d9e0;--card:#f6f8fa}
@media(prefers-color-scheme:dark){:root{--bg:#0d1117;--fg:#e6edf3;--mut:#9198a1;--line:#30363d;--card:#161b22}}
body{margin:0 auto;max-width:44em;padding:16px;font:15px/1.45 system-ui,sans-serif;background:var(--bg);color:var(--fg)}
h1{font-size:1.4em;margin:0 0 4px}.mut{color:var(--mut)}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 16px;margin:12px 0}
.big{font-size:2.4em;font-weight:700}.bar{height:14px;border-radius:7px;background:var(--line);overflow:hidden}
.bar>div{height:100%;transition:width .4s}
table{width:100%;border-collapse:collapse}td,th{text-align:left;padding:5px 6px;border-bottom:1px solid var(--line)}
.n{text-align:right;font-variant-numeric:tabular-nums}.dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px}
button{font:inherit;padding:6px 12px;border-radius:8px;border:1px solid var(--line);background:var(--bg);color:var(--fg);cursor:pointer}
.row{display:flex;gap:8px;flex-wrap:wrap}.ev{font-family:ui-monospace,monospace;font-size:.85em;white-space:pre-wrap}
</style></head><body>
<h1>WLAN-Matrix</h1><div class="mut" id="sub">lade …</div>
<div class="card">
 <div class="row" style="justify-content:space-between;align-items:baseline">
  <div><span class="big" id="rssi">–</span> dBm <span id="q"></span></div>
  <div class="mut" id="avg"></div></div>
 <div class="bar"><div id="bar"></div></div>
 <table style="margin-top:8px"><tr><td>Mesh-Knoten (BSSID)</td><td id="bssid"></td></tr>
 <tr><td>Kanal</td><td id="ch"></td></tr><tr><td>IP-Adresse</td><td id="ip"></td></tr>
 <tr><td>Abbrüche / AP-Wechsel</td><td id="dr"></td></tr><tr><td>Laufzeit</td><td id="up"></td></tr></table>
</div>
<div class="card"><div class="row" style="justify-content:space-between">
 <b>Mesh-Knoten mit gleicher SSID</b><span class="mut" id="scan"></span></div>
 <table id="nodes"></table>
 <p class="mut">Nur 2,4-GHz-Knoten – der ESP32 empfängt kein 5 GHz.</p></div>
<div class="card row">
 <button onclick="post('/api/mode')">Anzeige: <span id="mode"></span></button>
 <button onclick="post('/api/rescan')">Jetzt scannen</button>
 <button onclick="if(confirm('WLAN-Zugangsdaten löschen und Hotspot öffnen?'))post('/api/reset-wifi')">WLAN zurücksetzen</button>
</div>
<div class="card"><b>Ereignisse</b><div class="ev" id="ev"></div></div>
<script>
const col=r=>r>=-55?'#1a7f37':r>=-65?'#6cc644':r>=-72?'#d4a72c':r>=-80?'#e16f24':'#cf222e';
const $=id=>document.getElementById(id);
const dur=s=>`${Math.floor(s/3600)} h ${Math.floor(s/60)%60} min ${s%60} s`;
async function post(u,b){await fetch(u,{method:'POST',body:b});load()}
async function load(){
 try{const d=await (await fetch('/api/status')).json();
  $('sub').textContent=`${d.ssid} · ${d.hostname}.local`;
  $('rssi').textContent=d.connected?d.rssi:'–';
  $('q').innerHTML=d.connected?`<span class="dot" style="background:${col(d.rssi_avg)}"></span>${d.quality}`:'<b>nicht verbunden</b>';
  $('avg').textContent=d.connected?`Mittel ${d.rssi_avg} dBm`:'';
  const p=Math.max(0,Math.min(100,(d.rssi_avg+90)/45*100));
  $('bar').style.width=(d.connected?p:0)+'%';$('bar').style.background=col(d.rssi_avg);
  $('bssid').textContent=d.bssid;$('ch').textContent=d.channel;$('ip').textContent=d.ip;
  $('dr').textContent=`${d.drops} / ${d.roams}`;$('up').textContent=dur(d.uptime);
  $('mode').textContent=d.mode==='live'?'Live':'Mesh';
  $('scan').textContent=d.scan_age<0?'noch kein Scan':`Scan vor ${d.scan_age} s`;
  $('nodes').innerHTML='<tr><th>BSSID</th><th class="n">dBm</th><th class="n">Kanal</th><th></th></tr>'+d.nodes.map((n,i)=>
   `<tr><td><span class="dot" style="background:${col(n.rssi)}"></span>${n.bssid}</td><td class="n">${n.rssi}</td><td class="n">${n.channel}</td>
   <td>${n.connected?'<b>verbunden</b>':`<button onclick="post('/api/connect','${i}')">verbinden</button>`}</td></tr>`).join('');
  $('ev').textContent=d.events.map(e=>`[${dur(e.t)}] ${e.msg}`).join('\n');
 }catch(e){$('sub').textContent='keine Verbindung zum Gerät'}}
load();setInterval(load,2000);
</script></body></html>)rawliteral";

static void handleStatus() {
  JsonDocument doc;
  doc["ssid"] = targetSsid;
  doc["hostname"] = HOSTNAME;
  doc["connected"] = linkUp;
  doc["ip"] = WiFi.localIP().toString();
  doc["bssid"] = haveBssid ? bssidStr(curBssid) : "";
  doc["channel"] = linkUp ? WiFi.channel() : 0;
  doc["rssi"] = rssiNow;
  doc["rssi_avg"] = (int)lroundf(rssiAvg);
  doc["quality"] = rssiLabel((int32_t)lroundf(rssiAvg));
  doc["drops"] = drops;
  doc["roams"] = roams;
  doc["uptime"] = millis() / 1000;
  doc["mode"] = mode == MODE_LIVE ? "live" : "mesh";
  doc["scan_age"] = lastScan ? (int32_t)((millis() - lastScan) / 1000) : -1;

  JsonArray arr = doc["nodes"].to<JsonArray>();
  for (int i = 0; i < nodeCount; i++) {
    JsonObject n = arr.add<JsonObject>();
    n["bssid"] = bssidStr(nodes[i].bssid);
    n["rssi"] = nodes[i].rssi;
    n["channel"] = nodes[i].ch;
    n["connected"] = linkUp && memcmp(nodes[i].bssid, curBssid, 6) == 0;
  }
  JsonArray ev = doc["events"].to<JsonArray>();     // neueste zuerst
  for (int k = 0; k < eventCount; k++) {
    const Event& e = events[(eventHead - 1 - k + MAX_EVENTS) % MAX_EVENTS];
    JsonObject o = ev.add<JsonObject>();
    o["t"] = e.t;
    o["msg"] = e.msg;
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void setupWebServer() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/mode", HTTP_POST, []() {
    mode = (mode == MODE_LIVE) ? MODE_MESH : MODE_LIVE;
    server.send(204);
  });
  server.on("/api/rescan", HTTP_POST, []() {
    scanRequested = true;
    server.send(204);
  });
  server.on("/api/connect", HTTP_POST, []() {
    const int idx = server.arg("plain").toInt();
    server.send(connectToNode(idx) ? 204 : 400);
  });
  server.on("/api/reset-wifi", HTTP_POST, []() {
    server.send(204);
    delay(200);
    resetWifiAndRestart();
  });
  server.onNotFound([]() { server.send(404, "text/plain", "Nicht gefunden"); });
  server.begin();
}

// -------------------------------- Setup -------------------------------------
void setup() {
  Serial.begin(MONITOR_SPEED);
  delay(300);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);
  bootAnimation();

  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setHostname(HOSTNAME);
  wm.setConfigPortalTimeout(300);                   // 5 min, dann Neustart
  wm.setAPCallback([](WiFiManager*) {               // Portal aktiv = blau
    fillAll(CRGB(0, 0, 120));
    Serial.printf("Hotspot \"%s\" offen, Passwort \"%s\"\n", AP_NAME, AP_PASS);
  });

  fillAll(CRGB(60, 60, 0));                         // gelb = verbinde …
  if (!wm.autoConnect(AP_NAME, AP_PASS)) {
    Serial.println("Keine Verbindung – Neustart");
    fillAll(CRGB::Red);
    delay(2000);
    ESP.restart();
  }

  // Ab hier: gezielte Verbindungen (connectToNode) nicht dauerhaft speichern,
  // sonst bliebe der ESP32 nach einem Neustart an einem festen Knoten hängen.
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(onWifiEvent);

  targetSsid = WiFi.SSID();
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  setupWebServer();
  Serial.printf("Weboberfläche: http://%s.local/  bzw. http://%s/\n", HOSTNAME,
                WiFi.localIP().toString().c_str());

  fillAll(CRGB(0, 60, 0));
  delay(500);
}

// -------------------------------- Loop --------------------------------------
void loop() {
  handleButton();
  updateLink();
  handleScan();
  server.handleClient();

  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 200) {
    lastDraw = millis();
    if (mode == MODE_LIVE) showLive(); else showMesh();
  }
}
