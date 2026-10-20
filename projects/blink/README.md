# blink

Erster Funktionstest für ein neues Board: LED blinkt, serielle Ausgabe läuft.

```bash
cd projects/blink
pio run -e esp32dev  -t upload -t monitor   # ESP32
pio run -e nodemcuv2 -t upload -t monitor   # ESP8266 NodeMCU
pio run -e d1mini    -t upload -t monitor   # Wemos D1 mini
```

Hinweis: Beim ESP8266 ist die Onboard-LED „active low“ (LOW = an) – für Blinken egal.
