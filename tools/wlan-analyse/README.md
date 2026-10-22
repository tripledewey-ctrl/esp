# wlan-analyse

Auswertung des **WLAN-Analyse-Moduls**: liest die Messungen der
[wlan-sonde](../../projects/wlan-sonde)-Geräte und die Mesh-Topologie der FRITZ!Box und
erstellt einen HTML-Bericht mit Lageplan, Heatmap, Befunden und Maßnahmen.
Hintergrund und Messplan: [docs/06-wlan-analyse.md](../../docs/06-wlan-analyse.md).

- nur Python-Standardbibliothek (Python ≥ 3.9), keine Installation nötig
- Bericht ist eine einzelne HTML-Datei, funktioniert offline und im Ausdruck

<p align="center"><img src="../../docs/img/wlan-analyse-bericht.png" alt="Beispielbericht" width="820"></p>

## Schnellstart

```bash
cd tools/wlan-analyse
python3 -m wlan_analyse demo --oeffnen          # Beispiel: zwei Gebäude mit Halle
```

## Befehle

| Befehl | Zweck |
|---|---|
| `fritz --benutzer admin -o daten/fritz-mesh.json` | Mesh-Topologie per TR-064 von der FRITZ!Box lesen, Übersicht ausgeben |
| `mesh daten/fritz-mesh.json` | gespeicherte Mesh-Liste anzeigen |
| `abrufen wlan-zentrale.local sonde-mobil.local -o daten/messungen.jsonl` | Exporte von Geräten holen (wird angehängt) |
| `sammeln --port 8080 -o daten/messungen.jsonl` | Messungen der Sonden auf diesem Rechner sammeln, statt ESP32-Zentrale |
| `bericht -s daten/standort.json -m daten/*.jsonl --mesh daten/fritz-mesh.json -o daten/bericht.html` | Bericht erstellen (`--json` zusätzlich Ergebnis als JSON, `--oeffnen` im Browser) |
| `demo` | Beispielbericht aus `beispiel/` |

Alle Befehle: `python3 -m wlan_analyse --help` bzw. `python3 -m wlan_analyse <befehl> --help`.

## Eigene Daten

```bash
mkdir -p daten
cp beispiel/standort.json daten/standort.json   # Lageplan anpassen
```

Der Ordner `daten/` ist in `.gitignore` eingetragen. Die Mesh-Liste der FRITZ!Box enthält MAC-Adressen
und Namen aller Geräte im Heimnetz und gehört nicht ins Repository.

Das Passwort für `fritz` wird abgefragt oder aus der Umgebungsvariable `FRITZ_PASSWORT` gelesen.

## Was der Bericht prüft

| Prüfung | Grundlage |
|---|---|
| Abdeckung je Bereich (A / Halle / B) | Anteil der Messstellen mit bestem Knoten ≥ −67 dBm |
| Funklöcher | Messpunkte unter −80 dBm |
| Repeater-Anbindung | WLAN-Datenrate < 150 Mbit/s (Warnung) bzw. < 60 Mbit/s (kritisch), Kaskaden |
| Roaming | verbundener Knoten ≥ 8 dB schwächer als der beste („falscher Knoten“) |
| Stabilität | Abbrüche, Paketverlust > 2 %, Schwankung (10 %-Wert) |
| Rückrichtung | Empfang der Sonde am Access Point laut FRITZ!Box |
| Kanäle | überlappende Fremdnetze (2,4 GHz, Abstand < 5 Kanäle), stark belegter eigener Kanal |
| Zuordnung | BSSIDs ohne bekanntes Gerät (fremder AP mit gleicher SSID?) |

## Aufbau

| Datei | Inhalt |
|---|---|
| `wlan_analyse/daten.py` | Lageplan (`standort.json`) und Messungen (JSON Lines) laden |
| `wlan_analyse/fritz.py` | TR-064-Abruf und Auswertung der Mesh-Liste |
| `wlan_analyse/analyse.py` | Zuordnung BSSID → Gerät, Kennzahlen, Heatmap, Befunde |
| `wlan_analyse/bericht.py` | HTML-Bericht mit SVG-Lageplan |
| `wlan_analyse/sammler.py` | HTTP-Sammler (`POST /api/sonde`) und Abruf von `/api/export` |
| `beispiel/erzeugen.py` | erzeugt die Beispieldaten aus einem Ausbreitungsmodell |

## Tests

```bash
cd tools/wlan-analyse
python3 -m unittest discover -s tests -v
```

Die Tests prüfen auch das Datenformat der Firmware (`FIRMWARE_*` in `tests/test_wlan_analyse.py`).
Ändert sich `messungAlsJson()` in `projects/wlan-sonde/src/messung.cpp`, müssen Tests und
[Datenformat](../../docs/06-wlan-analyse.md#7-datenformat) mitgezogen werden.
