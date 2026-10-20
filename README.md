# ESP – Anleitungen & Konfigurationen

> 📖 Die Doku gibt es auch als **[Wiki](https://github.com/tripledewey-ctrl/esp/wiki)**.
> Es wird automatisch aus diesem Repo erzeugt.

Sammlung von Anleitungen, **PlatformIO**-Konfigurationen und Projekten für
**ESP32** und **ESP8266** – von der LED-Steuerung mit **WLED** bis zu eigenen
Sensoren. Auschecken, Board anschließen, loslegen.

```bash
git clone https://github.com/tripledewey-ctrl/esp.git
cd esp
pip install platformio                     # einmalig, siehe docs/01-einrichtung.md
cd projects/blink
pio run -e esp32dev -t upload -t monitor   # ESP8266: -e nodemcuv2 oder -e d1mini
```

## Inhalt

```
esp/
├── boards/
│   └── boards.ini            Gemeinsame Board-Konfiguration (ESP32, S3, C3, ESP8266, D1 mini)
├── docs/                     Schritt-für-Schritt-Anleitungen
│   └── boards/               Eine Seite pro Board: Bezeichnung, Bild, Pinbelegung
├── projects/                 Eigene Projekte (je ein PlatformIO-Projekt)
│   ├── _template/            Vorlage für neue Projekte
│   ├── blink/                Board-Test
│   ├── webserver-ota/        WLAN + Weboberfläche + OTA-Updates
│   ├── sensor-mqtt/          BME280 → MQTT / Home Assistant
│   ├── matrix-demo/          Waveshare ESP32-S3-Matrix (8×8 LEDs)
│   └── wlan-matrix/          WLAN-Signalstärke + Mesh-Knoten auf LED-Matrix, Webserver
├── wled/
│   ├── README.md             WLED installieren, verdrahten, sichern
│   ├── platformio_override.ini  Eigene WLED-Builds (Pins, LED-Anzahl …)
│   └── config/               Beispiel-Presets, Sicherungen
├── scripts/                  Hilfsskripte (siehe unten)
├── .claude/skills/esp-doku/  Claude-Skill: Doku nutzen, Boards ergänzen, Updates
└── .github/workflows/        CI: Doku-Prüfung, alle Projekte und WLED bei jedem Push
```

## Anleitungen

| Nr. | Thema                                                  | Inhalt                                          |
|-----|--------------------------------------------------------|-------------------------------------------------|
| 1   | [Einrichtung](docs/01-einrichtung.md)                  | PlatformIO, VS Code, USB-Treiber, Ports         |
| 2   | [Boards](docs/02-boards.md)                            | ESP32/S3/C3/ESP8266 im Vergleich, Pin-Hinweise  |
| 2a  | [Board-Seiten](docs/boards/README.md)                  | Bezeichnungen erklärt, je Board Bild + Pinout   |
| 3   | [WLED](wled/README.md)                                 | Web-Installer, Release flashen, eigener Build   |
| 4   | [Eigene Projekte](docs/04-eigene-projekte.md)          | Struktur, neues Projekt, `platformio.ini`       |
| 5   | [Fehlerbehebung](docs/05-fehlerbehebung.md)            | Typische Fehler und Lösungen                    |

## Unterstützte Boards

Die allgemeinen Projekte bringen diese Umgebungen (`-e …`) mit:

| Env         | Board                                  |
|-------------|----------------------------------------|
| `esp32dev`  | ESP32 DevKit / WROOM-32 (Standard)     |
| `esp32s3`   | ESP32-S3-DevKitC-1                     |
| `esp32c3`   | ESP32-C3-DevKitM-1 / Super Mini        |
| `nodemcuv2` | ESP8266 NodeMCU                        |
| `d1mini`    | Wemos/LOLIN D1 mini                    |

Spezialboard: **Waveshare ESP32-S3-Matrix** → Env `waveshare_s3_matrix` in
[`projects/matrix-demo`](projects/matrix-demo) und [`projects/wlan-matrix`](projects/wlan-matrix),
`wled_s3_matrix` für WLED.

Ausführliche Board-Seiten mit Abbildung: [docs/boards/](docs/boards/README.md).

## Skripte

| Skript                                   | Zweck                                                      |
|------------------------------------------|------------------------------------------------------------|
| `scripts/new-project.sh <name>`          | Neues Projekt aus der Vorlage anlegen                      |
| `scripts/build-all.sh [projekt …]`       | Alle (oder bestimmte) Projekte für alle Boards bauen       |
| `scripts/wled-build.sh [env …]`          | Eigene WLED-Firmware aus `wled/platformio_override.ini`    |
| `scripts/wled-backup.sh <host> …`        | `cfg.json` + `presets.json` von WLED-Geräten sichern       |
| `scripts/flash.sh <datei.bin> [port]`    | Fertige Firmware per USB (esptool) flashen                 |
| `scripts/board-image.py <spec.json>`     | Board-Abbildung (SVG) aus Pinbelegung erzeugen             |
| `scripts/check-docs.py`                  | Doku prüfen: Links, Board-Übersicht, Bilder, CI-Liste      |
| `scripts/build-wiki.py [ordner]`         | Wiki-Seiten aus der Doku erzeugen (lokale Vorschau)        |

Unter Windows die Skripte in **Git Bash** oder **WSL** ausführen.

## Wiki

Das [GitHub-Wiki](https://github.com/tripledewey-ctrl/esp/wiki) wird aus `README.md`,
`docs/`, `wled/` und den Projekt-READMEs erzeugt (`scripts/build-wiki.py`). Die Action
`.github/workflows/wiki.yml` veröffentlicht es bei jedem Push auf `main`.
**Änderungen immer im Repo machen.** Direkte Änderungen im Wiki werden beim
nächsten Lauf überschrieben.

Einmalige Einrichtung: **Settings → General → Features → Wikis** aktivieren, dann im
Reiter **Wiki** auf „Create the first page“ klicken und speichern. Danach unter
**Actions → Wiki → Run workflow** einmal starten.

## Claude-Skill „esp-doku“

Unter `.claude/skills/esp-doku/` liegt ein Skill für Claude Code. Er beantwortet
Fragen anhand der Doku in diesem Repo, nimmt neue Boards nach festem Ablauf auf
(Seite, Bild, `boards.ini`, Projekte, WLED, CI) und führt Updates durch, z. B. auf eine
neue WLED-Version. Aufruf in Claude Code: `/esp-doku` oder einfach fragen, etwa
„Dokumentiere das ESP32-C6-DevKitC-1“.

## Zugangsdaten

WLAN- und MQTT-Daten gehören in `projects/<projekt>/include/secrets.h`
(Vorlage: `secrets.example.h`). `secrets.h` und WLED-Sicherungen sind per
`.gitignore` vom Einchecken ausgeschlossen.
