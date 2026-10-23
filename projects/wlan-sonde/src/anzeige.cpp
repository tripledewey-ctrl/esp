#include "anzeige.h"

#include "config.h"
#include "einstellungen.h"
#include "messung.h"
#include "speicher.h"

// "A", "Gebäude A" -> 0 · "Halle" -> 1 · "B", "Gebäude B" -> 2 · Unbekanntes -> Halle
int bereichSpalte(const char* bereich) {
  String b(bereich);
  b.trim();
  b.toLowerCase();
  if (b.indexOf("hall") >= 0) return 1;
  if (b.startsWith("a") || b.endsWith(" a") || b.endsWith("-a")) return 0;
  if (b.startsWith("b") || b.endsWith(" b") || b.endsWith("-b")) return 2;
  return 1;
}

#ifdef WLAN_MATRIX
#include <FastLED.h>

static constexpr int ANZAHL = MATRIX_W * MATRIX_H;
static CRGB leds[ANZAHL];
static bool uebersicht = true;

bool anzeigeVorhanden() { return true; }
void anzeigeUmschalten() { uebersicht = !uebersicht; }

// Ursprung x=0 links, y=0 unten
static uint16_t XY(int x, int y) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return ANZAHL - 1;
  const int zeile = MATRIX_H - 1 - y;
  if (SERPENTINE && (zeile & 1)) return zeile * MATRIX_W + (MATRIX_W - 1 - x);
  return zeile * MATRIX_W + x;
}

static CRGB farbe(int32_t rssi) {
  if (rssi >= -55) return CRGB(0, 255, 0);      // grün
  if (rssi >= -65) return CRGB(128, 255, 0);    // hellgrün
  if (rssi >= -72) return CRGB(255, 200, 0);    // gelb
  if (rssi >= -80) return CRGB(255, 70, 0);     // orange
  return CRGB(255, 0, 0);                       // rot
}

// -90 dBm -> 1 Balken, -45 dBm -> MATRIX_H Balken
static uint8_t balken(int32_t rssi) {
  if (rssi >= -45) return MATRIX_H;
  if (rssi <= -90) return 1;
  return (uint8_t)(1 + ((rssi + 90) * (MATRIX_H - 1)) / 45);
}

static void anzeigeFarbe(uint8_t r, uint8_t g, uint8_t b);

void anzeigeStart() {
  FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, ANZAHL).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);
  anzeigeFarbe(0, 0, 120);
}

static void anzeigeFarbe(uint8_t r, uint8_t g, uint8_t b) {
  fill_solid(leds, ANZAHL, CRGB(r, g, b));
  FastLED.show();
}

// blau = Portal/Start, gelb = verbinde, rot = Fehler, grün = verbunden
void anzeigeZustand(Zustand z, const char*) {
  switch (z) {
    case Zustand::Verbinde:  anzeigeFarbe(60, 60, 0); break;
    case Zustand::Fehler:    anzeigeFarbe(120, 0, 0); break;
    case Zustand::Verbunden: anzeigeFarbe(0, 60, 0); break;
    default:                 anzeigeFarbe(0, 0, 120); break;
  }
}

static void zeigeLive() {
  FastLED.clear();
  if (!verbindung.up) {                               // rotes X = keine Verbindung
    for (int i = 0; i < MATRIX_H && i < MATRIX_W; i++) {
      leds[XY(i, i)] = CRGB::Red;
      leds[XY(i, MATRIX_H - 1 - i)] = CRGB::Red;
    }
  } else {
    const int32_t r = (int32_t)lroundf(verbindung.rssiMittel);
    const CRGB c = farbe(r);
    for (int y = 0; y < balken(r); y++)
      for (int x = 0; x < MATRIX_W; x++) leds[XY(x, y)] = c;
    leds[XY(verbindung.bssid[5] % MATRIX_W, MATRIX_H - 1)] = CRGB::White;  // Mesh-Knoten
  }
  FastLED.show();
}

static void zeigeFortschritt(uint8_t prozent) {
  FastLED.clear();
  const int zeilen = (prozent * MATRIX_H + 99) / 100;
  for (int y = 0; y < zeilen; y++)
    for (int x = 0; x < MATRIX_W; x++) leds[XY(x, y)] = CRGB(0, 0, 150);
  FastLED.show();
}

