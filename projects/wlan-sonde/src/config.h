// Feste Einstellungen der wlan-sonde. Alle Werte lassen sich per build_flags
// in platformio.ini überschreiben; Gerätename, Rolle, Bereich usw. werden zur
// Laufzeit im Portal bzw. in der Weboberfläche eingestellt (einstellungen.h).
#pragma once
#include <Arduino.h>

#ifndef MONITOR_SPEED
#define MONITOR_SPEED 115200
#endif
#ifndef BUTTON_PIN
#define BUTTON_PIN 0                    // BOOT-Taster
#endif
#ifndef DEFAULT_ROLLE
#define DEFAULT_ROLLE "sonde"           // sonde | mobil | zentrale
#endif
#ifndef DEFAULT_INTERVALL
#define DEFAULT_INTERVALL 30            // s zwischen zwei Messungen (Sonde/Zentrale)
#endif
#ifndef AP_PASS
#define AP_PASS "12345678"              // Passwort des Konfigurations-Hotspots
#endif
#ifndef NTP_SERVER
#define NTP_SERVER "fritz.box"          // FRITZ!Box als Zeitserver, sonst pool.ntp.org
#endif
#ifndef TZ_INFO
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
#endif
#ifndef ROUTER_PORT
#define ROUTER_PORT 80                  // Antwortzeit = TCP-Verbindungsaufbau zum Router
#endif

// Pufferspeicher für Messungen (Anzahl Einträge)
#ifndef PUFFER_SONDE
#define PUFFER_SONDE 240                // eigene Messungen, z. B. 2 h bei 30 s
#endif
#ifndef PUFFER_ZENTRALE
#define PUFFER_ZENTRALE 400             // alle Sonden, ohne PSRAM
#endif
#ifndef PUFFER_ZENTRALE_PSRAM
#define PUFFER_ZENTRALE_PSRAM 6000      // alle Sonden, mit PSRAM (z. B. Waveshare-Matrix)
#endif

// LED-Matrix (optional): aktiv, wenn LED_PIN oder MATRIX_PIN gesetzt ist
#if defined(LED_PIN) || defined(MATRIX_PIN)
  #define WLAN_MATRIX 1
  #ifndef LED_PIN
    #define LED_PIN MATRIX_PIN
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
    #define SERPENTINE true
  #endif
  #ifndef LED_COLOR_ORDER
    #define LED_COLOR_ORDER GRB
  #endif
  #ifndef BRIGHTNESS
    #define BRIGHTNESS 40
  #endif
  #ifndef MAX_MILLIAMPS
    #define MAX_MILLIAMPS 1500
  #endif
#endif

// OLED (optional): aktiv, wenn OLED_WIDTH gesetzt ist und keine LED-Matrix
// (z. B. aus boards.ini: esp32c3_oled_base, oder per build_flags für ein externes SSD1306)
#if defined(OLED_WIDTH) && !defined(WLAN_MATRIX)
  #define WLAN_OLED 1
  #ifndef OLED_HEIGHT
    #define OLED_HEIGHT 64
  #endif
  #ifndef OLED_SDA_PIN
    #define OLED_SDA_PIN SDA
  #endif
  #ifndef OLED_SCL_PIN
    #define OLED_SCL_PIN SCL
  #endif
  #ifndef OLED_SEITE_MS
    #define OLED_SEITE_MS 4000          // ms je Seite
  #endif
#endif
