// Messung: Verbindungsüberwachung, Scan (Mesh-Knoten + Fremdnetze),
// Antwortzeit des Routers und der Ablauf einer Messung.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

static constexpr int MAX_KNOTEN = 8;      // Mesh-Knoten (gleiche SSID) je Messung
static constexpr int KANAELE = 13;        // 2,4-GHz-Kanäle 1–13
static constexpr uint16_t KEINE_ANTWORT = 0xFFFF;

struct Knoten {
  uint8_t bssid[6];
  int8_t rssi;
  uint8_t kanal;
};

struct Fremd {          // fremde Netze auf einem Kanal
  uint8_t anzahl;
  int8_t max;           // stärkstes fremdes Netz in dBm
};

struct Messung {
  uint32_t t = 0;                 // Unix-Zeit, 0 = unbekannt
  uint32_t uptime = 0;            // s seit Start der Sonde
  uint8_t sonde = 0;              // Index in der Sondentabelle (0 = dieses Gerät)
  bool verbunden = false;
  bool istPunkt = false;          // Messpunkt einer Begehung (Rolle mobil)
  int8_t rssi = -127;             // Mittelwert im Messzeitraum
  int8_t rssiMin = -127;
  int8_t rssiMax = -127;
  uint8_t kanal = 0;
  uint8_t bssid[6] = {0};         // verbundener Mesh-Knoten
  uint16_t abbrueche = 0;         // Zähler seit Start
  uint16_t wechsel = 0;           // AP-Wechsel seit Start
  uint16_t antwort = KEINE_ANTWORT;  // Antwortzeit Router in 0,1 ms
  uint8_t verlust = 0;            // verlorene Router-Anfragen in %
  uint8_t nKnoten = 0;
  Knoten knoten[MAX_KNOTEN];
  Fremd fremd[KANAELE] = {};
  char punkt[16] = "";            // Messpunkt-ID (nur Begehung)
  char punktBereich[16] = "";     // Bereich des Messpunkts (nur Begehung)
};

// Zustand der WLAN-Verbindung, laufend aktualisiert
struct Verbindung {
  bool up = false;
  bool hatBssid = false;
  uint8_t bssid[6] = {0};
  uint8_t kanal = 0;
  int32_t rssi = -127;
  float rssiMittel = -100;
  uint32_t abbrueche = 0;
  uint32_t wechsel = 0;
};
extern Verbindung verbindung;

enum class Phase : uint8_t { Ruhe, Sammeln, Scan, Router };

void verbindungStart();          // nach dem ersten Verbinden aufrufen
void verbindungLoop();

bool messungStarten(const char* punkt = nullptr, const char* bereich = nullptr);
void messungLoop();
bool messungAbholen(Messung& out);   // true, sobald eine neue Messung fertig ist
Phase messungPhase();
uint8_t messungFortschritt();        // 0–100 %
bool hatLetzteMessung();
const Messung& letzteMessung();

uint32_t jetztUnix();
String bssidText(const uint8_t* b);
bool bssidAusText(const char* text, uint8_t* out);
const char* rssiBewertung(int32_t rssi);

void ereignis(const char* fmt, ...);
void ereignisseAlsJson(JsonArray arr);

// Gemeinsames Datenformat (siehe docs/06-wlan-analyse.md, "Datenformat")
void messungAlsJson(const Messung& m, const char* sonde, const char* bereich, const char* ort,
                    uint16_t intervall, JsonObject o);
bool messungAusJson(JsonObjectConst o, Messung& m);
