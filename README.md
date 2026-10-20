# ESP – Anleitungen & Konfigurationen

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
├── projects/                 Eigene Projekte (je ein PlatformIO-Projekt)
│   ├── _template/            Vorlage für neue Projekte
│   ├── blink/                Board-Test
│   ├── webserver-ota/        WLAN + Weboberfläche + OTA-Updates
│   └── sensor-mqtt/          BME280 → MQTT / Home Assistant
├── wled/
│   ├── README.md             WLED installieren, verdrahten, sichern
│   ├── platformio_override.ini  Eigene WLED-Builds (Pins, LED-Anzahl …)
│   └── config/               Beispiel-Presets, Sicherungen
├── scripts/                  Hilfsskripte (siehe unten)
└── .github/workflows/        CI: baut alle Projekte und WLED bei jedem Push
```

## Anleitungen

| Nr. | Thema                                                  | Inhalt                                          |
|-----|--------------------------------------------------------|-------------------------------------------------|
| 1   | [Einrichtung](docs/01-einrichtung.md)                  | PlatformIO, VS Code, USB-Treiber, Ports         |
| 2   | [Boards](docs/02-boards.md)                            | ESP32/S3/C3/ESP8266 im Vergleich, Pin-Hinweise  |
| 3   | [WLED](wled/README.md)                                 | Web-Installer, Release flashen, eigener Build   |
| 4   | [Eigene Projekte](docs/04-eigene-projekte.md)          | Struktur, neues Projekt, `platformio.ini`       |
| 5   | [Fehlerbehebung](docs/05-fehlerbehebung.md)            | Typische Fehler und Lösungen                    |

## Unterstützte Boards

Jedes Projekt bringt diese Umgebungen (`-e …`) mit:

| Env         | Board                                  |
|-------------|----------------------------------------|
| `esp32dev`  | ESP32 DevKit / WROOM-32 (Standard)     |
| `esp32s3`   | ESP32-S3-DevKitC-1                     |
| `esp32c3`   | ESP32-C3-DevKitM-1 / Super Mini        |
| `nodemcuv2` | ESP8266 NodeMCU                        |
| `d1mini`    | Wemos/LOLIN D1 mini                    |

Weitere Boards: [docs/02-boards.md](docs/02-boards.md).

## Skripte

| Skript                                   | Zweck                                                      |
|------------------------------------------|------------------------------------------------------------|
| `scripts/new-project.sh <name>`          | Neues Projekt aus der Vorlage anlegen                      |
| `scripts/build-all.sh [projekt …]`       | Alle (oder bestimmte) Projekte für alle Boards bauen       |
| `scripts/wled-build.sh [env …]`          | Eigene WLED-Firmware aus `wled/platformio_override.ini`    |
| `scripts/wled-backup.sh <host> …`        | `cfg.json` + `presets.json` von WLED-Geräten sichern       |
| `scripts/flash.sh <datei.bin> [port]`    | Fertige Firmware per USB (esptool) flashen                 |

Unter Windows die Skripte in **Git Bash** oder **WSL** ausführen.

## Zugangsdaten

WLAN- und MQTT-Daten gehören in `projects/<projekt>/include/secrets.h`
(Vorlage: `secrets.example.h`). `secrets.h` und WLED-Sicherungen sind per
`.gitignore` vom Einchecken ausgeschlossen.
