# 3 – WLED

Die komplette Anleitung für WLED liegt direkt beim WLED-Ordner:
**[wled/README.md](../wled/README.md)**

Kurzfassung:

```bash
# A) Am schnellsten: im Chrome/Edge https://install.wled.me öffnen

# B) Fertige Release-Datei flashen
./scripts/flash.sh WLED_0.15.5_ESP8266.bin

# C) Eigener Build mit festen Pins/LED-Anzahl (wled/platformio_override.ini)
./scripts/wled-build.sh wled_esp32

# Konfiguration und Presets aller Geräte sichern
./scripts/wled-backup.sh wled-kueche.local wled-flur.local
```
