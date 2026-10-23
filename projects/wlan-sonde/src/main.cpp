/******************************************************************************
 * wlan-sonde – Messgerät des WLAN-Analyse-Moduls
 * -----------------------------------------------------------------------------
 * Misst die WLAN-Abdeckung eines Mesh-Netzes (z. B. FRITZ!Box + Repeater) in
 * mehreren Gebäuden. Ein Gerät, drei Rollen:
 *
 *   sonde    – fest montiert, misst alle N Sekunden: Signalstärke, verbundener
 *              Mesh-Knoten, alle Knoten mit gleicher SSID, Fremdnetze je Kanal,
 *              Antwortzeit des Routers. Meldet an die Zentrale, sendet nach.
 *   mobil    – Begehung: Messpunkt per Webseite oder BOOT-Taste erfassen,
 *              Speicherung im Flash, Export als JSON Lines.
 *   zentrale – sammelt die Meldungen aller Sonden, misst selbst mit, zeigt
 *              Übersicht im Browser und auf der LED-Matrix (A | Halle | B).
 *
 * Auswertung: tools/wlan-analyse (Bericht mit Lageplan und Empfehlungen).
 * Anleitung:  docs/06-wlan-analyse.md
 *
 * Taster (BOOT):  kurz = Sonde: jetzt messen · Mobil: Messpunkt · Zentrale:
 *                 Anzeige umschalten   ·   lang (> 3 s) = WLAN zurücksetzen
 *****************************************************************************/

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "anzeige.h"
#include "config.h"
#include "einstellungen.h"
#include "messung.h"
#include "sender.h"
#include "speicher.h"
#include "web.h"

static uint32_t letzteMessung_ms = 0;

void wlanZuruecksetzen() {
  ereignis("WLAN-Zugangsdaten werden gelöscht");
  anzeigeZustand(Zustand::Zuruecksetzen);
  WiFi.persistent(true);
  WiFiManager wm;
  wm.resetSettings();
  delay(500);
  ESP.restart();
}

static void tasterLoop() {
  static bool gedrueckt = false;
  static uint32_t seit = 0;
  const bool unten = digitalRead(BUTTON_PIN) == LOW;
  if (unten && !gedrueckt) {
    gedrueckt = true;
    seit = millis();
  }
  if (unten && gedrueckt && millis() - seit > 3000) wlanZuruecksetzen();
  if (!unten && gedrueckt) {
    gedrueckt = false;
    if (millis() - seit > 1000) return;
    switch (cfg.rolle) {
      case Rolle::Mobil: {
        const String id = "P" + String(punkteAnzahl() + 1);
        if (messungStarten(id.c_str(), cfg.bereich.c_str())) ereignis("Messpunkt %s gestartet", id.c_str());
        break;
      }
      case Rolle::Zentrale: anzeigeUmschalten(); break;
      default: messungStarten(); break;
    }
  }
}

// Fertige Messungen ablegen: Messpunkt -> Flash, sonst -> Puffer
static void messungVerarbeiten() {
  Messung m;
  if (!messungAbholen(m)) return;
  if (m.istPunkt) {
    const bool ok = punktSpeichern(m);
    ereignis("Messpunkt %s (%s): %d dBm, %d Knoten%s", m.punkt, m.punktBereich,
             m.verbunden ? m.rssi : -127, m.nKnoten, ok ? "" : " – SPEICHERN FEHLGESCHLAGEN");
    return;
  }
  m.sonde = 0;
  puffer.add(m);
  sonden[0].letzte = m;
  sonden[0].hatMessung = true;
  sonden[0].zuletzt = millis();
}

