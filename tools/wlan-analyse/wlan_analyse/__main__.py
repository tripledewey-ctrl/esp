"""Kommandozeile: python3 -m wlan_analyse <befehl> …"""

from __future__ import annotations

import argparse
import getpass
import json
import os
import sys
import webbrowser
from dataclasses import asdict
from pathlib import Path

from . import __version__
from .analyse import auswerten
from .bericht import bericht_html
from .daten import messungen_laden, standort_laden
from .fritz import FritzFehler, mesh_abrufen, mesh_auswerten, mesh_laden, mesh_text
from .sammler import abrufen, sammeln

BEISPIEL = Path(__file__).resolve().parent.parent / "beispiel"


def cmd_bericht(a) -> int:
    standort = standort_laden(a.standort)
    geladen = messungen_laden(a.messungen)
    if geladen.fehlerhaft:
        print(f"Hinweis: {geladen.fehlerhaft} fehlerhafte Zeilen übersprungen", file=sys.stderr)
    mesh = mesh_laden(a.mesh, standort.ssid or None) if a.mesh else None
    erg = auswerten(standort, geladen.messungen, mesh)
    Path(a.ausgabe).write_text(bericht_html(erg), encoding="utf-8")
    print(f"Bericht: {a.ausgabe}  ({len(erg.punkte)} Messpunkte, {len(erg.sonden)} Sonden, "
          f"{len(erg.befunde)} Befunde)")
    for b in erg.befunde:
        if b.stufe != "hinweis":
            print(f"  [{b.stufe}] {b.titel}")
    if a.json:
        daten = asdict(erg)
        daten.pop("heatmap", None)
        Path(a.json).write_text(json.dumps(daten, ensure_ascii=False, indent=1, default=str), encoding="utf-8")
        print(f"Ergebnis als JSON: {a.json}")
    if a.oeffnen:
        webbrowser.open(Path(a.ausgabe).resolve().as_uri())
    return 0


def cmd_demo(a) -> int:
    a.standort = BEISPIEL / "standort.json"
    a.messungen = [BEISPIEL / "messungen.jsonl"]
    a.mesh = BEISPIEL / "fritz-mesh.json"
    return cmd_bericht(a)


def cmd_fritz(a) -> int:
    passwort = a.passwort or os.environ.get("FRITZ_PASSWORT") or getpass.getpass("FRITZ!Box-Passwort: ")
    try:
        roh = mesh_abrufen(a.adresse, a.benutzer, passwort, timeout=a.timeout)
    except FritzFehler as e:
        print(f"Fehler: {e}", file=sys.stderr)
        return 1
    Path(a.ausgabe).write_text(json.dumps(roh, ensure_ascii=False, indent=1), encoding="utf-8")
    print(mesh_text(mesh_auswerten(roh)))
    print(f"\nGespeichert: {a.ausgabe}  ->  im Bericht mit --mesh {a.ausgabe} verwenden")
    return 0


def cmd_mesh(a) -> int:
    print(mesh_text(mesh_laden(a.datei)))
    return 0


def cmd_abrufen(a) -> int:
    for url, n in abrufen(a.geraete, a.ausgabe).items():
        print(f"{url}: {n} Zeilen")
    print(f"angehängt an {a.ausgabe}")
    return 0


def cmd_sammeln(a) -> int:
    server = sammeln(a.port, a.ausgabe)
    print(f"Sammler läuft auf Port {a.port}, schreibt nach {a.ausgabe}.\n"
          f"In den Sonden als Zentrale eintragen: http://<IP dieses Rechners>:{a.port}  (Strg+C beendet)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    print("\n" + "\n".join(f"  {s}: {n} Messungen" for s, n in sorted(server.zaehler.items())))
    return 0


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="wlan_analyse", description="WLAN-Abdeckung auswerten (wlan-sonde + FRITZ!Box-Mesh).")
    p.add_argument("--version", action="version", version=__version__)
    sub = p.add_subparsers(dest="befehl", required=True)

    b = sub.add_parser("bericht", help="HTML-Bericht aus Messungen erstellen")
    b.add_argument("--standort", "-s", required=True, help="standort.json (Lageplan)")
    b.add_argument("--messungen", "-m", required=True, nargs="+", help="JSONL-Dateien (Export/Sammler)")
    b.add_argument("--mesh", help="Mesh-Liste der FRITZ!Box (Ausgabe von 'fritz')")
    b.add_argument("--ausgabe", "-o", default="bericht.html")
    b.add_argument("--json", help="Ergebnis zusätzlich als JSON speichern")
    b.add_argument("--oeffnen", action="store_true", help="Bericht im Browser öffnen")
    b.set_defaults(f=cmd_bericht)

    d = sub.add_parser("demo", help="Beispielbericht (zwei Gebäude mit Halle) erzeugen")
    d.add_argument("--ausgabe", "-o", default="beispiel-bericht.html")
    d.add_argument("--json")
    d.add_argument("--oeffnen", action="store_true")
    d.set_defaults(f=cmd_demo)

    f = sub.add_parser("fritz", help="Mesh-Topologie von der FRITZ!Box lesen (TR-064)")
    f.add_argument("--adresse", default="fritz.box")
    f.add_argument("--benutzer", default="", help="FRITZ!Box-Benutzer (Recht 'FRITZ!Box Einstellungen')")
    f.add_argument("--passwort", help="sonst Umgebungsvariable FRITZ_PASSWORT oder Abfrage")
    f.add_argument("--timeout", type=float, default=10)
    f.add_argument("--ausgabe", "-o", default="fritz-mesh.json")
    f.set_defaults(f=cmd_fritz)

    m = sub.add_parser("mesh", help="gespeicherte Mesh-Liste anzeigen")
    m.add_argument("datei")
    m.set_defaults(f=cmd_mesh)

    r = sub.add_parser("abrufen", help="/api/export von Geräten holen (z. B. wlan-zentrale.local)")
    r.add_argument("geraete", nargs="+", help="Adressen, z. B. wlan-zentrale.local sonde-mobil.local")
    r.add_argument("--ausgabe", "-o", default="messungen.jsonl")
    r.set_defaults(f=cmd_abrufen)

    s = sub.add_parser("sammeln", help="Messungen der Sonden auf diesem Rechner sammeln (statt ESP32-Zentrale)")
    s.add_argument("--port", type=int, default=8080)
    s.add_argument("--ausgabe", "-o", default="messungen.jsonl")
    s.set_defaults(f=cmd_sammeln)

    a = p.parse_args(argv)
    return a.f(a)


if __name__ == "__main__":
    sys.exit(main())
