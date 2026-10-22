---
name: esp-doku
description: Nutzt und pflegt die ESP-Dokumentation dieses Repos (Boards, Pinbelegungen, PlatformIO-Konfiguration, WLED, Projekte). Verwenden bei Fragen zu einem ESP-Board, zu GPIOs/Pins, Flash/PSRAM, WLED-Einrichtung oder Projekten in diesem Repo; beim Aufnehmen eines neuen Boards ("füge Board X hinzu", "dokumentiere das ESP32-C6"); und bei Updates (neue WLED-Version, neue PlatformIO-Plattformversion, Korrekturen an Board-Seiten, Doku auf Stand bringen).
---

# ESP-Doku nutzen und pflegen

Dieses Repo ist die Wissensbasis für ESP-Boards, PlatformIO und WLED. Antworten und
Änderungen stützen sich **zuerst auf die Doku im Repo**, nicht auf Allgemeinwissen.

## Landkarte

| Thema                          | Datei(en)                                                            |
|--------------------------------|----------------------------------------------------------------------|
| Übersicht, Schnellstart        | `README.md`                                                          |
| Board-Übersicht + Bezeichnungen| `docs/boards/README.md`                                              |
| Eine Seite pro Board           | `docs/boards/<board>.md` (Vorlage: `docs/boards/_vorlage.md`)        |
| Board-Bilder                   | `docs/boards/specs/<board>.json` → `scripts/board-image.py` → `docs/boards/img/<board>.svg` |
| PlatformIO-Board-Basen         | `boards/boards.ini`                                                  |
| Allgemeine Anleitungen         | `docs/01-einrichtung.md` … `docs/05-fehlerbehebung.md`              |
| WLED                           | `wled/README.md`, `wled/platformio_override.ini`, `wled/config/`     |
| Projekte                       | `projects/<name>/` (Vorlage `_template`), Doku in `docs/04-eigene-projekte.md` |
| CI                             | `.github/workflows/build.yml`                                        |
| Konsistenzprüfung              | `scripts/check-docs.py`                                              |
| WLAN-Analyse-Modul             | Anleitung `docs/06-wlan-analyse.md`, Firmware `projects/wlan-sonde`, Auswertung `tools/wlan-analyse` (Python, Tests in `tests/`) |
| GitHub-Wiki                    | wird aus den Dateien oben erzeugt: `scripts/build-wiki.py`, veröffentlicht von `.github/workflows/wiki.yml` |

## 1. Fragen beantworten

1. Passende Board-Seite in `docs/boards/` lesen. Bei Pin-Fragen auch die JSON-Spec lesen,
   sie ist die maschinenlesbare Pinbelegung.
2. Für Build-Fragen `boards/boards.ini` und die `platformio.ini` des Projekts lesen.
3. Antwort mit Verweis auf die Datei geben (z. B. `docs/boards/esp32-devkitc.md`).
4. Steht etwas nicht in der Doku oder ist es veraltet, das sagen und anbieten, die Doku zu ergänzen.

## 2. Neues Board aufnehmen

Eingaben klären: genaue Board-Bezeichnung und Version, Chip/Modul (Aufdruck oder
`esptool.py flash_id`), Hersteller-Dokumentation.

1. **Recherche:** Datenblatt und Benutzerhandbuch des Herstellers. Pinbelegung nur aus
   Primärquellen (Hersteller, Schaltplan) oder klar benannten Sekundärquellen übernehmen.
   Unsicheres ausdrücklich als „zu prüfen“ markieren und nicht raten.
2. **Spec anlegen:** `docs/boards/specs/<board>.json`. Das Format steht im Kopf von
   `scripts/board-image.py`. Die Pins von oben (Antennenseite) nach unten eintragen, Typen:
   `power gnd gpio input strap flash ctrl uart usb`. Boards ohne Stiftleisten-Info
   bekommen `internal` (+ optional `matrix`).
