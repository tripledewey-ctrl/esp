#include "speicher.h"

#include <LittleFS.h>

#include "einstellungen.h"

Ringpuffer puffer;
SondeInfo sonden[MAX_SONDEN];
int anzahlSonden = 0;

static const char* PUNKTE = "/punkte.jsonl";
static const char* KNOTEN = "/knoten.json";
static JsonDocument knotenNamen;
static size_t punkteZahl = 0;

// --------------------------------- Ringpuffer -------------------------------
bool Ringpuffer::begin(size_t kapazitaet) {
  const size_t bytes = kapazitaet * sizeof(Messung);
  daten_ = psramFound() ? (Messung*)ps_malloc(bytes) : (Messung*)malloc(bytes);
  if (!daten_) {
    kapazitaet = 50;                                  // Notlösung bei knappem Speicher
    daten_ = (Messung*)malloc(kapazitaet * sizeof(Messung));
  }
  kap_ = daten_ ? kapazitaet : 0;
  return daten_ != nullptr;
}

void Ringpuffer::add(const Messung& m) {
  if (!kap_) return;
  memcpy(&daten_[kopf_], &m, sizeof(Messung));
  kopf_ = (kopf_ + 1) % kap_;
  if (anzahl_ < kap_) anzahl_++;
  gesamt_++;
}

const Messung& Ringpuffer::alt(size_t i) const {
  return daten_[(kopf_ + kap_ - anzahl_ + i) % kap_];
}

const Messung* Ringpuffer::nachNummer(uint32_t nr) const {
  if (nr >= gesamt_ || gesamt_ - nr > anzahl_) return nullptr;
  return &alt(anzahl_ - (gesamt_ - nr));
}

// -------------------------------- Sondentabelle -----------------------------
void speicherStart(size_t kapazitaet) {
  puffer.begin(kapazitaet);
  anzahlSonden = 1;                                   // Eintrag 0 = dieses Gerät
  strlcpy(sonden[0].name, cfg.name.c_str(), sizeof(sonden[0].name));
  strlcpy(sonden[0].bereich, cfg.bereich.c_str(), sizeof(sonden[0].bereich));
  strlcpy(sonden[0].ort, cfg.ort.c_str(), sizeof(sonden[0].ort));
  sonden[0].intervall = cfg.intervall;

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS nicht verfügbar");
    return;
  }
  File f = LittleFS.exists(PUNKTE) ? LittleFS.open(PUNKTE, "r") : File();
  if (f) {
    while (f.available()) {
      if (f.readStringUntil('\n').length() > 2) punkteZahl++;
    }
    f.close();
  }
}

int sondeFinden(const char* name, bool anlegen) {
  for (int i = 0; i < anzahlSonden; i++)
    if (strcmp(sonden[i].name, name) == 0) return i;
  if (!anlegen || anzahlSonden >= MAX_SONDEN) return -1;
  SondeInfo& s = sonden[anzahlSonden];
  s = SondeInfo();
  strlcpy(s.name, name, sizeof(s.name));
  ereignis("Neue Sonde: %s", name);
  return anzahlSonden++;
}

bool sondeVeraltet(int idx) {
  if (idx == 0) return false;
  const SondeInfo& s = sonden[idx];
  const uint32_t erlaubt = (uint32_t)max<uint16_t>(s.intervall, 10) * 3000UL + 10000UL;
  return !s.hatMessung || millis() - s.zuletzt > erlaubt;
}

void sondeAlsJson(int idx, JsonObject o) {
  const SondeInfo& s = sonden[idx];
  o["name"] = s.name;
  o["bereich"] = s.bereich;
  o["ort"] = s.ort;
  o["intervall"] = s.intervall;
  o["eigen"] = idx == 0;
  o["veraltet"] = sondeVeraltet(idx);
  o["alter"] = idx == 0 ? 0 : (millis() - s.zuletzt) / 1000;
  if (idx > 0) o["ip"] = s.ip.toString();
  if (s.hatMessung)
    messungAlsJson(s.letzte, s.name, s.bereich, s.ort, s.intervall, o["messung"].to<JsonObject>());
}

void messungZeile(const Messung& m, String& zeile) {
  const SondeInfo& s = sonden[m.sonde < anzahlSonden ? m.sonde : 0];
  JsonDocument doc;
  messungAlsJson(m, s.name, s.bereich, s.ort, s.intervall, doc.to<JsonObject>());
  zeile = "";
  serializeJson(doc, zeile);
  zeile += '\n';
}

// ------------------------------ Messpunkte (mobil) --------------------------
const char* punkteDatei() { return PUNKTE; }
size_t punkteAnzahl() { return punkteZahl; }

bool punktSpeichern(const Messung& m) {
  String zeile;
  messungZeile(m, zeile);
  File f = LittleFS.open(PUNKTE, FILE_APPEND);
  if (!f) return false;
  const bool ok = f.print(zeile) == zeile.length();
  f.close();
  if (ok) punkteZahl++;
  return ok;
}

bool punkteLoeschen(bool nurLetzten) {
  if (!nurLetzten || punkteZahl <= 1) {
    punkteZahl = 0;
    return !LittleFS.exists(PUNKTE) || LittleFS.remove(PUNKTE);
  }
  // alle Zeilen außer der letzten in eine neue Datei kopieren
  File alt = LittleFS.open(PUNKTE, "r");
  File neu = LittleFS.open("/punkte.tmp", "w");
  if (!alt || !neu) return false;
  size_t behalten = punkteZahl - 1, n = 0;
  while (alt.available() && n < behalten) {
    const String zeile = alt.readStringUntil('\n');
    if (zeile.length() < 2) continue;
    neu.print(zeile);
    neu.print('\n');
    n++;
  }
  alt.close();
  neu.close();
  LittleFS.remove(PUNKTE);
  LittleFS.rename("/punkte.tmp", PUNKTE);
  punkteZahl = n;
  return true;
}

// ---------------------------- Knotennamen (zentrale) ------------------------
void knotenNamenLaden() {
  File f = LittleFS.exists(KNOTEN) ? LittleFS.open(KNOTEN, "r") : File();
  if (!f || deserializeJson(knotenNamen, f)) knotenNamen.to<JsonObject>();
  if (f) f.close();
}

String knotenName(const String& bssid) {
  return knotenNamen[bssid] | "";
}

bool knotenNameSetzen(const String& bssid, const String& name) {
  if (name.isEmpty()) knotenNamen.remove(bssid);
  else knotenNamen[bssid] = name;
  File f = LittleFS.open(KNOTEN, "w");
  if (!f) return false;
  serializeJson(knotenNamen, f);
  f.close();
  return true;
}
