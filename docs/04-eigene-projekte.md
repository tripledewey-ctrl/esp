# 4 – Eigene Projekte

Jedes Projekt ist ein eigenständiger PlatformIO-Ordner unter `projects/`:

```
projects/mein-projekt/
├── platformio.ini            Umgebungen (Boards) + Bibliotheken
├── include/
│   ├── secrets.example.h     Vorlage für Zugangsdaten (eingecheckt)
│   └── secrets.h             echte Zugangsdaten (NICHT eingecheckt)
├── src/
│   └── main.cpp              Programm
└── README.md                 Beschreibung, Verdrahtung
```

## Neues Projekt anlegen

```bash
./scripts/new-project.sh mein-projekt
cd projects/mein-projekt
# include/secrets.h anpassen
pio run -e esp32dev -t upload -t monitor
```

## Vorhandene Projekte als Ausgangspunkt

| Projekt                                              | Inhalt                                          |
|------------------------------------------------------|-------------------------------------------------|
| [`blink`](../projects/blink)                         | Board-Test: LED + serielle Ausgabe              |
| [`webserver-ota`](../projects/webserver-ota)         | WLAN, Weboberfläche, JSON-API, mDNS, OTA-Update |
| [`sensor-mqtt`](../projects/sensor-mqtt)             | BME280 → MQTT, Home-Assistant-Discovery         |
| [`matrix-demo`](../projects/matrix-demo)             | Waveshare ESP32-S3-Matrix: Pixel-/Farbtest, Effekte |
| [`wlan-matrix`](../projects/wlan-matrix)             | WLAN-Ausleuchtung/Mesh-Knoten auf LED-Matrix + Weboberfläche, WiFiManager-Portal |
| [`wlan-sonde`](../projects/wlan-sonde)               | WLAN-Analyse-Modul: Sonde, Begehung, Zentrale – Auswertung mit [`tools/wlan-analyse`](../tools/wlan-analyse) |

Ein Projekt kopieren statt der Vorlage: `cp -r projects/webserver-ota projects/neu`.

## platformio.ini – das Wichtigste

```ini
[platformio]
extra_configs = ../../boards/boards.ini   ; gemeinsame Board-Einstellungen
default_envs  = esp32dev                  ; was "pio run" ohne -e baut

[project]                                 ; eigene Variablen für alle Envs
lib_deps =
    knolleary/PubSubClient@^2.8

[env:esp32dev]
extends     = esp32_base
lib_deps    = ${project.lib_deps}
build_flags = ${esp32_base.build_flags} -D MEIN_FLAG=1   ; zusätzliche Defines
```

Bibliotheken suchen: <https://registry.platformio.org> oder `pio pkg search <name>`.

## Code für ESP32 *und* ESP8266

Die Plattformen unterscheiden sich vor allem bei WLAN und Webserver:

```cpp
#if defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
#endif
```

Zusätzlich setzt `boards/boards.ini` `BOARD_ESP32`, `BOARD_ESP32S3`,
`BOARD_ESP32C3` bzw. `BOARD_ESP8266` sowie `LED_BUILTIN_PIN` und
`MONITOR_SPEED`.

## Zugangsdaten

`secrets.h` ist in `.gitignore` eingetragen. Die Beispielprojekte binden
automatisch `secrets.example.h` ein, wenn `secrets.h` fehlt – so bauen sie auch
frisch ausgecheckt und in der CI, aber natürlich ohne gültiges WLAN.

## Häufige Befehle

| Befehl                                   | Wirkung                              |
|------------------------------------------|--------------------------------------|
| `pio run`                                | Standard-Env bauen                   |
| `pio run -e nodemcuv2`                   | bestimmtes Env bauen                 |
| `pio run -e esp32dev -t upload`          | bauen + hochladen                    |
| `pio device monitor`                     | serieller Monitor                    |
| `pio run -t erase`                       | kompletten Flash löschen             |
| `pio run -t uploadfs`                    | `data/`-Ordner ins Dateisystem laden |
| `pio run -t clean`                       | Build-Dateien löschen                |
| `./scripts/build-all.sh`                 | alle Projekte & Envs bauen           |

## Einchecken

Vor dem Commit `./scripts/build-all.sh` ausführen. Die GitHub-Action
(`.github/workflows/build.yml`) baut bei jedem Push alle Projekte. Ein neues
Projekt in die `matrix.project`-Liste der Action aufnehmen.
