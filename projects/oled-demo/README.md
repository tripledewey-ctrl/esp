# oled-demo

Funktionstest für Boards mit kleinem OLED:
[ESP32-C3 mit 0,42″-OLED](../../docs/boards/esp32-c3-oled-042.md) (72×40),
[Heltec WiFi Kit 8](../../docs/boards/heltec-wifi-kit-8.md) (128×32) und ein externes
0,96″-SSD1306 (128×64, I2C) am [DOIT DevKit V1](../../docs/boards/doit-esp32-devkit-v1.md)
oder [NodeMCU](../../docs/boards/nodemcu-v2.md).

```bash
cd projects/oled-demo
pio run -e esp32c3_oled      -t upload -t monitor
pio run -e heltec_wifi_kit_8 -t upload -t monitor
pio run -e doit_devkit_v1    -t upload -t monitor   # externes OLED an SDA 21 / SCL 22
pio run -e nodemcuv2         -t upload -t monitor   # externes OLED an D2 / D1
```

| Anzeige (je 3 s)    | Inhalt                                    | Was wird geprüft?                         |
|---------------------|-------------------------------------------|-------------------------------------------|
| beim Start (2 s)    | Rahmen + Diagonalen über die ganze Fläche | Treiber und Größe richtig, kein Versatz   |
| Seite 1             | Board, Chip, Flash-Größe, MAC             | richtiges Board/Env, Flash erkannt        |
| Seite 2             | Anzahl WLANs, stärkstes Netz mit dBm      | WLAN-Funk und Antenne                     |
| Seite 3             | Laufzeit, freier Heap, LED-Pin            | läuft stabil, keine Neustarts             |

Die Onboard-LED leuchtet während des WLAN-Scans (alle 30 s). Alle Ausgaben erscheinen
zusätzlich im seriellen Monitor. Beim 128×32-Display fehlt jeweils die 4. Zeile.

Display-Pins und -Größe kommen aus `boards/boards.ini` (`OLED_SDA_PIN`, `OLED_SCL_PIN`,
`OLED_RST_PIN`, `OLED_WIDTH`, `OLED_HEIGHT`). Bleibt das Display dunkel, dort die
Pins prüfen (beim Heltec je nach Revision verschieden).