// | Gebäude A | Halle | Gebäude B |, je Sonde eine Zeile von oben
static void zeigeUebersicht() {
  FastLED.clear();
  const int wA = MATRIX_W * 3 / 8, wH = MATRIX_W - 2 * wA;
  const int x0[3] = {0, wA, wA + wH};
  const int breite[3] = {wA, wH, wA};
  int zeile[3] = {0, 0, 0};
  const bool blink = (millis() / 500) % 2;

  for (int i = 0; i < anzahlSonden; i++) {
    const int z = bereichSpalte(sonden[i].bereich);
    if (zeile[z] >= MATRIX_H) continue;
    const int y = MATRIX_H - 1 - zeile[z]++;
    const SondeInfo& s = sonden[i];
    CRGB c;
    if (i == 0) c = verbindung.up ? farbe((int32_t)lroundf(verbindung.rssiMittel))
                                  : (blink ? CRGB(CRGB::Red) : CRGB(CRGB::Black));
    else if (sondeVeraltet(i)) c = blink ? CRGB(0, 0, 90) : CRGB(CRGB::Black);   // meldet sich nicht
    else if (!s.letzte.verbunden) c = blink ? CRGB(CRGB::Red) : CRGB(CRGB::Black);
    else c = farbe(s.letzte.rssi);
    for (int x = 0; x < breite[z]; x++) leds[XY(x0[z] + x, y)] = c;
  }
  for (int z = 0; z < 3; z++)                         // leerer Bereich: schwacher Punkt unten
    if (zeile[z] == 0) leds[XY(x0[z], 0)] = CRGB(20, 20, 20);
  FastLED.show();
}

void anzeigeLoop() {
  static uint32_t zuletzt = 0;
  if (millis() - zuletzt < 200) return;
  zuletzt = millis();
  if (cfg.rolle == Rolle::Mobil && messungPhase() != Phase::Ruhe) zeigeFortschritt(messungFortschritt());
  else if (cfg.rolle == Rolle::Zentrale && uebersicht) zeigeUebersicht();
  else zeigeLive();
}

#elif defined(WLAN_OLED)
#include <U8g2lib.h>
#include <WiFi.h>

#include "sender.h"

#ifndef OLED_RST_PIN
#define OLED_RST_PIN U8X8_PIN_NONE
#endif

#if OLED_WIDTH == 72 && OLED_HEIGHT == 40
static U8G2_SSD1306_72X40_ER_F_HW_I2C oled(U8G2_R0, OLED_RST_PIN, OLED_SCL_PIN, OLED_SDA_PIN);
#elif OLED_WIDTH == 128 && OLED_HEIGHT == 64
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, OLED_RST_PIN, OLED_SCL_PIN, OLED_SDA_PIN);
#else
#error "wlan-sonde: OLED nur mit 72x40 oder 128x64 – OLED_WIDTH/OLED_HEIGHT prüfen"
#endif

static constexpr bool KLEIN = OLED_WIDTH < 100;          // 0,42": 12 Zeichen, 4 Zeilen
static constexpr int ZEILEN = OLED_HEIGHT / 10;          // Schrift 6x10
static bool uebersicht = true;
static bool zustandAktiv = true;                         // Startphase: Zustand statt Seiten
static uint8_t seite = 0;
static uint32_t seiteSeit = 0;

bool anzeigeVorhanden() { return true; }
void anzeigeUmschalten() {
  uebersicht = !uebersicht;
  seite = 0;
  seiteSeit = millis();
}

static void text(int zeile, const String& t) { oled.drawUTF8(0, zeile * 10 + 8, t.c_str()); }

// Balken über die volle Breite: -90 dBm leer, -45 dBm voll
static void balken(int y, int h, int32_t rssi) {
  const int w = constrain((rssi + 90) * OLED_WIDTH / 45, 0, OLED_WIDTH);
  oled.drawFrame(0, y, OLED_WIDTH, h);
  oled.drawBox(0, y, w, h);
}

// "AA:BB:CC:DD:EE:FF" -> "DD:EE:FF" bzw. kompakt "DDEEFF"
static String bssidKurz(const uint8_t* b) {
  char t[10];
  if (KLEIN) snprintf(t, sizeof(t), "%02X%02X%02X", b[3], b[4], b[5]);
  else snprintf(t, sizeof(t), "%02X:%02X:%02X", b[3], b[4], b[5]);
  return t;
}

static String ms(uint16_t antwort) {
  if (antwort == KEINE_ANTWORT) return "--";
  return String(antwort / 10.0f, 1) + " ms";
}

void anzeigeStart() {
  oled.begin();
  anzeigeZustand(Zustand::Start);
}

