// webserver-ota: Verbindet sich mit dem WLAN, stellt eine kleine Weboberfläche
// bereit (Status + LED schalten) und nimmt Firmware-Updates per OTA an.
// Läuft unverändert auf ESP32 und ESP8266.
#include <Arduino.h>
#include <ArduinoOTA.h>

#if defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #include <ESPmDNS.h>
  using WebServerT = WebServer;
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <ESP8266mDNS.h>
  using WebServerT = ESP8266WebServer;
#endif

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #warning "include/secrets.h fehlt – verwende secrets.example.h"
  #include "secrets.example.h"
#endif

#ifndef LED_BUILTIN_PIN
#define LED_BUILTIN_PIN 2
#endif

static WebServerT server(80);
static bool ledOn = false;

static uint32_t freeHeap() { return ESP.getFreeHeap(); }

static void setLed(bool on) {
  ledOn = on;
#if defined(ESP8266)
  digitalWrite(LED_BUILTIN_PIN, on ? LOW : HIGH);  // ESP8266-LED ist active low
#else
  digitalWrite(LED_BUILTIN_PIN, on ? HIGH : LOW);
#endif
}

static void handleRoot() {
  String html;
  html.reserve(1024);
  html += F("<!doctype html><html lang=de><meta charset=utf-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1'>"
            "<title>" HOSTNAME "</title>"
            "<style>body{font-family:sans-serif;max-width:30em;margin:2em auto;padding:0 1em}"
            "td{padding:.2em .8em}a.b{display:inline-block;padding:.5em 1em;border:1px solid #888;"
            "border-radius:.4em;text-decoration:none}</style>"
            "<h1>" HOSTNAME "</h1><table>");
  html += "<tr><td>IP</td><td>" + WiFi.localIP().toString() + "</td></tr>";
  html += "<tr><td>RSSI</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  html += "<tr><td>Laufzeit</td><td>" + String(millis() / 1000) + " s</td></tr>";
  html += "<tr><td>Freier Heap</td><td>" + String(freeHeap()) + " B</td></tr>";
  html += "<tr><td>LED</td><td>" + String(ledOn ? "an" : "aus") + "</td></tr>";
  html += F("</table><p><a class=b href=/led?state=1>LED an</a> "
            "<a class=b href=/led?state=0>LED aus</a></p>"
            "<p><a href=/api/status>/api/status</a> (JSON)</p></html>");
  server.send(200, "text/html", html);
}

static void handleLed() {
  if (server.hasArg("state")) setLed(server.arg("state") == "1");
  server.sendHeader("Location", "/");
  server.send(303);
}

static void handleStatus() {
  String json = "{\"host\":\"" HOSTNAME "\",\"ip\":\"" + WiFi.localIP().toString() +
                "\",\"rssi\":" + String(WiFi.RSSI()) +
                ",\"uptime\":" + String(millis() / 1000) +
                ",\"heap\":" + String(freeHeap()) +
                ",\"led\":" + String(ledOn ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

static void connectWifi() {
  WiFi.mode(WIFI_STA);
#if defined(ESP32)
  WiFi.setHostname(HOSTNAME);
#else
  WiFi.hostname(HOSTNAME);
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Verbinde mit WLAN \"%s\"", WIFI_SSID);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 30000) {
      Serial.println(F("\nWLAN-Verbindung fehlgeschlagen – Neustart"));
      ESP.restart();
    }
    delay(500);
    Serial.print('.');
  }
  Serial.printf("\nVerbunden, IP: %s\n", WiFi.localIP().toString().c_str());
}

static void setupOta() {
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() { Serial.println(F("OTA-Update startet")); });
  ArduinoOTA.onEnd([]() { Serial.println(F("\nOTA-Update fertig")); });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    Serial.printf("OTA: %u%%\r", done * 100 / total);
  });
  ArduinoOTA.onError([](ota_error_t err) { Serial.printf("OTA-Fehler %u\n", err); });
  ArduinoOTA.begin();
}

void setup() {
  Serial.begin(MONITOR_SPEED);
  pinMode(LED_BUILTIN_PIN, OUTPUT);
  setLed(false);
  delay(200);
  Serial.println();

  connectWifi();
  setupOta();  // startet auch mDNS (HOSTNAME.local)
  MDNS.addService("http", "tcp", 80);

  server.on("/", handleRoot);
  server.on("/led", handleLed);
  server.on("/api/status", handleStatus);
  server.onNotFound([]() { server.send(404, "text/plain", "Nicht gefunden"); });
  server.begin();
  Serial.printf("Weboberfläche: http://%s.local/\n", HOSTNAME);
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
#if defined(ESP8266)
  MDNS.update();
#endif
}
