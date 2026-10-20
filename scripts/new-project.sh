#!/usr/bin/env bash
# Legt ein neues Projekt aus projects/_template an.
#   ./scripts/new-project.sh <name>
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NAME="${1:-}"

if [[ -z "$NAME" || ! "$NAME" =~ ^[a-z0-9][a-z0-9_-]*$ ]]; then
  echo "Aufruf: $0 <projektname>   (nur a-z, 0-9, - und _)" >&2
  exit 1
fi

DEST="$ROOT/projects/$NAME"
if [[ -e "$DEST" ]]; then
  echo "Projekt existiert bereits: $DEST" >&2
  exit 1
fi

cp -r "$ROOT/projects/_template" "$DEST"
cp "$DEST/include/secrets.example.h" "$DEST/include/secrets.h"
sed -i.bak "s/esp-projekt/$NAME/" "$DEST/include/secrets.h" && rm -f "$DEST/include/secrets.h.bak"
sed -i.bak "1s/.*/; $NAME/" "$DEST/platformio.ini" && rm -f "$DEST/platformio.ini.bak"
sed -i.bak "s/^# Projektvorlage$/# $NAME/" "$DEST/README.md" && rm -f "$DEST/README.md.bak"

echo "Projekt angelegt: projects/$NAME"
echo "  1. WLAN-Daten in projects/$NAME/include/secrets.h eintragen"
echo "  2. cd projects/$NAME && pio run -e esp32dev -t upload -t monitor"
