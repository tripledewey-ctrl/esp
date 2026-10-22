// Speicher: Ringpuffer für Messungen, Sondentabelle (Zentrale),
// Messpunkte der Begehung (LittleFS) und Namen der Mesh-Knoten (LittleFS).
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

#include "messung.h"

static constexpr int MAX_SONDEN = 24;

struct SondeInfo {
  char name[24] = "";
  char bereich[16] = "";
  char ort[40] = "";
  uint16_t intervall = 0;
  uint32_t zuletzt = 0;          // millis() der letzten Meldung
  IPAddress ip;
  bool hatMessung = false;
  Messung letzte;
};

class Ringpuffer {
 public:
  bool begin(size_t kapazitaet);
  void add(const Messung& m);
  size_t anzahl() const { return anzahl_; }
  size_t kapazitaet() const { return kap_; }
  uint32_t gesamt() const { return gesamt_; }        // je hinzugefügte Messungen
  const Messung& alt(size_t i) const;                // 0 = älteste vorhandene
  const Messung* nachNummer(uint32_t nr) const;      // laufende Nummer, nullptr = überschrieben

 private:
  Messung* daten_ = nullptr;
  size_t kap_ = 0, kopf_ = 0, anzahl_ = 0;
  uint32_t gesamt_ = 0;
};

extern Ringpuffer puffer;
extern SondeInfo sonden[MAX_SONDEN];   // [0] = dieses Gerät
extern int anzahlSonden;

void speicherStart(size_t kapazitaet);
int sondeFinden(const char* name, bool anlegen);
void sondeAlsJson(int idx, JsonObject o);
void messungZeile(const Messung& m, String& zeile);   // eine Zeile im Exportformat (NDJSON)
bool sondeVeraltet(int idx);

// Messpunkte (Rolle mobil), Datei /punkte.jsonl
bool punktSpeichern(const Messung& m);
bool punkteLoeschen(bool nurLetzten);
size_t punkteAnzahl();
const char* punkteDatei();

// Namen der Mesh-Knoten (Rolle zentrale), Datei /knoten.json
void knotenNamenLaden();
String knotenName(const String& bssid);
bool knotenNameSetzen(const String& bssid, const String& name);
