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

void anzeigeStart() {
  FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, ANZAHL).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);
  anzeigeFarbe(0, 0, 120);
}

void anzeigeFarbe(uint8_t r, uint8_t g, uint8_t b) {
  fill_solid(leds, ANZAHL, CRGB(r, g, b));
  FastLED.show();
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

#else  // ohne LED-Matrix

bool anzeigeVorhanden() { return false; }
void anzeigeStart() {}
void anzeigeFarbe(uint8_t, uint8_t, uint8_t) {}
void anzeigeLoop() {}
void anzeigeUmschalten() {}

#endif