void anzeigeZustand(Zustand z, const char* t) {
  zustandAktiv = z != Zustand::Verbunden;
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  switch (z) {
    case Zustand::Start:     text(0, "wlan-sonde"); text(1, "Start ..."); break;
    case Zustand::Verbinde:  text(0, "Verbinde"); text(1, "mit WLAN ..."); break;
    case Zustand::Fehler:    text(0, "Kein WLAN"); text(1, "Neustart"); break;
    case Zustand::Zuruecksetzen: text(0, "WLAN-Daten"); text(1, "geloescht"); break;
    case Zustand::Verbunden: text(0, "Verbunden"); if (t) text(1, t); break;
    case Zustand::Portal:
      text(0, "Portal offen");
      if (t) text(1, t);
      text(2, "PW " AP_PASS);
      if (ZEILEN > 3) text(3, "192.168.4.1");
      break;
  }
  oled.sendBuffer();
}

// Seite "Signal": große dBm-Zahl, Bewertung, Balken
static void seiteSignal() {
  const int32_t r = (int32_t)lroundf(verbindung.rssiMittel);
  char zahl[8];
  snprintf(zahl, sizeof(zahl), "%ld", (long)r);
  if (KLEIN) {
    text(0, "K" + String(verbindung.kanal) + " " + rssiBewertung(r));
    oled.setFont(u8g2_font_logisoso16_tn);
    oled.drawStr(0, 29, zahl);
    const int x = oled.getStrWidth(zahl) + 3;
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(x, 29, "dBm");
    balken(33, 7, r);
  } else {
    text(0, cfg.name.substring(0, 14) + "  K" + String(verbindung.kanal));
    oled.setFont(u8g2_font_logisoso24_tn);
    oled.drawStr(0, 40, zahl);
    const int x = oled.getStrWidth(zahl) + 6;
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(x, 25, "dBm");
    oled.drawUTF8(x, 39, rssiBewertung(r));
    text(4, "AP " + bssidKurz(verbindung.bssid));
    balken(55, 9, r);
  }
}

// Seite "Mesh": alle Knoten mit gleicher SSID aus der letzten Messung, * = verbunden
static void seiteMesh() {
  if (!hatLetzteMessung()) { text(0, "Mesh"); text(1, "warte auf"); text(2, "Messung"); return; }
  const Messung& m = letzteMessung();
  text(0, (KLEIN ? "Knoten " : "Mesh-Knoten: ") + String(m.nKnoten));
  for (int i = 0; i < m.nKnoten && i + 1 < ZEILEN; i++) {
    const Knoten& k = m.knoten[i];
    const bool meiner = memcmp(k.bssid, verbindung.bssid, 6) == 0;
    char z[28];
    if (KLEIN) snprintf(z, sizeof(z), "%s%4d%s", bssidKurz(k.bssid).c_str(), k.rssi, meiner ? "*" : "");
    else snprintf(z, sizeof(z), "%s K%-2u %4d dBm%s", bssidKurz(k.bssid).c_str(), (unsigned)k.kanal, k.rssi, meiner ? " *" : "");
    text(i + 1, z);
  }
}

// Seite "Stabilität": Router-Antwortzeit, Verlust, Abbrüche, Roaming, Fremdnetze
static void seiteRouter() {
  const Messung* m = hatLetzteMessung() ? &letzteMessung() : nullptr;
  const String antwort = m ? ms(m->antwort) : "--";
  const String verlust = m ? String(m->verlust) + "%" : "--";
  if (KLEIN) {
    text(0, "Router");
    text(1, antwort);
    text(2, "Verlust " + verlust);
    text(3, "Ab " + String(verbindung.abbrueche) + " AP " + String(verbindung.wechsel));
    return;
  }
  text(0, "Router " + antwort + "  " + verlust);
  text(1, "Abbrueche " + String(verbindung.abbrueche) + "  Wechsel " + String(verbindung.wechsel));
  if (m) {
    text(2, "Min/Max " + String(m->rssiMin) + " / " + String(m->rssiMax) + " dBm");
    if (m->kanal >= 1 && m->kanal <= KANAELE) {
      const Fremd& f = m->fremd[m->kanal - 1];
      String z = "Fremd K" + String(m->kanal) + ": " + String(f.anzahl);
      if (f.anzahl) z += " (" + String(f.max) + " dBm)";
      text(3, z);
    }
    if (m->t) text(4, "Messung vor " + String(jetztUnix() - m->t) + " s");
  }
}

