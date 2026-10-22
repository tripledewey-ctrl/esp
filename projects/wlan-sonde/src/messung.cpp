#include "messung.h"

#include <WiFi.h>
#include <stdarg.h>
#include <time.h>

#include "config.h"

Verbindung verbindung;

static constexpr uint32_t PRUEF_MS = 500;             // Verbindung prüfen
static constexpr uint32_t SAMMEL_MS = 300;            // Takt beim Messpunkt
static constexpr uint32_t NEU_VERBINDEN_MS = 30000;   // eigener Reconnect-Versuch
static constexpr uint32_t NEUSTART_MS = 900000;       // Neustart nach 15 min offline
static constexpr float GLAETTUNG = 0.3f;              // Gewicht neuer RSSI-Werte
static constexpr uint8_t PUNKT_WERTE = 10;            // RSSI-Werte je Messpunkt
static constexpr uint8_t ROUTER_ANFRAGEN = 5;
static constexpr int32_t ROUTER_TIMEOUT_MS = 400;

// ------------------------------- Hilfsfunktionen ----------------------------
uint32_t jetztUnix() {
  const time_t n = time(nullptr);
  return n > 1700000000 ? (uint32_t)n : 0;
}

String bssidText(const uint8_t* b) {
  char s[18];
  snprintf(s, sizeof(s), "%02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  return s;
}

bool bssidAusText(const char* text, uint8_t* out) {
  unsigned v[6];
  if (!text || sscanf(text, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6)
    return false;
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)v[i];
  return true;
}

const char* rssiBewertung(int32_t rssi) {
  if (rssi >= -55) return "sehr gut";
  if (rssi >= -65) return "gut";
  if (rssi >= -72) return "brauchbar";
  if (rssi >= -80) return "schwach";
  return "kritisch";
}

// Häufige Abbruchgründe (esp_wifi_types.h) in Klartext
static const char* grundText(uint8_t r) {
  switch (r) {
    case 2:   return "Authentifizierung abgelaufen";
    case 3:   return "vom AP abgemeldet";
    case 4:   return "Inaktivität";
    case 8:   return "Station hat Verbindung beendet";
    case 15:  return "4-Way-Handshake Timeout";
    case 200: return "Beacon-Timeout (Signal verloren)";
    case 201: return "kein AP mit dieser SSID gefunden";
    case 202: return "Authentifizierung fehlgeschlagen";
    case 203: return "Assoziierung fehlgeschlagen";
    case 204: return "Handshake Timeout";
    default:  return "sonstiger Grund";
  }
}

// --------------------------------- Ereignisse -------------------------------
struct Ereignis {
  uint32_t uptime;
  uint32_t zeit;
  char text[80];
};
static constexpr int MAX_EREIGNISSE = 20;
static Ereignis ereignisse[MAX_EREIGNISSE];
static int ereignisKopf = 0, ereignisAnzahl = 0;

void ereignis(const char* fmt, ...) {
  Ereignis& e = ereignisse[ereignisKopf];
  e.uptime = millis() / 1000;
  e.zeit = jetztUnix();
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(e.text, sizeof(e.text), fmt, ap);
  va_end(ap);
  ereignisKopf = (ereignisKopf + 1) % MAX_EREIGNISSE;
  if (ereignisAnzahl < MAX_EREIGNISSE) ereignisAnzahl++;
  Serial.printf("[%6lus] %s\n", (unsigned long)e.uptime, e.text);
}

void ereignisseAlsJson(JsonArray arr) {             // neueste zuerst
  for (int k = 0; k < ereignisAnzahl; k++) {
    const Ereignis& e = ereignisse[(ereignisKopf - 1 - k + MAX_EREIGNISSE) % MAX_EREIGNISSE];
    JsonObject o = arr.add<JsonObject>();
    o["uptime"] = e.uptime;
    o["zeit"] = e.zeit;
    o["text"] = e.text;
  }
}

// ---------------------- RSSI-Statistik im Messzeitraum ----------------------
static int32_t rssiSumme = 0;
static uint16_t rssiAnzahl = 0;
static int8_t rssiMin = 0, rssiMax = 0;

static void statistikReset() {
  rssiSumme = 0;
  rssiAnzahl = 0;
  rssiMin = 0;
  rssiMax = -127;
}

static void statistikAdd(int32_t r) {
  if (rssiAnzahl == 0 || r < rssiMin) rssiMin = r;
  if (rssiAnzahl == 0 || r > rssiMax) rssiMax = r;
  rssiSumme += r;
  if (rssiAnzahl < 0xFFFF) rssiAnzahl++;
}

// ---------------------------- Verbindungsüberwachung ------------------------
static volatile uint8_t letzterGrund = 0;            // aus dem WLAN-Task
static uint32_t offlineSeit = 0, letzterVersuch = 0;
static Phase phase = Phase::Ruhe;

static void wlanEreignis(WiFiEvent_t event, WiFiEventInfo_t info) {
  // läuft im WLAN-Task: nur merken, ausgewertet wird in verbindungLoop()
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) letzterGrund = info.wifi_sta_disconnected.reason;
}

