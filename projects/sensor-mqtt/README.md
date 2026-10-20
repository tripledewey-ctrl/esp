# sensor-mqtt

Klimasensor mit **BME280** (Temperatur, Luftfeuchte, Luftdruck), der die Werte
alle 30 s als JSON per **MQTT** sendet. Home Assistant erkennt den Sensor
automatisch (MQTT-Discovery).

## Verdrahtung

| BME280 | ESP32  | ESP32-S3 / C3 | ESP8266 / D1 mini |
|--------|--------|---------------|-------------------|
| VIN    | 3V3    | 3V3           | 3V3               |
| GND    | GND    | GND           | GND               |
| SDA    | GPIO21 | GPIO8         | D2 (GPIO4)        |
| SCL    | GPIO22 | GPIO9         | D1 (GPIO5)        |

## Benutzung

```bash
cd projects/sensor-mqtt
cp include/secrets.example.h include/secrets.h   # WLAN + MQTT eintragen
pio run -e esp32dev -t upload -t monitor
```

## Topics

| Topic                                  | Inhalt                                   |
|----------------------------------------|------------------------------------------|
| `<HOSTNAME>/state`                     | `{"temperature":21.4,"humidity":45.2,…}` |
| `<HOSTNAME>/status`                    | `online` / `offline` (Last Will)         |
| `homeassistant/sensor/<HOSTNAME>/…`    | Discovery-Konfiguration (retained)       |

Testen ohne Home Assistant: `mosquitto_sub -h <broker> -t '<HOSTNAME>/#' -v`

Sendeintervall ändern: in `platformio.ini` unter dem Env
`build_flags = ${esp32_base.build_flags} -D PUBLISH_INTERVAL_MS=60000` ergänzen.
