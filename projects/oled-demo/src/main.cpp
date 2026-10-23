// oled-demo: Funktionstest für Boards mit kleinem OLED.
// Zeigt nacheinander Rahmentest, Chip-Infos, WLAN-Scan und Laufzeit an und
// gibt dasselbe seriell aus. Display-Pins und -Größe kommen aus boards/boards.ini
// bzw. platformio.ini (OLED_SDA_PIN, OLED_SCL_PIN, OLED_RST_PIN, OLED_WIDTH, OLED_HEIGHT).
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

#ifndef OLED_RST_PIN
#define OLED_RST_PIN U8X8_PIN_NONE
#endif
#ifndef LED_BUILTIN_PIN
#define LED_BUILTIN_PIN 2
#endif
#if defined(LED_ACTIVE_LOW) || defined(ESP8266)
#define LED_ON LOW
#else
#define LED_ON HIGH
#endif

#if OLED_WIDTH == 72 && OLED_HEIGHT == 40
U8G2_SSD1306_72X40_ER_F_HW_I2C oled(U8G2_R0, OLED_RST_PIN, OLED_SCL_PIN, OLED_SDA_PIN);
#elif OLED_WIDTH == 128 && OLED_HEIGHT == 32
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C oled(U8G2_R0, OLED_RST_PIN, OLED_SCL_PIN, OLED_SDA_PIN);
#elif OLED_WIDTH == 128 && OLED_HEIGHT == 64
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, OLED_RST_PIN, OLED_SCL_PIN, OLED_SDA_PIN);
#else
#error "Unbekannte OLED-Größe – OLED_WIDTH/OLED_HEIGHT prüfen"
#endif

static const char *boardName() {
#if defined(BOARD_HELTEC_WIFI_KIT_8)
  return "WiFi Kit 8";
#elif defined(BOARD_ESP32C3_OLED)
  return "C3 OLED";
#elif defined(BOARD_DOIT_DEVKIT_V1)
  return "DOIT V1";
#elif defined(ESP8266)
  return "NodeMCU";
#else
  return "ESP32";
#endif
}

static const char *chipName() {
#if defined(ESP32)
  return ESP.getChipModel();
#else
  return "ESP8266";
#endif
}

static const uint8_t LINE_H = 10;                        // Zeilenhöhe der Schrift 6x10
static const uint8_t LINES  = OLED_HEIGHT / LINE_H;      // 3 (128x32), 4 (72x40), 6 (128x64)

static char lines[4][24];
static int page = 0;
static int netCount = -1;
static int bestRssi = 0;
static char bestSsid[33] = "";
static uint32_t lastScan = 0;

static void show(const char *a, const char *b, const char *c, const char *d) {
  const char *src[4] = {a, b, c, d};
  oled.clearBuffer();
  for (uint8_t i = 0; i < 4 && i < LINES; i++) {
    oled.drawStr(0, (i + 1) * LINE_H - 1, src[i]);
  }
  oled.sendBuffer();
  Serial.printf("[%s] %s | %s | %s | %s\n", boardName(), a, b, c, d);
}

static void frameTest() {
  // Rahmen um die volle Fläche: alle vier Kanten müssen sichtbar sein,
  // sonst stimmt der Treiber/Offset nicht (siehe Board-Doku).
  oled.clearBuffer();
  oled.drawFrame(0, 0, OLED_WIDTH, OLED_HEIGHT);
  oled.drawLine(0, 0, OLED_WIDTH - 1, OLED_HEIGHT - 1);
  oled.drawLine(0, OLED_HEIGHT - 1, OLED_WIDTH - 1, 0);
  oled.sendBuffer();
  Serial.printf("Rahmentest %dx%d – alle Kanten sichtbar?\n", OLED_WIDTH, OLED_HEIGHT);
}

static void scan() {
  digitalWrite(LED_BUILTIN_PIN, LED_ON);                  // LED leuchtet während des Scans
  netCount = WiFi.scanNetworks();
  bestRssi = -127;
  bestSsid[0] = '\0';
  for (int i = 0; i < netCount; i++) {
    if (WiFi.RSSI(i) > bestRssi) {
      bestRssi = WiFi.RSSI(i);
      strncpy(bestSsid, WiFi.SSID(i).c_str(), sizeof(bestSsid) - 1);
      bestSsid[sizeof(bestSsid) - 1] = '\0';
    }
  }
  WiFi.scanDelete();
  digitalWrite(LED_BUILTIN_PIN, !LED_ON);
  lastScan = millis();
}

void setup() {
  Serial.begin(MONITOR_SPEED);
  pinMode(LED_BUILTIN_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN_PIN, !LED_ON);
  delay(300);
  Serial.printf("\noled-demo auf %s, OLED %dx%d an SDA %d / SCL %d\n",
                boardName(), OLED_WIDTH, OLED_HEIGHT, OLED_SDA_PIN, OLED_SCL_PIN);

  oled.begin();
  oled.setFont(u8g2_font_6x10_tf);
  frameTest();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(2000);
}

void loop() {
  if (netCount < 0 || millis() - lastScan > 30000) scan();

  switch (page) {
    case 0:
      snprintf(lines[0], sizeof(lines[0]), "%s", boardName());
      snprintf(lines[1], sizeof(lines[1]), "%s", chipName());
      snprintf(lines[2], sizeof(lines[2]), "Flash %luMB", (unsigned long)(ESP.getFlashChipSize() >> 20));
      snprintf(lines[3], sizeof(lines[3]), "%s", WiFi.macAddress().substring(6).c_str());
      break;
    case 1:
      snprintf(lines[0], sizeof(lines[0]), "WLAN: %d", netCount);
      snprintf(lines[1], sizeof(lines[1]), "%.12s", netCount > 0 ? bestSsid : "-");
      snprintf(lines[2], sizeof(lines[2]), "%d dBm", netCount > 0 ? bestRssi : 0);
      snprintf(lines[3], sizeof(lines[3]), "%s", "staerkstes");
      break;
    default:
      snprintf(lines[0], sizeof(lines[0]), "Laufzeit");
      snprintf(lines[1], sizeof(lines[1]), "%lus", (unsigned long)(millis() / 1000));
      snprintf(lines[2], sizeof(lines[2]), "Heap %luk", (unsigned long)(ESP.getFreeHeap() / 1024));
      snprintf(lines[3], sizeof(lines[3]), "LED %d", LED_BUILTIN_PIN);
      break;
  }
  show(lines[0], lines[1], lines[2], lines[3]);
  page = (page + 1) % 3;
  delay(3000);
}
