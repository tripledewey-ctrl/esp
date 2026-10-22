#include "sender.h"

#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "einstellungen.h"
#include "messung.h"
#include "speicher.h"

static constexpr uint32_t PAUSE_NACH_FEHLER = 30000;
static constexpr uint32_t ADRESSE_GUELTIG = 600000;  // mDNS-Auflösung 10 min merken

static uint32_t naechsteNr = 0;          // laufende Nummer der nächsten zu sendenden Messung
static uint32_t gesendet = 0, fehler = 0;
static uint32_t gesperrtBis = 0;
static String basisUrl;                  // aufgelöste Adresse, z. B. http://192.168.178.40
static uint32_t basisZeit = 0;
static String letzterFehler;

// "http://wlan-zentrale.local:8080/" -> "http://192.168.178.40:8080"
static bool adresseAufloesen() {
  if (basisUrl.length() && millis() - basisZeit < ADRESSE_GUELTIG) return true;
  String url = cfg.zentrale;
  url.trim();
  if (url.startsWith("http://")) url.remove(0, 7);
  while (url.endsWith("/")) url.remove(url.length() - 1);
  const int slash = url.indexOf('/');
  if (slash >= 0) url.remove(slash);
  String host = url, port;
  const int doppelpunkt = url.indexOf(':');
  if (doppelpunkt >= 0) {
    host = url.substring(0, doppelpunkt);
    port = url.substring(doppelpunkt);
  }
  if (host.isEmpty()) return false;

  if (host.endsWith(".local")) {         // mDNS: der ESP32 löst .local nicht selbst auf
    const IPAddress ip = MDNS.queryHost(host.substring(0, host.length() - 6), 2000);
    if ((uint32_t)ip == 0) {
      letzterFehler = host + " nicht gefunden (mDNS)";
      return false;
    }
    host = ip.toString();
  }
  basisUrl = "http://" + host + port;
  basisZeit = millis();
  return true;
}

void senderLoop() {
  if (cfg.rolle != Rolle::Sonde || cfg.zentrale.isEmpty() || !verbindung.up) return;
  if (messungPhase() != Phase::Ruhe) return;             // nicht während Scan/Messung
  if ((int32_t)(millis() - gesperrtBis) < 0) return;
  if (naechsteNr >= puffer.gesamt()) return;

  const uint32_t aelteste = puffer.gesamt() - puffer.anzahl();
  if (naechsteNr < aelteste) naechsteNr = aelteste;      // Überschriebenes überspringen
  const Messung* m = puffer.nachNummer(naechsteNr);
  if (!m) return;

  if (!adresseAufloesen()) {
    fehler++;
    gesperrtBis = millis() + PAUSE_NACH_FEHLER;
    return;
  }

  String body;
  messungZeile(*m, body);
  HTTPClient http;
  http.setConnectTimeout(2000);
  http.setTimeout(3000);
  int code = -1;
  if (http.begin(basisUrl + "/api/sonde")) {
    http.addHeader("Content-Type", "application/json");
    code = http.POST(body);
    http.end();
  }
  if (code >= 200 && code < 300) {
    naechsteNr++;
    gesendet++;
    letzterFehler = "";
    return;
  }
  fehler++;
  if (code < 0) letzterFehler = "keine Verbindung zu " + basisUrl;
  else letzterFehler = "HTTP " + String(code);
  if (fehler % 10 == 1) ereignis("Senden an Zentrale fehlgeschlagen: %s", letzterFehler.c_str());
  basisUrl = "";                                          // Adresse neu auflösen
  gesperrtBis = millis() + PAUSE_NACH_FEHLER;
}

void senderStatus(JsonObject o) {
  o["ziel"] = cfg.zentrale;
  o["adresse"] = basisUrl;
  o["gesendet"] = gesendet;
  o["offen"] = puffer.gesamt() > naechsteNr ? puffer.gesamt() - naechsteNr : 0;
  o["fehler"] = fehler;
  o["letzter_fehler"] = letzterFehler;
}
