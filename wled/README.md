# WLED

[WLED](https://kno.wled.ge) ist eine fertige Firmware zur Steuerung adressierbarer
LED-Streifen (WS2812B, SK6812, WS2815 …) über WLAN – mit App, Weboberfläche,
Home-Assistant-, Alexa- und MQTT-Anbindung.

Es gibt drei Wege zur Installation – von „einfach“ bis „voll anpassbar“:

| Weg                     | Wann?                                           | Werkzeug              |
|-------------------------|-------------------------------------------------|-----------------------|
| **A – Web-Installer**   | Standard-Board, Standardeinstellungen           | Chrome/Edge           |
| **B – Release-Datei**   | Bestimmte Version, ohne Browser                 | `scripts/flash.sh`    |
| **C – Eigener Build**   | Eigene Pins/LED-Anzahl/Usermods fest einbauen   | `scripts/wled-build.sh` |

---

## A – Web-Installer (empfohlen für den Einstieg)

1. ESP per USB-Datenkabel anschließen.
2. In **Chrome oder Edge** <https://install.wled.me> öffnen.
3. Version wählen → **Install** → seriellen Port auswählen.
4. Optional direkt im Installer die WLAN-Daten eingeben.

Klappt die Verbindung nicht: USB-Treiber prüfen (siehe
[docs/01-einrichtung.md](../docs/01-einrichtung.md)) und beim Verbinden die
**BOOT**-Taste gedrückt halten.

## B – Release-Datei flashen

Releases: <https://github.com/wled/WLED/releases>

| Board                  | Datei (Beispiel v0.15.x)          |
|------------------------|-----------------------------------|
| ESP32 (DevKit, WROOM)  | `WLED_0.15.x_ESP32.bin`           |
| ESP32-C3               | `WLED_0.15.x_ESP32-C3.bin`        |
| ESP32-S3 (8 MB, PSRAM) | `WLED_0.15.x_ESP32-S3_8MB_opi.bin`|
| Waveshare ESP32-S3-Matrix (4 MB) | `WLED_0.15.x_ESP32-S3_4M_qspi.bin` |
| ESP8266 (4 MB)         | `WLED_0.15.x_ESP8266.bin`         |
| ESP-01 (1 MB)          | `WLED_0.15.x_ESP01.bin`           |

```bash
pip install esptool
./scripts/flash.sh ~/Downloads/WLED_0.15.5_ESP8266.bin            # Port automatisch
ERASE=1 ./scripts/flash.sh WLED_0.15.5_ESP8266.bin /dev/ttyUSB0   # mit Löschen
```

> Für die **Erstinstallation auf ESP32** lieber Weg A nutzen – die ESP32-Releases
> enthalten keinen Bootloader. Updates bereits laufender Geräte gehen bequem über
> die WLED-Oberfläche (**Config → Security & Updates → Manual OTA Update**).

## C – Eigener Build mit festen Voreinstellungen

Alle Anpassungen stehen in [`platformio_override.ini`](platformio_override.ini):

```ini
[wled_custom]
build_flags =
    -D SERVERNAME='"WLED-Custom"'
    -D PIXEL_COUNTS=60              ; Anzahl LEDs
    -D ABL_MILLIAMPS_DEFAULT=2000   ; Strombegrenzung in mA
    ...

[env:wled_esp32]
extends     = env:esp32dev
build_flags = ...
    -D DATA_PINS=16                 ; GPIO für die LED-Datenleitung
    ${wled_custom.build_flags}
```

Bauen (benötigt PlatformIO, Git und Node.js ≥ 20):

```bash
./scripts/wled-build.sh                  # alle eigenen Envs
./scripts/wled-build.sh wled_esp32       # nur ESP32
WLED_VERSION=v0.15.5 ./scripts/wled-build.sh
```

Das Skript lädt den WLED-Quellcode nach `wled/build/`, kopiert die Override-Datei
hinein und legt die fertigen Dateien in `wled/build/bin/` ab. Flashen per
Weboberfläche (OTA), `scripts/flash.sh` oder direkt:

```bash
pio run -d wled/build/WLED-v0.15.5 -e wled_esp32 -t upload
```

Die GitHub-Action baut die Firmware bei jedem Push mit und stellt sie unter
**Actions → Build → Artifacts** zum Download bereit.

Weitere Optionen (Usermods, Audioreactive, Ethernet, IR …) sind in
`wled/build/WLED-*/platformio_override.sample.ini` dokumentiert.

---

## Ersteinrichtung nach dem Flashen

1. Mit dem WLAN **`WLED-AP`** verbinden (Passwort `wled1234`).
2. Es öffnet sich die Einrichtungsseite, sonst <http://4.3.2.1>.
3. Heim-WLAN eintragen, speichern – WLED startet neu.
4. Gerät im Heimnetz finden: WLED-App, Router oder `http://wled-xxxxxx.local`.
5. **Config → LED Preferences**: LED-Typ, GPIO, Anzahl und Stromlimit einstellen.

## Verdrahtung

```
 Netzteil 5 V ──┬───────────────────── +5V  LED-Streifen
                │
              ESP 5V/VIN
 Netzteil GND ──┬───────────────────── GND  LED-Streifen
              ESP GND
 ESP GPIO ──[330 Ω]──────────────────── DIN  LED-Streifen
```

- **Gemeinsame Masse** zwischen ESP, Netzteil und Streifen ist Pflicht.
- 330 Ω-Widerstand in der Datenleitung, 1000 µF-Elko am Streifeneingang.
- Bei langen Leitungen (> 1 m) oder Flackern: Pegelwandler 3,3 V → 5 V
  (z. B. 74AHCT125 / SN74AHCT1G125).
- Strombedarf: ca. **60 mA pro LED** bei Vollweiß (WS2812B) – 60 LEDs ≈ 3,6 A.
  Stromlimit in WLED passend zum Netzteil setzen.

| Board            | Empfohlener Daten-Pin  | Hinweis                                   |
|------------------|------------------------|-------------------------------------------|
| ESP32            | GPIO16                 | fast alle Ausgänge möglich, nicht 6–11    |
| ESP32-C3         | GPIO10                 | nicht GPIO 11–17 (Flash)                  |
| ESP8266 / D1 mini| GPIO2 (D4) oder GPIO3 (RX, DMA) | GPIO3 ist am stabilsten         |
| Waveshare ESP32-S3-Matrix | GPIO14 (fest verdrahtet) | 64 LEDs, Stromlimit ≤ 500 mA – siehe [Board-Seite](../docs/boards/waveshare-esp32-s3-matrix.md) |

## Presets & Sicherung

Siehe [`config/README.md`](config/README.md).

## Steuerung per JSON-API (Beispiele)

```bash
# Einschalten mit 50 % Helligkeit
curl -X POST http://wled.local/json/state -H 'Content-Type: application/json' \
     -d '{"on":true,"bri":128}'

# Preset 3 aktivieren
curl -X POST http://wled.local/json/state -d '{"ps":3}'

# Farbe Rot auf Segment 0
curl -X POST http://wled.local/json/state -d '{"seg":[{"id":0,"col":[[255,0,0]]}]}'

# Status abfragen
curl http://wled.local/json/info
```
