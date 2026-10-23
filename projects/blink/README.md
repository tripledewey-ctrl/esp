# blink

Erster Funktionstest für ein neues Board: LED blinkt, serielle Ausgabe läuft.

```bash
cd projects/blink
pio run -e esp32dev  -t upload -t monitor   # ESP32
pio run -e nodemcuv2 -t upload -t monitor   # ESP8266 NodeMCU
pio run -e d1mini    -t upload -t monitor   # Wemos D1 mini
pio run -e doit_devkit_v1    -t upload -t monitor   # DOIT ESP32 DevKit V1 (HW-463)
pio run -e heltec_wifi_kit_8 -t upload -t monitor   # Heltec WiFi Kit 8 (keine Benutzer-LED, nur Monitor)
pio run -e esp32c3_oled      -t upload -t monitor   # ESP32-C3 0,42" OLED (LED GPIO8)
```

Hinweis: Beim ESP8266 ist die Onboard-LED „active low“ (LOW = an) – für Blinken egal.