void verbindungStart() {
  WiFi.onEvent(wlanEreignis);
  WiFi.setAutoReconnect(true);
  statistikReset();
}

void verbindungLoop() {
  static uint32_t letztePruefung = 0;
  const uint32_t takt = phase == Phase::Sammeln ? SAMMEL_MS : PRUEF_MS;
  if (millis() - letztePruefung < takt) return;
  letztePruefung = millis();

  const bool up = WiFi.status() == WL_CONNECTED;
  if (up != verbindung.up) {
    verbindung.up = up;
    if (up) {
      verbindung.rssiMittel = WiFi.RSSI();
      ereignis("Verbunden: %s, Kanal %d, %d dBm", bssidText(WiFi.BSSID()).c_str(),
               (int)WiFi.channel(), (int)WiFi.RSSI());
    } else {
      verbindung.abbrueche++;
      offlineSeit = letzterVersuch = millis();
      ereignis("Verbindung verloren (%u: %s)", letzterGrund, grundText(letzterGrund));
    }
  }

  if (up) {
    const uint8_t* b = WiFi.BSSID();
    if (verbindung.hatBssid && memcmp(b, verbindung.bssid, 6) != 0) {
      verbindung.wechsel++;
      ereignis("AP-Wechsel: %s -> %s", bssidText(verbindung.bssid).c_str(), bssidText(b).c_str());
    }
    memcpy(verbindung.bssid, b, 6);
    verbindung.hatBssid = true;
    verbindung.kanal = WiFi.channel();
    verbindung.rssi = WiFi.RSSI();
    verbindung.rssiMittel += GLAETTUNG * (verbindung.rssi - verbindung.rssiMittel);
    statistikAdd(verbindung.rssi);
    return;
  }

  // Offline: der ESP32 verbindet sich selbst neu (autoReconnect). Hilft das
  // nicht, alle 30 s selbst anstoßen und nach 15 min neu starten.
  if (millis() - letzterVersuch > NEU_VERBINDEN_MS) {
    letzterVersuch = millis();
    ereignis("Neuer Verbindungsversuch");
    WiFi.reconnect();
  }
  if (millis() - offlineSeit > NEUSTART_MS) {
    ereignis("Seit 15 min offline – Neustart");
    delay(200);
    ESP.restart();
  }
}

// --------------------------------- Messablauf -------------------------------
static Messung aktuell;
static Messung letzte;
static bool hatLetzte = false, neueMessung = false;
static uint32_t phaseStart = 0;
static uint8_t routerVersuche = 0, routerErfolge = 0;
static uint32_t routerSumme = 0;                     // in 0,1 ms

Phase messungPhase() { return phase; }
bool hatLetzteMessung() { return hatLetzte; }
const Messung& letzteMessung() { return letzte; }

uint8_t messungFortschritt() {
  switch (phase) {
    case Phase::Sammeln: return min<uint32_t>(50, rssiAnzahl * 50 / PUNKT_WERTE);
    case Phase::Scan:    return 50 + min<uint32_t>(30, (millis() - phaseStart) * 30 / 4000);
    case Phase::Router:  return 80 + routerVersuche * 20 / ROUTER_ANFRAGEN;
    default:             return 0;
  }
}

