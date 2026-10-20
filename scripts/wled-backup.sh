#!/usr/bin/env bash
# Sichert Konfiguration und Presets eines oder mehrerer WLED-Geräte.
#   ./scripts/wled-backup.sh 192.168.1.50 wled-kueche.local
#
# Ergebnis: wled/config/backup/<host>/cfg.json + presets.json
# Wiederherstellen: WLED-Oberfläche -> Config -> Security & Updates -> Restore
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ $# -gt 0 ]] || { echo "Aufruf: $0 <host> [host ...]" >&2; exit 1; }

for host in "$@"; do
  dest="$ROOT/wled/config/backup/$host"
  mkdir -p "$dest"
  echo "Sichere $host ..."
  curl -fsS --max-time 10 "http://$host/cfg.json"     -o "$dest/cfg.json"
  curl -fsS --max-time 10 "http://$host/presets.json" -o "$dest/presets.json"
  curl -fsS --max-time 10 "http://$host/json/info"    -o "$dest/info.json"
  echo "  -> $dest"
done
