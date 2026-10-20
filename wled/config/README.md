# WLED-Konfigurationen

| Datei / Ordner          | Inhalt                                                          |
|-------------------------|-----------------------------------------------------------------|
| `presets.example.json`  | Beispiel-Presets (Warmweiß, Regenbogen, Kaminfeuer, Nachtlicht) |
| `backup/<host>/`        | Sicherungen von `scripts/wled-backup.sh` (nicht eingecheckt)    |

## Presets einspielen

WLED-Oberfläche → **Config** → **Security & Updates** → **Restore presets**
→ `presets.example.json` auswählen.

Oder per Kommandozeile (überschreibt alle Presets!):

```bash
curl -F "data=@presets.example.json;filename=/presets.json" http://<wled-ip>/upload
```

## Gerätekonfiguration sichern & wiederherstellen

```bash
./scripts/wled-backup.sh 192.168.1.50 wled-kueche.local
```

Wiederherstellen über **Config → Security & Updates → Restore configuration**
(`cfg.json`). Achtung: `cfg.json` enthält WLAN-Zugangsdaten – deshalb ist
`backup/` in `.gitignore` eingetragen.