3. **Bild erzeugen:** `python3 scripts/board-image.py docs/boards/specs/<board>.json`.
   Optional zur Sichtprüfung als PNG rendern:
   `/opt/pw-browsers/chromium --headless --no-sandbox --screenshot=/tmp/b.png --window-size=1000,1100 file://$PWD/docs/boards/img/<board>.svg`
   (oder einen anderen lokalen Chromium/Chrome verwenden).
4. **Seite schreiben:** `_vorlage.md` kopieren. Pflichtabschnitte: Bild,
   *Von der Familie zum Board* (Hersteller → Familie → Chip → Modul → Board → PlatformIO),
   Technische Daten, Pinbelegung, Verwendung im Repo, Typische Probleme, Quellen.
5. **Übersicht:** Zeile in der Tabelle in `docs/boards/README.md` ergänzen.
   Bei einer neuen Familie auch `docs/02-boards.md` (Vergleichstabelle) anpassen.
6. **PlatformIO:** Basis-Sektion `[<name>_base]` in `boards/boards.ini` mit Kommentar-Kopf
   und Verweis auf die Doku-Seite anlegen. Den passenden PlatformIO-Board-Namen mit
   `pio boards <suchwort>` ermitteln. Gibt es kein eigenes Profil, eines mit gleichem
   Chip/Flash/PSRAM nehmen und das auf der Board-Seite begründen.
7. **Projekte:** Wo sinnvoll, `[env:<name>]` in die Projekte eintragen (mindestens `_template`).
   Ein Board mit Spezialhardware bekommt ein eigenes Demo-Projekt unter `projects/`.
   Neue Projekte gehören in die `matrix.project`-Liste der CI.
8. **WLED** (wenn LED-tauglich): Env in `wled/platformio_override.ini` ergänzen, auf einer
   passenden WLED-Basis aufbauen und in `default_envs` sowie in der CI-Liste `matrix.env` eintragen.
9. **Prüfen** (siehe unten).

## 3. Updates

| Update                       | Was zu ändern ist                                                                 |
|------------------------------|-----------------------------------------------------------------------------------|
| Neue WLED-Version            | Tags prüfen: `git ls-remote --tags https://github.com/wled/WLED.git`. Dann `WLED_VERSION` in `scripts/wled-build.sh` ändern, den Text „Getestet mit WLED …“ in `wled/platformio_override.ini` anpassen und Versionsangaben in `wled/README.md`, `docs/03-wled.md` und `.github/workflows/build.yml` (Cache-Key hängt am Skript) aktualisieren. Prüfen, ob die Basis-Envs (`env:esp32dev`, `env:nodemcuv2`, `env:esp32s3_4M_qspi`) und Flags (`DATA_PINS`, `PIXEL_COUNTS` …) im neuen `platformio.ini` bzw. `platformio_override.sample.ini` noch existieren. |
| Neue PlatformIO-Plattform    | `platform = espressif32@…` / `espressif8266@…` in `boards/boards.ini`, danach alle Projekte bauen |
| Korrektur an einem Board     | Board-Seite **und** JSON-Spec ändern, Bild neu erzeugen                           |
| Neues Projekt                | `./scripts/new-project.sh <name>`, README im Projekt, Tabelle in `docs/04-eigene-projekte.md`, CI-Liste |
| Datenformat der WLAN-Analyse | `messungAlsJson()`/`messungAusJson()` in `projects/wlan-sonde/src/messung.cpp`, `messung_aus_dict()` in `tools/wlan-analyse/wlan_analyse/daten.py`, Testzeilen `FIRMWARE_*` in `tools/wlan-analyse/tests/test_wlan_analyse.py` und Tabelle „Datenformat“ in `docs/06-wlan-analyse.md` **immer gemeinsam** ändern |
| Bewertungsgrenzen (dBm)      | `STUFEN`/`ZIEL_DBM` in `tools/wlan-analyse/wlan_analyse/__init__.py`, `rssiBewertung()`/`farbe()` in der Firmware, Tabellen in `docs/06-wlan-analyse.md` und `projects/wlan-matrix/README.md` |
| Beispieldaten der Analyse    | `python3 tools/wlan-analyse/beispiel/erzeugen.py` ausführen und das Ergebnis mit einchecken (die CI vergleicht) |

