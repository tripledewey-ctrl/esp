# 2 – Boards

> Ausführliche Seiten pro Board (Bezeichnung von Familie bis Board, Abbildung,
> Pinbelegung): **[docs/boards/](boards/README.md)**

Alle Board-Einstellungen stehen zentral in [`boards/boards.ini`](../boards/boards.ini).
Jedes Projekt bindet die Datei ein und erbt mit `extends` von einer Basis:

| Basis-Sektion   | Env-Name in Projekten | Board                                  | PlatformIO-Board      |
|-----------------|-----------------------|----------------------------------------|-----------------------|
| `esp32_base`    | `esp32dev`            | ESP32-DevKitC, WROOM-32, NodeMCU-32S   | `esp32dev`            |
| `esp32s3_base`  | `esp32s3`             | ESP32-S3-DevKitC-1                     | `esp32-s3-devkitc-1`  |
| `esp32c3_base`  | `esp32c3`             | ESP32-C3-DevKitM-1, C3 Super Mini      | `esp32-c3-devkitm-1`  |
| `esp8266_base`  | `nodemcuv2`           | NodeMCU v2/v3 (ESP-12E/F)              | `nodemcuv2`           |
| `d1mini_base`   | `d1mini`              | Wemos/LOLIN D1 mini                    | `d1_mini`             |
| `doit_devkit_v1_base` | `doit_devkit_v1` | DOIT ESP32 DevKit V1, 30 Pins (HW-463) | `esp32doit-devkit-v1` |
| `heltec_wifi_kit_8_base` | `heltec_wifi_kit_8` | Heltec WiFi Kit 8 (ESP8266 + OLED 128×32) | `heltec_wifi_kit_8` |
| `esp32c3_oled_base` | `esp32c3_oled`    | ESP32-C3 mit 0,42″-OLED 72×40           | `esp32-c3-devkitm-1`  |

Ein neues Board ergänzen: Sektion in `boards/boards.ini` anlegen (Board-ID
über `pio boards esp32` bzw. `pio boards esp8266` suchen) und in den Projekten
ein `[env:...]` mit `extends = <neue_basis>` hinzufügen.

## Vergleich

|                    | ESP8266         | ESP32            | ESP32-S3              | ESP32-C3           |
|--------------------|-----------------|------------------|-----------------------|--------------------|
| CPU                | 1× 80/160 MHz   | 2× 240 MHz       | 2× 240 MHz            | 1× 160 MHz RISC-V  |
| RAM                | ~80 KB          | 520 KB           | 512 KB (+PSRAM)       | 400 KB             |
| WLAN / Bluetooth   | 2,4 GHz / –     | 2,4 GHz / BT+BLE | 2,4 GHz / BLE 5       | 2,4 GHz / BLE 5    |
| GPIOs              | 11 nutzbar      | ~25              | ~36                   | ~15                |
| ADC                | 1× (0–1 V)      | 18× (ADC2 nicht mit WLAN) | 20×          | 6×                 |
| USB                | über Wandler    | über Wandler     | nativ                 | nativ              |
| WLED geeignet      | ja (bis ~500 LEDs) | ja, empfohlen | ja                    | ja                 |

**Faustregel:** Für neue Projekte ESP32 nehmen. ESP8266/D1 mini ist günstig und
klein – ideal für einfache Sensoren und kleine WLED-Installationen.

## Pin-Hinweise

### ESP32

- **Nicht verwenden:** GPIO 6–11 (interner Flash).
- **Nur Eingang:** GPIO 34, 35, 36 (VP), 39 (VN) – kein Pull-up.
- **Strapping-Pins** (Pegel beim Booten wichtig): 0, 2, 5, 12, 15.
  GPIO12 beim Start HIGH → Board bootet nicht.
- **ADC2** (GPIO 0, 2, 4, 12–15, 25–27) funktioniert nicht, solange WLAN aktiv ist.
- I2C Standard: SDA 21, SCL 22. Onboard-LED meist GPIO2.

### ESP8266 (NodeMCU / D1 mini)

| Beschriftung | GPIO | Hinweis                                     |
|--------------|------|---------------------------------------------|
| D0           | 16   | kein PWM/I2C/Interrupt, Wake-up aus Deep Sleep |
| D1           | 5    | I2C SCL – frei verwendbar                   |
| D2           | 4    | I2C SDA – frei verwendbar                   |
| D3           | 0    | FLASH-Taste, beim Boot HIGH                 |
| D4           | 2    | Onboard-LED (active low), beim Boot HIGH    |
| D5–D7        | 14, 12, 13 | SPI – frei verwendbar                 |
| D8           | 15   | beim Boot LOW                               |
| RX / TX      | 3 / 1| Serielle Schnittstelle                      |
| A0           | ADC  | 0–1 V (NodeMCU/D1 mini: Teiler bis 3,3 V)   |

In der Arduino-Welt können die Namen `D1`, `D2` … direkt im Code genutzt werden.

### ESP32-C3

- Nicht verwenden: GPIO 11–17 (Flash).
- Strapping-Pins: 2, 8, 9 (GPIO9 = BOOT-Taste).
- Onboard-LED: meist GPIO8 (DevKitM: RGB-LED).

### ESP32-S3

- Nicht verwenden: GPIO 26–32 (Flash), bei Octal-PSRAM auch 33–37.
- Strapping-Pins: 0, 3, 45, 46.
- RGB-LED auf dem DevKitC-1: GPIO48 (v1.0) bzw. GPIO38 (v1.1).

## Flash-Modus / Upload-Probleme

Startet der Upload nicht („Failed to connect … Timed out waiting for packet
header“): **BOOT** gedrückt halten, kurz **EN/RST** drücken, BOOT loslassen,
dann Upload starten. Mehr dazu in [05 – Fehlerbehebung](05-fehlerbehebung.md).
