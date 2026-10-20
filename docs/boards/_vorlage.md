# <Board-Name>

<!-- Vorlage für neue Board-Seiten. Kopieren nach docs/boards/<board-name>.md.
     Bild erzeugen: docs/boards/specs/<board-name>.json anlegen und
     python3 scripts/board-image.py docs/boards/specs/<board-name>.json ausführen. -->

<p align="center"><img src="img/<board-name>.svg" alt="<Board-Name> Pinbelegung" width="760"></p>

<Ein bis zwei Sätze: Was ist das Board, wofür eignet es sich, unter welchen Namen wird es verkauft?>

## Von der Familie zum Board

| Ebene       | Bezeichnung        | Erklärung |
|-------------|--------------------|-----------|
| Hersteller  |                    |           |
| Familie     | **ESP32-…**        |           |
| Chip        | **…**              |           |
| Modul       | **…** oder – (keins) |         |
| Board       | **…**              |           |
| PlatformIO  | `<board-id>` → Basis `<basis>` | siehe [`boards/boards.ini`](../../boards/boards.ini) |

## Technische Daten

| Eigenschaft | Wert |
|-------------|------|
| CPU         |      |
| Speicher    |      |
| Funk        |      |
| GPIOs       |      |
| USB         |      |
| Versorgung  |      |
| Logikpegel  | 3,3 V |

## Pinbelegung

**Frei nutzbar (empfohlen):** …

| GPIO | Einschränkung |
|------|---------------|
|      |               |

## Verwendung im Repo

```bash
cd projects/blink
pio run -e <env> -t upload -t monitor
```

| Zweck    | Einstellung |
|----------|-------------|
| Projekte | `[env:<env>]` mit `extends = <basis>` |
| WLED     |             |

## Typische Probleme

| Problem | Ursache / Lösung |
|---------|------------------|
|         |                  |

## Quellen

- Hersteller: [Produktseite](https://…)
