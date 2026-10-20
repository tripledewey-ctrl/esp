# 5 – Fehlerbehebung

| Problem                                                        | Lösung                                                                                                         |
|----------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------|
| Kein serieller Port sichtbar                                   | USB-**Daten**kabel verwenden (viele Kabel laden nur), Treiber installieren ([01](01-einrichtung.md)), anderen USB-Port testen |
| `Permission denied: /dev/ttyUSB0`                              | Linux: `sudo usermod -aG dialout $USER`, neu anmelden                                                          |
| `Failed to connect … Timed out waiting for packet header`      | BOOT halten, EN/RST kurz drücken, BOOT loslassen. Ggf. `upload_speed = 115200` setzen                          |
| Upload klappt, danach nur Zeichensalat im Monitor              | Baudrate prüfen: `monitor_speed = 115200`; nach dem Reset gibt der ROM-Bootloader 74880 Baud aus (ESP8266)      |
| ESP32-S3/C3: keine Ausgabe im Monitor                          | `ARDUINO_USB_CDC_ON_BOOT=1` ist gesetzt – nach dem Upload einmal RST drücken, Port kann wechseln             |
| `Brownout detector was triggered`                              | Stromversorgung zu schwach: anderes Kabel/Netzteil, 100–470 µF an 3V3/GND                                      |
| Bootschleife nach Upload                                       | `pio run -t erase`, dann neu flashen. Strapping-Pins (GPIO 0, 2, 12, 15) nicht beschalten                      |
| `Guru Meditation Error` / `Exception (28)`                      | Monitor zeigt dank `exception_decoder`-Filter die Codezeile an – meist Null-Pointer oder Stack-Überlauf        |
| WLAN verbindet nicht                                           | Nur 2,4 GHz wird unterstützt; SSID/Passwort in `secrets.h` prüfen; Router-Kanal 1–13                           |
| `xyz.local` nicht erreichbar                                   | mDNS wird von manchen Systemen/Netzen blockiert – IP aus dem seriellen Monitor oder Router verwenden            |
| OTA: `No response from device`                                 | Gerät und PC im selben Netz? Firewall (UDP/TCP) erlauben; `ota_host`/`ota_auth` in `platformio.ini` prüfen      |
| `Sketch too big` / Partition zu klein                          | ESP32: `board_build.partitions = huge_app.csv` (dann kein OTA); ESP8266: `eagle.flash.4m1m.ld`                 |
| `fatal error: secrets.h: No such file`                         | `include/secrets.example.h` nach `include/secrets.h` kopieren                                                  |
| `HTTPClientError` beim ersten Build                            | PlatformIO braucht Internetzugriff auf `registry.platformio.org` und `dl.registry.platformio.org`              |

## WLED-spezifisch

| Problem                                   | Lösung                                                                                        |
|-------------------------------------------|-----------------------------------------------------------------------------------------------|
| LEDs flackern / falsche Farben            | Gemeinsame Masse prüfen, 330 Ω in Datenleitung, Pegelwandler einsetzen, Farbreihenfolge (GRB/RGB) in LED Preferences |
| Nur die erste LED leuchtet falsch         | Pegelproblem → Pegelwandler oder „Sacrificial Pixel“ nahe am ESP                              |
| Helligkeit bricht ein                     | Stromlimit (ABL) in LED Preferences / Netzteil zu klein / Einspeisung an beiden Enden         |
| WLED-AP erscheint nicht                   | 3 Minuten warten oder per Web-Installer neu flashen; AP erscheint nur ohne gültige WLAN-Daten  |
| Gerät nach Update nicht erreichbar        | Falsche Datei (z. B. ESP32-Datei auf ESP8266) → per USB mit Web-Installer neu flashen          |
