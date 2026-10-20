# webserver-ota

WLAN-Grundgerüst für eigene Projekte:

- Verbindung ins WLAN mit Hostname, automatischer Neustart bei Fehlschlag
- Weboberfläche unter `http://<HOSTNAME>.local/` (Status, LED schalten)
- JSON-Status unter `/api/status`
- Firmware-Updates per WLAN (ArduinoOTA, passwortgeschützt)

## Benutzung

```bash
cd projects/webserver-ota
cp include/secrets.example.h include/secrets.h   # WLAN-Daten eintragen
pio run -e esp32dev -t upload -t monitor         # 1. Upload per USB
pio run -e esp32dev_ota -t upload                # danach per WLAN
```

Für OTA müssen `ota_host` und `ota_auth` in `platformio.ini` zu `HOSTNAME`
und `OTA_PASSWORD` aus `secrets.h` passen. Statt `xyz.local` geht auch die IP.