void setup() {
  Serial.begin(MONITOR_SPEED);
  delay(300);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  anzeigeStart();

  WiFi.mode(WIFI_STA);
  einstellungenLaden();
  Serial.printf("\nwlan-sonde \"%s\", Rolle %s, Bereich \"%s\"\n", cfg.name.c_str(),
                rolleName(cfg.rolle), cfg.bereich.c_str());

  // Portal: WLAN + Grundeinstellungen beim ersten Start
  char intervall[8];
  snprintf(intervall, sizeof(intervall), "%u", cfg.intervall);
  WiFiManagerParameter pName("name", "Gerätename (Hostname)", cfg.name.c_str(), 24);
  WiFiManagerParameter pRolle("rolle", "Rolle: sonde / mobil / zentrale", rolleName(cfg.rolle), 10);
  WiFiManagerParameter pBereich("bereich", "Bereich: A / Halle / B", cfg.bereich.c_str(), 15);
  WiFiManagerParameter pOrt("ort", "Ort (Beschreibung)", cfg.ort.c_str(), 39);
  WiFiManagerParameter pZentrale("zentrale", "Zentrale (nur Sonde)", cfg.zentrale.c_str(), 64);
  WiFiManagerParameter pIntervall("intervall", "Messintervall in s", intervall, 5);

  WiFiManager wm;
  wm.addParameter(&pName);
  wm.addParameter(&pRolle);
  wm.addParameter(&pBereich);
  wm.addParameter(&pOrt);
  wm.addParameter(&pZentrale);
  wm.addParameter(&pIntervall);
  wm.setHostname(cfg.name.c_str());
  wm.setConfigPortalTimeout(300);
  wm.setAPCallback([](WiFiManager* w) { anzeigeZustand(Zustand::Portal, w->getConfigPortalSSID().c_str()); });

  String apName = "WLAN-Sonde-" + WiFi.macAddress().substring(12);
  apName.replace(":", "");
  anzeigeZustand(Zustand::Verbinde);                  // Matrix: gelb
  if (!wm.autoConnect(apName.c_str(), AP_PASS)) {
    Serial.println("Keine Verbindung – Neustart");
    anzeigeZustand(Zustand::Fehler);
    delay(2000);
    ESP.restart();
  }

  // Werte aus dem Portal übernehmen (ohne Portal stehen dort die alten Werte)
  const String neuerName = hostnameAus(pName.getValue());
  if (neuerName.length()) cfg.name = neuerName;
  cfg.rolle = rolleAus(pRolle.getValue());
  cfg.bereich = pBereich.getValue();
  cfg.ort = pOrt.getValue();
  cfg.zentrale = pZentrale.getValue();
  cfg.intervall = constrain(atoi(pIntervall.getValue()), 10, 3600);
  einstellungenSpeichern();

  verbindungStart();
  configTzTime(TZ_INFO, NTP_SERVER, "pool.ntp.org");
  if (MDNS.begin(cfg.name.c_str())) MDNS.addService("http", "tcp", 80);

  size_t kapazitaet = PUFFER_SONDE;
  if (cfg.rolle == Rolle::Zentrale) kapazitaet = psramFound() ? PUFFER_ZENTRALE_PSRAM : PUFFER_ZENTRALE;
  if (cfg.rolle == Rolle::Mobil) kapazitaet = 20;      // Messpunkte liegen im Flash
  speicherStart(kapazitaet);
  if (cfg.rolle == Rolle::Zentrale) knotenNamenLaden();
  webStart();

  ereignis("Gestartet als %s, http://%s.local/ (%s)", rolleName(cfg.rolle), cfg.name.c_str(),
           WiFi.localIP().toString().c_str());
  anzeigeZustand(Zustand::Verbunden, WiFi.localIP().toString().c_str());
  delay(300);
  letzteMessung_ms = millis() - cfg.intervall * 1000UL + 5000;   // erste Messung nach 5 s
}

void loop() {
  tasterLoop();
  verbindungLoop();

  if (cfg.rolle != Rolle::Mobil && messungPhase() == Phase::Ruhe &&
      millis() - letzteMessung_ms >= cfg.intervall * 1000UL) {
    letzteMessung_ms = millis();
    messungStarten();
  }
  messungLoop();
  messungVerarbeiten();

  senderLoop();
  webLoop();
  anzeigeLoop();
}
