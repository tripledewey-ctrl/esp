#!/usr/bin/env bash
# Flasht eine fertige .bin-Datei (z. B. WLED-Release) per USB mit esptool.
#   ./scripts/flash.sh <firmware.bin> [port]          -> nur Firmware (Adresse 0x0 bzw. 0x10000)
#   ERASE=1 ./scripts/flash.sh <firmware.bin> [port]  -> vorher kompletten Flash löschen
#
# Hinweis: Offizielle WLED-Releases für ESP32 sind App-Images (Adresse 0x10000).
# Für eine Erstinstallation auf ESP32 ist der Web-Installer https://install.wled.me
# am einfachsten, da er Bootloader und Partitionstabelle mitschreibt.
set -euo pipefail

BIN="${1:-}"; PORT="${2:-}"
[[ -f "$BIN" ]] || { echo "Aufruf: $0 <firmware.bin> [port]" >&2; exit 1; }

if command -v esptool.py >/dev/null; then ESPTOOL=(esptool.py)
elif command -v esptool >/dev/null; then ESPTOOL=(esptool)
else ESPTOOL=(python3 -m esptool); fi

PORTARG=(); [[ -n "$PORT" ]] && PORTARG=(--port "$PORT")

# Chip erkennen, um die Startadresse zu bestimmen
CHIP=$("${ESPTOOL[@]}" "${PORTARG[@]}" chip_id 2>/dev/null | grep -oE 'Chip is [^ ]+' | awk '{print $3}' || true)
case "$CHIP" in
  ESP8266*) ADDR=0x0 ;;
  "")       echo "Kein ESP gefunden – Port prüfen, ggf. BOOT-Taste beim Verbinden halten" >&2; exit 1 ;;
  *)        ADDR=0x10000 ;;
esac
echo "Erkannt: $CHIP -> schreibe $BIN an $ADDR"

[[ "${ERASE:-0}" == "1" ]] && "${ESPTOOL[@]}" "${PORTARG[@]}" erase_flash
"${ESPTOOL[@]}" "${PORTARG[@]}" --baud 460800 write_flash "$ADDR" "$BIN"
