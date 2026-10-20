# 1 – Einrichtung der Entwicklungsumgebung

## PlatformIO installieren

PlatformIO ist das Build-System für alle eigenen Projekte in `projects/` und für
eigene WLED-Builds. Es lädt Compiler, Frameworks und Bibliotheken automatisch.

### Variante A – Visual Studio Code (empfohlen)

1. [VS Code](https://code.visualstudio.com) installieren.
2. Erweiterung **PlatformIO IDE** installieren (VS Code schlägt sie beim Öffnen
   des Repos über `.vscode/extensions.json` vor).
3. Einen Projektordner öffnen, z. B. `projects/blink`
   (**Datei → Ordner öffnen**). Die blaue Leiste unten zeigt Build ✓,
   Upload → und Monitor 🔌.

> PlatformIO erkennt nur Ordner mit einer `platformio.ini` als Projekt.
> Mehrere Projekte gleichzeitig: **PlatformIO → Projects → Add existing**.

### Variante B – Kommandozeile

```bash
python3 -m pip install --user platformio     # oder: pipx install platformio
pio --version
```

Unter Windows funktioniert das genauso in der PowerShell; die `.sh`-Skripte
laufen dort in **Git Bash** oder **WSL**.

## USB-Treiber

Die meisten Boards haben einen USB-Seriell-Wandler:

| Chip auf dem Board | Typische Boards                       | Treiber                                                            |
|--------------------|---------------------------------------|--------------------------------------------------------------------|
| CH340 / CH9102     | NodeMCU v3, D1 mini, viele ESP32-Klone| [WCH](https://www.wch-ic.com/downloads/CH341SER_ZIP.html)          |
| CP2102 / CP2104    | ESP32-DevKitC, NodeMCU v2             | [Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) |
| nativ (USB-CDC)    | ESP32-S3, ESP32-C3                    | kein Treiber nötig                                                 |

Linux und aktuelles macOS bringen die Treiber meist mit.

### Linux: Zugriffsrechte

```bash
sudo usermod -aG dialout $USER     # danach ab- und wieder anmelden
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules \
  | sudo tee /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Unter Ubuntu kann `brltty` CH340-Adapter „klauen“: `sudo apt remove brltty`.

## Port finden

```bash
pio device list
```

| System  | Beispiel                       |
|---------|--------------------------------|
| Linux   | `/dev/ttyUSB0`, `/dev/ttyACM0` |
| macOS   | `/dev/cu.usbserial-0001`       |
| Windows | `COM3`                         |

PlatformIO findet den Port normalerweise automatisch. Sonst in der
`platformio.ini` des Projekts `upload_port = /dev/ttyUSB0` ergänzen.

## Erster Test

```bash
cd projects/blink
pio run -e esp32dev -t upload -t monitor      # ESP32
pio run -e nodemcuv2 -t upload -t monitor     # ESP8266
```

Monitor beenden: `Strg + C`. Weiter mit [02 – Boards](02-boards.md).