Versionsangaben repo-weit finden: `grep -rn "v0\.1[0-9]\.\|espressif32@\|espressif8266@" --include=*.md --include=*.ini --include=*.sh --include=*.yml .`

## Prüfen vor dem Commit

```bash
python3 scripts/check-docs.py          # Links, Board-Übersicht, Bilder aktuell, Basen, CI-Liste
python3 scripts/build-wiki.py /tmp/wiki # Wiki-Erzeugung läuft durch?
./scripts/build-all.sh <projekt>       # wenn PlatformIO und Internet verfügbar
pio project config -d projects/<projekt>   # offline: Konfiguration auflösbar?
```

Ist die PlatformIO-Registry nicht erreichbar (z. B. in gesperrten Umgebungen), mindestens
`pio project config` ausführen und das Bauen der GitHub Action überlassen. Im Bericht
dann klar sagen, was wirklich kompiliert wurde und was nicht.

## WLAN-Abdeckung analysieren

Bei Fragen wie „Warum ist das WLAN in der Halle schlecht?“ oder „Wo stelle ich den Repeater hin?“:

1. Vorgehen und Maßnahmenkatalog aus `docs/06-wlan-analyse.md` nutzen.
2. Liegen Messdaten vor (`tools/wlan-analyse/daten/`, nicht im Git), den Bericht erzeugen:
   `cd tools/wlan-analyse && python3 -m wlan_analyse bericht -s daten/standort.json -m daten/*.jsonl --mesh daten/fritz-mesh.json -o daten/bericht.html --json daten/ergebnis.json`
   und die Befunde aus `daten/ergebnis.json` erklären.
3. Neue Prüfregeln gehören in `befunde()` in `wlan_analyse/analyse.py` – mit Test in `tests/` und Eintrag in der Tabelle
   „Was der Bericht prüft“ in `tools/wlan-analyse/README.md`.
4. Firmware-Änderungen an `wlan-sonde` lokal bauen (`pio run -d projects/wlan-sonde`); Python-Teil mit
   `python3 -m unittest discover -s tests` im Ordner `tools/wlan-analyse` prüfen.

## Wiki

Das Wiki ist **generiert**. Inhalte nie im Wiki selbst ändern, sondern im Repo.
Welche Dateien zu Wiki-Seiten werden, steht in `SOURCES` in `scripts/build-wiki.py`:
Root-README, `docs/0*.md`, `docs/boards/*.md`, `wled/**/README.md`, `projects/*/README.md` und `tools/*/README.md`.
Neue Board-Seiten und Projekte erscheinen automatisch. Für eine neue Doku-Kategorie
`SOURCES` ergänzen und die Workflow-`paths` prüfen.

Lokale Vorschau: `python3 scripts/build-wiki.py /tmp/wiki`. Die Ausgabe listet alle
Seiten mit ihrer Quelle. Links zwischen Doku-Dateien werden zu Wiki-Links, Bilder
landen in `images/`. Links auf Code zeigen auf GitHub (`blob/main`).

## Regeln

- Sprache der Doku: Deutsch, sachlich, Tabellen statt Fließtext, wo es passt.
- Keine Herstellerfotos einchecken (Urheberrecht). Bilder sind schematische SVGs aus
  `scripts/board-image.py` und gehören zur Quelle (JSON-Spec) dazu.
- Zugangsdaten nie einchecken (`secrets.h`, `wled/config/backup/` sind ignoriert).
- Pinangaben, die nicht aus einer Quelle belegt sind, als „zu prüfen“ kennzeichnen.
