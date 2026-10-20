#!/usr/bin/env bash
# Baut eine eigene WLED-Firmware mit wled/platformio_override.ini.
#   ./scripts/wled-build.sh                 -> alle Envs aus der Override-Datei
#   ./scripts/wled-build.sh wled_esp32      -> nur ein Env
#   WLED_VERSION=v0.15.5 ./scripts/wled-build.sh
#
# Ergebnis: wled/build/bin/*.bin (zum Flashen per Web-Installer, esptool oder OTA)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WLED_VERSION="${WLED_VERSION:-v0.15.5}"
SRC="$ROOT/wled/build/WLED-$WLED_VERSION"
OUT="$ROOT/wled/build/bin"

command -v pio >/dev/null || { echo "PlatformIO (pio) nicht gefunden" >&2; exit 1; }
command -v npm >/dev/null || { echo "Node.js/npm wird für die WLED-Weboberfläche benötigt" >&2; exit 1; }

if [[ ! -d "$SRC" ]]; then
  echo "Lade WLED $WLED_VERSION ..."
  git clone --depth 1 --branch "$WLED_VERSION" https://github.com/wled/WLED.git "$SRC"
  (cd "$SRC" && npm ci)
fi

cp "$ROOT/wled/platformio_override.ini" "$SRC/platformio_override.ini"

if [[ $# -gt 0 ]]; then
  args=(); for e in "$@"; do args+=(-e "$e"); done
  pio run -d "$SRC" "${args[@]}"
else
  pio run -d "$SRC"
fi

mkdir -p "$OUT"
find "$SRC/build_output/release" -name '*.bin' -exec cp {} "$OUT/" \; 2>/dev/null || true
find "$SRC/.pio/build" -maxdepth 2 -name firmware.bin | while read -r f; do
  env="$(basename "$(dirname "$f")")"
  cp "$f" "$OUT/$env.bin"
done
echo "Firmware-Dateien:"
ls -1 "$OUT"