// Seite "Gerät": Name, Rolle, Bereich, IP, Puffer bzw. Sender
static void seiteGeraet() {
  String ip = WiFi.localIP().toString();
  if (KLEIN && ip.length() > 12) ip = ip.substring(ip.length() - 12);
  text(0, cfg.name.substring(0, KLEIN ? 12 : 21));
  text(1, String(rolleName(cfg.rolle)).substring(0, KLEIN ? 6 : 10) + " " + cfg.bereich);
  text(2, ip);
  String info;
  switch (cfg.rolle) {
    case Rolle::Sonde:    info = (KLEIN ? "offen " : "nicht gesendet: ") + String(senderOffen()); break;
    case Rolle::Mobil:    info = "Punkte " + String(punkteAnzahl()); break;
    case Rolle::Zentrale: info = "Puffer " + String(puffer.anzahl()); break;
  }
  if (ZEILEN > 3) text(3, info);
}

// Zentrale: eine Zeile je Sonde, ! = meldet sich nicht, x = offline
static int uebersichtSeiten() { return max(1, (anzahlSonden + ZEILEN - 2) / (ZEILEN - 1)); }
static void seiteUebersicht(int nr) {
  const int proSeite = ZEILEN - 1;
  String kopf = "Sonden " + String(anzahlSonden);
  if (uebersichtSeiten() > 1) kopf += "  " + String(nr + 1) + "/" + String(uebersichtSeiten());
  text(0, kopf);
  for (int z = 0; z < proSeite; z++) {
    const int i = nr * proSeite + z;
    if (i >= anzahlSonden) break;
    const SondeInfo& s = sonden[i];
    String wert;
    if (i == 0 && verbindung.up) wert = String((int32_t)lroundf(verbindung.rssiMittel));
    else if (i == 0) wert = "x";
    else if (sondeVeraltet(i)) wert = "!";
    else if (!s.letzte.verbunden) wert = "x";
    else wert = String(s.letzte.rssi);
    char zeile[28];
    if (KLEIN) snprintf(zeile, sizeof(zeile), "%-8.8s%4s", i == 0 ? cfg.name.c_str() : s.name, wert.c_str());
    else snprintf(zeile, sizeof(zeile), "%-11.11s %-5.5s %4s", i == 0 ? cfg.name.c_str() : s.name,
                  i == 0 ? cfg.bereich.c_str() : s.bereich, wert.c_str());
    text(z + 1, zeile);
  }
}

static void seiteFortschritt() {
  text(0, "Messpunkt P" + String(punkteAnzahl() + 1));
  text(1, String(messungFortschritt()) + " %");
  const int w = messungFortschritt() * OLED_WIDTH / 100;
  oled.drawFrame(0, OLED_HEIGHT - 10, OLED_WIDTH, 10);
  oled.drawBox(0, OLED_HEIGHT - 10, w, 10);
}

static void seiteOffline() {
  text(0, "WLAN weg");
  text(1, "Abbrueche " + String(verbindung.abbrueche));
  text(2, "verbinde neu");
  if (ZEILEN > 3 && cfg.rolle == Rolle::Sonde) text(3, "offen " + String(senderOffen()));
}

void anzeigeLoop() {
  static uint32_t zuletzt = 0;
  if (zustandAktiv || millis() - zuletzt < 500) return;
  zuletzt = millis();

  // Seitenfolge je Rolle; Zentrale in der Übersicht: alle Sonden-Seiten, dann Signal
  const bool zentraleListe = cfg.rolle == Rolle::Zentrale && uebersicht;
  const int anzahl = zentraleListe ? uebersichtSeiten() + 1 : (cfg.rolle == Rolle::Mobil ? 2 : 4);
  if (millis() - seiteSeit >= OLED_SEITE_MS) {
    seite = (seite + 1) % anzahl;
    seiteSeit = millis();
  }
  if (seite >= anzahl) seite = 0;

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  if (cfg.rolle == Rolle::Mobil && messungPhase() != Phase::Ruhe) seiteFortschritt();
  else if (!verbindung.up) seiteOffline();
  else if (zentraleListe) {
    if (seite < uebersichtSeiten()) seiteUebersicht(seite);
    else seiteSignal();
  } else if (cfg.rolle == Rolle::Mobil) {
    if (seite == 0) seiteSignal();
    else seiteGeraet();
  } else {
    switch (seite) {
      case 0: seiteSignal(); break;
      case 1: seiteMesh(); break;
      case 2: seiteRouter(); break;
      default: seiteGeraet(); break;
    }
  }
  oled.sendBuffer();
}

#else  // ohne Anzeige

bool anzeigeVorhanden() { return false; }
void anzeigeStart() {}
void anzeigeZustand(Zustand, const char*) {}
void anzeigeLoop() {}
void anzeigeUmschalten() {}

#endif
