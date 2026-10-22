// Laufzeit-Einstellungen (NVS/Preferences): Name, Rolle, Bereich, Ort, Zentrale.
#pragma once
#include <Arduino.h>

enum class Rolle : uint8_t { Sonde, Mobil, Zentrale };

struct Einstellungen {
  String name;        // Gerätename = mDNS-Hostname, z. B. "sonde-halle"
  Rolle rolle;
  String bereich;     // "A", "Halle" oder "B" (frei wählbar)
  String ort;         // Beschreibung, z. B. "Halle Mitte, Säule 4, 3 m hoch"
  String zentrale;    // URL der Zentrale, z. B. http://wlan-zentrale.local
  uint16_t intervall; // Sekunden zwischen zwei Messungen
};

extern Einstellungen cfg;

void einstellungenLaden();
void einstellungenSpeichern();
const char* rolleName(Rolle r);
Rolle rolleAus(const String& text);
String hostnameAus(const String& text);
