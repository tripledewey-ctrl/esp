# Projektvorlage

Neues Projekt anlegen (aus dem Repo-Root):

```bash
./scripts/new-project.sh mein-projekt
cd projects/mein-projekt
pio run -e esp32dev -t upload -t monitor
```

Zugangsdaten: `include/secrets.example.h` nach `include/secrets.h` kopieren und anpassen.
