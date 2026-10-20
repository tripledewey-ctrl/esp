// sensor-mqtt: Liest einen BME280 per I2C und veröffentlicht die Messwerte als
// JSON per MQTT. Meldet sich per MQTT-Discovery automatisch bei Home Assistant an.
//
// Verdrahtung (I2C, Standardpins):
//   ESP32:          SDA = GPIO21, SCL = GPIO22
//   ESP32-S3:       SDA = GPIO8,  SCL = GPIO9
//   ESP32-C3:       SDA = GPIO8,  SCL = GPIO9
//   ESP8266/D1mini: SDA = D2 (GPIO4), SCL = D1 (GPIO5)
#include <Arduino.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #warning "include/secrets.h fehlt – verwende secrets.example.h"
  #include "secrets.example.h"
#endif

#ifndef PUBLISH_INTERVAL_MS
#define PUBLISH_INTERVAL_MS 30000
#endif

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);
static Adafruit_BME280 bme;
static bool bmeOk = false;

static const String stateTopic = String(HOSTNAME) + "/state";
static const String availTopic = String(HOSTNAME) + "/status";

static void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Verbinde mit WLAN \"%s\"", WIFI_SSID);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 30000) ESP.restart();
    delay(500);
    Serial.print('.');
  }
  Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());
}

// Home-Assistant-Discovery: ein Konfigurations-Topic pro Messwert.
static void publishDiscovery(const char* key, const char* name, const char* unit,
                             const char* deviceClass) {
  JsonDocument doc;
  doc["name"] = name;
  doc["unique_id"] = String(HOSTNAME) + "_" + key;
  doc["state_topic"] = stateTopic;
  doc["availability_topic"] = availTopic;
  doc["unit_of_measurement"] = unit;
  doc["device_class"] = deviceClass;
  doc["state_class"] = "measurement";
  doc["value_template"] = String("{{ value_json.") + key + " }}";
  JsonObject dev = doc["device"].to<JsonObject>();
  dev["identifiers"][0] = HOSTNAME;
  dev["name"] = HOSTNAME;
  dev["model"] = "ESP + BME280";

  String payload;
  serializeJson(doc, payload);
  const String topic = String("homeassistant/sensor/") + HOSTNAME + "/" + key + "/config";
  mqtt.publish(topic.c_str(), payload.c_str(), true);
}

static void connectMqtt() {
  while (!mqtt.connected()) {
    Serial.printf("Verbinde mit MQTT %s:%d ... ", MQTT_HOST, MQTT_PORT);
    const char* user = strlen(MQTT_USER) ? MQTT_USER : nullptr;
    const char* pass = strlen(MQTT_PASSWORD) ? MQTT_PASSWORD : nullptr;
    if (mqtt.connect(HOSTNAME, user, pass, availTopic.c_str(), 1, true, "offline")) {
      Serial.println(F("ok"));
      mqtt.publish(availTopic.c_str(), "online", true);
      if (bmeOk) {
        publishDiscovery("temperature", "Temperatur", "°C", "temperature");
        publishDiscovery("humidity", "Luftfeuchte", "%", "humidity");
        publishDiscovery("pressure", "Luftdruck", "hPa", "atmospheric_pressure");
      }
      publishDiscovery("rssi", "WLAN-Signal", "dBm", "signal_strength");
    } else {
      Serial.printf("Fehler %d, neuer Versuch in 5 s\n", mqtt.state());
      delay(5000);
    }
  }
}

static void publishState() {
  JsonDocument doc;
  if (bmeOk) {
    doc["temperature"] = roundf(bme.readTemperature() * 10) / 10.0f;
    doc["humidity"] = roundf(bme.readHumidity() * 10) / 10.0f;
    doc["pressure"] = roundf(bme.readPressure() / 10.0f) / 10.0f;  // Pa -> hPa
  }
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;

  String payload;
  serializeJson(doc, payload);
  mqtt.publish(stateTopic.c_str(), payload.c_str());
  Serial.println(payload);
}

void setup() {
  Serial.begin(MONITOR_SPEED);
  delay(200);
  Serial.println();

  Wire.begin();
  bmeOk = bme.begin(0x76) || bme.begin(0x77);
  if (!bmeOk) Serial.println(F("Kein BME280 gefunden – sende nur WLAN-Daten"));

  connectWifi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(768);  // Discovery-Nachrichten sind größer als 256 Byte
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWifi();
  if (!mqtt.connected()) connectMqtt();
  mqtt.loop();

  static uint32_t last = 0;
  if (last == 0 || millis() - last >= PUBLISH_INTERVAL_MS) {
    last = millis();
    publishState();
  }
}
