# matrix-demo

Test- und Demoprogramm für die
[Waveshare ESP32-S3-Matrix](../../docs/boards/waveshare-esp32-s3-matrix.md)
(8×8 WS2812 an GPIO14). Läuft auch mit einer externen 8×8-Matrix am ESP32.

```bash
cd projects/matrix-demo
pio run -e waveshare_s3_matrix -t upload -t monitor
```

| Modus (BOOT-Taste) | Anzeige                         | Zweck                                         |
|--------------------|---------------------------------|-----------------------------------------------|
| 0                  | ein weißes Pixel wandert        | LED-Reihenfolge prüfen (Index im Monitor)     |
| 1                  | Regenbogen                      | Funktionstest aller Farben                    |
| 2                  | blaue Spalte läuft durch        | prüft die x/y-Zuordnung in `xy()`             |

Die Helligkeit ist bewusst auf 20/255 begrenzt – die Matrix wird bei voller
Helligkeit sehr heiß und zieht über 3 A.