static void abschliessen() {
  aktuell.t = jetztUnix();
  aktuell.uptime = millis() / 1000;
  aktuell.verbunden = verbindung.up;
  if (verbindung.up) {
    memcpy(aktuell.bssid, verbindung.bssid, 6);
    aktuell.kanal = verbindung.kanal;
  }
  if (rssiAnzahl > 0) {
    aktuell.rssi = (int8_t)(rssiSumme / (int32_t)rssiAnzahl);
    aktuell.rssiMin = rssiMin;
    aktuell.rssiMax = rssiMax;
  } else if (verbindung.up) {
    aktuell.rssi = aktuell.rssiMin = aktuell.rssiMax = (int8_t)verbindung.rssi;
  }
  aktuell.abbrueche = (uint16_t)min<uint32_t>(verbindung.abbrueche, 0xFFFF);
  aktuell.wechsel = (uint16_t)min<uint32_t>(verbindung.wechsel, 0xFFFF);
  if (routerVersuche == 0) {
    aktuell.antwort = KEINE_ANTWORT;
    aktuell.verlust = 100;
  } else {
    aktuell.antwort = routerErfolge ? (uint16_t)min<uint32_t>(routerSumme / routerErfolge, 0xFFFE)
                                    : KEINE_ANTWORT;
    aktuell.verlust = (uint8_t)((routerVersuche - routerErfolge) * 100 / routerVersuche);
  }

  statistikReset();
  letzte = aktuell;
  hatLetzte = neueMessung = true;
  phase = Phase::Ruhe;
}

static void scanStarten() {
  if (!verbindung.up) {                             // offline: Messung ohne Scan festhalten
    abschliessen();
    return;
  }
  WiFi.scanNetworks(true, false);                   // asynchron, ohne versteckte Netze
  phase = Phase::Scan;
  phaseStart = millis();
}

static void scanAuswerten(int n) {
  const String eigeneSsid = WiFi.SSID();
  aktuell.nKnoten = 0;
  for (int i = 0; i < n; i++) {
    const int ch = WiFi.channel(i);
    const int32_t r = WiFi.RSSI(i);
    if (WiFi.SSID(i) == eigeneSsid) {
      // nach Stärke einsortieren, schwächste fallen bei mehr als MAX_KNOTEN heraus
      int pos = aktuell.nKnoten;
      while (pos > 0 && aktuell.knoten[pos - 1].rssi < r) pos--;
      if (pos >= MAX_KNOTEN) continue;
      const int ende = min(aktuell.nKnoten, (uint8_t)(MAX_KNOTEN - 1));
      for (int j = ende; j > pos; j--) aktuell.knoten[j] = aktuell.knoten[j - 1];
      memcpy(aktuell.knoten[pos].bssid, WiFi.BSSID(i), 6);
      aktuell.knoten[pos].rssi = (int8_t)r;
      aktuell.knoten[pos].kanal = (uint8_t)ch;
      if (aktuell.nKnoten < MAX_KNOTEN) aktuell.nKnoten++;
    } else if (ch >= 1 && ch <= KANAELE) {
      Fremd& f = aktuell.fremd[ch - 1];
      if (f.anzahl == 0 || r > f.max) f.max = (int8_t)r;
      if (f.anzahl < 255) f.anzahl++;
    }
  }
  WiFi.scanDelete();
}

bool messungStarten(const char* punkt, const char* bereich) {
  if (phase != Phase::Ruhe) return false;
  aktuell = Messung();
  routerVersuche = routerErfolge = 0;
  routerSumme = 0;
  if (punkt) {                                      // Messpunkt: erst RSSI-Werte sammeln
    aktuell.istPunkt = true;
    strlcpy(aktuell.punkt, punkt, sizeof(aktuell.punkt));
    strlcpy(aktuell.punktBereich, bereich ? bereich : "", sizeof(aktuell.punktBereich));
    statistikReset();
    phase = Phase::Sammeln;
    phaseStart = millis();
    return true;
  }
  scanStarten();                                    // laufende Messung: Statistik seit der letzten
  return true;
}

void messungLoop() {
  switch (phase) {
    case Phase::Ruhe:
      return;

    case Phase::Sammeln:
      if (rssiAnzahl >= PUNKT_WERTE || millis() - phaseStart > 6000) scanStarten();
      return;

    case Phase::Scan: {
      const int n = WiFi.scanComplete();
      if (n == WIFI_SCAN_RUNNING) {
        if (millis() - phaseStart < 15000) return;
        WiFi.scanDelete();                          // hängt: ohne Scanergebnis weiter
      } else if (n >= 0) {
        scanAuswerten(n);
      }
      phase = Phase::Router;
      phaseStart = millis();
      return;
    }

    case Phase::Router: {
      if (routerVersuche >= ROUTER_ANFRAGEN || !verbindung.up) {
        abschliessen();
        return;
      }
      // Antwortzeit = Dauer des TCP-Verbindungsaufbaus zum Router (ähnlich einem Ping)
      WiFiClient c;
      const uint32_t t0 = micros();
      const bool ok = c.connect(WiFi.gatewayIP(), ROUTER_PORT, ROUTER_TIMEOUT_MS);
      const uint32_t dauer = micros() - t0;
      c.stop();
      routerVersuche++;
      if (ok) {
        routerErfolge++;
        routerSumme += dauer / 100;
      }
      return;
    }
  }
}

