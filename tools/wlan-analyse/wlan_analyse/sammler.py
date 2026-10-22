"""Messungen sammeln: als Ersatz für die ESP32-Zentrale (HTTP-Server) oder per Abruf vom Gerät."""

from __future__ import annotations

import json
import threading
import time
import urllib.request
from collections import Counter
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from .daten import messung_aus_dict


def abrufen(urls: list[str], ziel: str | Path, timeout: float = 15) -> dict[str, int]:
    """Holt /api/export von Sonden, Mobilgerät oder Zentrale und hängt die Zeilen an `ziel` an.
    Doppelte Zeilen stören nicht – messungen_laden() übernimmt jede Messung nur einmal."""
    oeffner = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    zaehler = {}
    with open(ziel, "a", encoding="utf-8") as f:
        for url in urls:
            basis = url if url.startswith("http") else f"http://{url}"
            daten = oeffner.open(basis.rstrip("/") + "/api/export", timeout=timeout).read().decode("utf-8")
            n = 0
            for zeile in daten.splitlines():
                zeile = zeile.strip()
                if zeile:
                    messung_aus_dict(json.loads(zeile))      # nur gültige Zeilen übernehmen
                    f.write(zeile + "\n")
                    n += 1
            zaehler[basis] = n
    return zaehler


class Sammler(ThreadingHTTPServer):
    """Nimmt POST /api/sonde wie die ESP32-Zentrale entgegen und schreibt JSON Lines."""

    daemon_threads = True

    def __init__(self, adresse, ziel: str | Path):
        super().__init__(adresse, _Handler)
        self.ziel = Path(ziel)
        self.sperre = threading.Lock()
        self.zaehler: Counter = Counter()
        self.zuletzt: dict[str, float] = {}


class _Handler(BaseHTTPRequestHandler):
    server: Sammler

    def log_message(self, fmt, *args):                       # ruhige Konsole
        pass

    def _antwort(self, code: int, text: str = "", typ: str = "text/plain; charset=utf-8"):
        daten = text.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", typ)
        self.send_header("Content-Length", str(len(daten)))
        self.end_headers()
        self.wfile.write(daten)

    def do_POST(self):
        if self.path.rstrip("/") != "/api/sonde":
            return self._antwort(404, "Nicht gefunden")
        laenge = int(self.headers.get("Content-Length") or 0)
        if laenge <= 0 or laenge > 65536:
            return self._antwort(400, "Länge ungültig")
        try:
            d = json.loads(self.rfile.read(laenge).decode("utf-8"))
            m = messung_aus_dict(d)
        except (ValueError, TypeError, KeyError) as e:
            return self._antwort(400, f"ungültig: {e}")
        if not d.get("t"):
            d["t"] = int(time.time())                       # Sonde ohne Uhrzeit: Empfangszeit
        d["empfangen"] = int(time.time())
        with self.server.sperre:
            with open(self.server.ziel, "a", encoding="utf-8") as f:
                f.write(json.dumps(d, ensure_ascii=False, separators=(",", ":")) + "\n")
            self.server.zaehler[m.sonde] += 1
            self.server.zuletzt[m.sonde] = time.time()
        self._antwort(204)

    def do_GET(self):
        if self.path.rstrip("/") in ("", "/api/status"):
            jetzt = time.time()
            status = {name: {"messungen": n, "vor_s": round(jetzt - self.server.zuletzt[name])}
                      for name, n in sorted(self.server.zaehler.items())}
            return self._antwort(200, json.dumps({"rolle": "sammler", "datei": str(self.server.ziel),
                                                  "sonden": status}, ensure_ascii=False, indent=1),
                                 "application/json; charset=utf-8")
        return self._antwort(404, "Nicht gefunden")


def sammeln(port: int, ziel: str | Path, host: str = "0.0.0.0") -> Sammler:
    return Sammler((host, port), ziel)
