#!/usr/bin/env bash
# Baut alle Projekte unter projects/ (alle Umgebungen) – praktisch vor einem Commit.
#   ./scripts/build-all.sh            -> alle Projekte
#   ./scripts/build-all.sh blink      -> nur ein Projekt
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
command -v pio >/dev/null || { echo "PlatformIO (pio) nicht gefunden – siehe docs/01-einrichtung.md" >&2; exit 1; }

if [[ $# -gt 0 ]]; then
  PROJECTS=("$@")
else
  PROJECTS=()
  for d in "$ROOT"/projects/*/; do
    [[ -f "$d/platformio.ini" ]] && PROJECTS+=("$(basename "$d")")
  done
fi

failed=()
for p in "${PROJECTS[@]}"; do
  dir="$ROOT/projects/$p"
  echo "=== $p ==="
  # Alle [env:...] eines Projekts bauen, OTA-Envs sind identisch zu ihrer Basis
  envs=$(grep -oE '^\[env:[^]]+\]' "$dir/platformio.ini" | sed -E 's/\[env:(.*)\]/\1/' | grep -v '_ota$' || true)
  for e in $envs; do
    if pio run -d "$dir" -e "$e" -s; then
      echo "  ok      $p:$e"
    else
      echo "  FEHLER  $p:$e"
      failed+=("$p:$e")
    fi
  done
done

if [[ ${#failed[@]} -gt 0 ]]; then
  echo "Fehlgeschlagen: ${failed[*]}" >&2
  exit 1
fi
echo "Alle Builds erfolgreich."