bool messungAbholen(Messung& out) {
  if (!neueMessung) return false;
  neueMessung = false;
  out = letzte;
  return true;
}

// ------------------------------ Datenformat (JSON) --------------------------
void messungAlsJson(const Messung& m, const char* sonde, const char* bereich, const char* ort,
                    uint16_t intervall, JsonObject o) {
  o["typ"] = m.istPunkt ? "punkt" : "messung";
  o["sonde"] = sonde;
  o["bereich"] = m.istPunkt ? m.punktBereich : bereich;
  o["ort"] = ort;
  if (m.istPunkt) o["punkt"] = m.punkt;
  o["t"] = m.t;
  o["uptime"] = m.uptime;
  o["intervall"] = intervall;
  o["verbunden"] = m.verbunden;
  if (m.verbunden) {
    o["bssid"] = bssidText(m.bssid);
    o["kanal"] = m.kanal;
    o["rssi"] = m.rssi;
    o["rssi_min"] = m.rssiMin;
    o["rssi_max"] = m.rssiMax;
  }
  o["abbrueche"] = m.abbrueche;
  o["wechsel"] = m.wechsel;
  if (m.antwort == KEINE_ANTWORT) o["antwort_ms"] = nullptr;
  else o["antwort_ms"] = serialized(String(m.antwort / 10.0f, 1));
  o["verlust"] = m.verlust;

  JsonArray k = o["knoten"].to<JsonArray>();
  for (int i = 0; i < m.nKnoten; i++) {
    JsonObject n = k.add<JsonObject>();
    n["bssid"] = bssidText(m.knoten[i].bssid);
    n["rssi"] = m.knoten[i].rssi;
    n["kanal"] = m.knoten[i].kanal;
  }
  JsonArray f = o["fremd"].to<JsonArray>();
  for (int ch = 1; ch <= KANAELE; ch++) {
    if (!m.fremd[ch - 1].anzahl) continue;
    JsonObject n = f.add<JsonObject>();
    n["kanal"] = ch;
    n["anzahl"] = m.fremd[ch - 1].anzahl;
    n["max"] = m.fremd[ch - 1].max;
  }
}

bool messungAusJson(JsonObjectConst o, Messung& m) {
  if (o.isNull()) return false;
  m = Messung();
  const char* typ = o["typ"] | "messung";
  m.istPunkt = strcmp(typ, "punkt") == 0;
  m.t = o["t"] | (uint32_t)0;
  m.uptime = o["uptime"] | (uint32_t)0;
  m.verbunden = o["verbunden"] | false;
  bssidAusText(o["bssid"] | "", m.bssid);
  m.kanal = o["kanal"] | 0;
  m.rssi = o["rssi"] | -127;
  m.rssiMin = o["rssi_min"] | (int)m.rssi;
  m.rssiMax = o["rssi_max"] | (int)m.rssi;
  m.abbrueche = o["abbrueche"] | 0;
  m.wechsel = o["wechsel"] | 0;
  JsonVariantConst antwort = o["antwort_ms"];
  m.antwort = antwort.isNull() ? KEINE_ANTWORT : (uint16_t)lroundf(antwort.as<float>() * 10);
  m.verlust = o["verlust"] | 0;

  for (JsonVariantConst v : o["knoten"].as<JsonArrayConst>()) {
    if (m.nKnoten >= MAX_KNOTEN) break;
    Knoten& k = m.knoten[m.nKnoten];
    if (!bssidAusText(v["bssid"] | "", k.bssid)) continue;
    k.rssi = v["rssi"] | -127;
    k.kanal = v["kanal"] | 0;
    m.nKnoten++;
  }
  for (JsonVariantConst v : o["fremd"].as<JsonArrayConst>()) {
    const int ch = v["kanal"] | 0;
    if (ch < 1 || ch > KANAELE) continue;
    m.fremd[ch - 1].anzahl = v["anzahl"] | 0;
    m.fremd[ch - 1].max = v["max"] | -127;
  }
  if (m.istPunkt) {
    strlcpy(m.punkt, o["punkt"] | "", sizeof(m.punkt));
    strlcpy(m.punktBereich, o["bereich"] | "", sizeof(m.punktBereich));
  }
  return true;
}
