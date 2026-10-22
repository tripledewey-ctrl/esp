#include "einstellungen.h"

#include <Preferences.h>
#include <WiFi.h>

#include "config.h"

Einstellungen cfg;

const char* rolleName(Rolle r) {
  switch (r) {
    case Rolle::Mobil:    return "mobil";
    case Rolle::Zentrale: return "zentrale";
    default:              return "sonde";
  }
}

Rolle rolleAus(const String& text) {
  String t = text;
  t.trim();
  t.toLowerCase();
  if (t.startsWith("m")) return Rolle::Mobil;
  if (t.startsWith("z")) return Rolle::Zentrale;
  return Rolle::Sonde;
}

// Erlaubte Zeichen für mDNS: a–z, 0–9, '-'
String hostnameAus(const String& text) {
  String out;
  for (size_t i = 0; i < text.length() && out.length() < 24; i++) {
    char c = tolower(text[i]);
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out += c;
    else if (out.length() && out[out.length() - 1] != '-') out += '-';
  }
  while (out.endsWith("-")) out.remove(out.length() - 1);
  return out;
}

static String standardName(Rolle r) {
  if (r == Rolle::Zentrale) return "wlan-zentrale";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char s[16];
  snprintf(s, sizeof(s), "sonde-%02x%02x%02x", mac[3], mac[4], mac[5]);
  return s;
}

void einstellungenLaden() {
  Preferences p;
  p.begin("wlan-sonde", true);
  cfg.rolle = rolleAus(p.getString("rolle", DEFAULT_ROLLE));
  cfg.name = hostnameAus(p.getString("name", ""));
  if (cfg.name.isEmpty()) cfg.name = standardName(cfg.rolle);
  cfg.bereich = p.getString("bereich", "");
  cfg.ort = p.getString("ort", "");
  cfg.zentrale = p.getString("zentrale", cfg.rolle == Rolle::Sonde ? "http://wlan-zentrale.local" : "");
  cfg.intervall = constrain(p.getUShort("intervall", DEFAULT_INTERVALL), 10, 3600);
  p.end();
}

void einstellungenSpeichern() {
  Preferences p;
  p.begin("wlan-sonde", false);
  p.putString("name", cfg.name);
  p.putString("rolle", rolleName(cfg.rolle));
  p.putString("bereich", cfg.bereich);
  p.putString("ort", cfg.ort);
  p.putString("zentrale", cfg.zentrale);
  p.putUShort("intervall", cfg.intervall);
  p.end();
}
