"""Eingabedaten: Lageplan (standort.json) und Messungen (JSON Lines der wlan-sonde)."""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path


# ------------------------------------------------------------------ Lageplan
@dataclass
class Bereich:
    id: str
    name: str
    x: float
    y: float
    breite: float
    hoehe: float
    art: str = "gebaeude"          # gebaeude | halle | aussen

    def enthaelt(self, x: float, y: float) -> bool:
        return self.x <= x <= self.x + self.breite and self.y <= y <= self.y + self.hoehe


@dataclass
class Geraet:
    """Mesh-Gerät im Lageplan (FRITZ!Box, Repeater, Access Point)."""
    name: str
    bereich: str
    x: float
    y: float
    bssids: list[str] = field(default_factory=list)
    modell: str = ""


@dataclass
class Ort:
    """Position einer Sonde oder eines Messpunkts."""
    id: str
    bereich: str
    x: float
    y: float
    name: str = ""


@dataclass
class Standort:
    name: str
    ssid: str
    bereiche: list[Bereich]
    geraete: list[Geraet]
    sonden: dict[str, Ort]
    punkte: dict[str, Ort]

    def bereich(self, bid: str) -> Bereich | None:
        return next((b for b in self.bereiche if b.id == bid), None)

    def bereich_zuordnen(self, text: str | None, x: float | None = None, y: float | None = None) -> str | None:
        """Bereich-ID aus Position oder Freitext ("A", "Halle", "Gebäude B", ...)."""
        if x is not None and y is not None:
            for b in self.bereiche:
                if b.enthaelt(x, y):
                    return b.id
        if not text:
            return None
        t = text.strip().lower()
        for b in self.bereiche:
            if t in (b.id.lower(), b.name.lower()):
                return b.id
        if "hall" in t:
            return next((b.id for b in self.bereiche if b.art == "halle"), None)
        for b in self.bereiche:   # "Gebäude A" <-> "A", "geb-b" <-> "B"
            bid = b.id.lower()
            if len(bid) == 1 and (t.endswith(" " + bid) or t.endswith("-" + bid) or t == bid):
                return b.id
        return None


def _num(d: dict, key: str, standard: float = 0.0) -> float:
    try:
        return float(d.get(key, standard))
    except (TypeError, ValueError):
        return standard


def standort_laden(pfad: str | Path) -> Standort:
    d = json.loads(Path(pfad).read_text(encoding="utf-8"))
    bereiche = [Bereich(id=str(b["id"]), name=b.get("name", b["id"]), x=_num(b, "x"), y=_num(b, "y"),
                        breite=_num(b, "breite", 10), hoehe=_num(b, "hoehe", 10), art=b.get("art", "gebaeude"))
                for b in d.get("bereiche", [])]
    geraete = [Geraet(name=g["name"], bereich=g.get("bereich", ""), x=_num(g, "x"), y=_num(g, "y"),
                      bssids=[normalisiere_bssid(b) for b in g.get("bssids", []) if normalisiere_bssid(b)],
                      modell=g.get("modell", ""))
               for g in d.get("geraete", [])]
    sonden = {s["name"]: Ort(id=s["name"], bereich=s.get("bereich", ""), x=_num(s, "x"), y=_num(s, "y"),
                             name=s.get("ort", ""))
              for s in d.get("sonden", [])}
    punkte = {str(p["id"]): Ort(id=str(p["id"]), bereich=p.get("bereich", ""), x=_num(p, "x"), y=_num(p, "y"),
                                name=p.get("name", ""))
              for p in d.get("punkte", [])}
    return Standort(name=d.get("name", "Standort"), ssid=d.get("ssid", ""), bereiche=bereiche,
                    geraete=geraete, sonden=sonden, punkte=punkte)


# ---------------------------------------------------------------- Messungen
def normalisiere_bssid(text) -> str | None:
    """'3c-a6-2f-11-22-33' -> '3C:A6:2F:11:22:33'; ungültig -> None."""
    if not isinstance(text, str):
        return None
    teile = text.replace("-", ":").strip().upper().split(":")
    if len(teile) != 6 or not all(len(t) == 2 and all(c in "0123456789ABCDEF" for c in t) for t in teile):
        return None
    return ":".join(teile)


@dataclass
class Knoten:
    bssid: str
    rssi: int
    kanal: int


@dataclass
class Messung:
    """Eine Zeile im Exportformat der wlan-sonde (siehe docs/06-wlan-analyse.md)."""
    typ: str                      # "messung" (Sonde) | "punkt" (Begehung)
    sonde: str
    bereich: str
    ort: str
    punkt: str | None
    t: int                        # Unix-Zeit, 0 = unbekannt
    uptime: int
    intervall: int
    verbunden: bool
    bssid: str | None
    kanal: int | None
    rssi: int | None
    rssi_min: int | None
    rssi_max: int | None
    abbrueche: int
    wechsel: int
    antwort_ms: float | None
    verlust: int
    knoten: list[Knoten]
    fremd: list[dict]

    @property
    def bester(self) -> Knoten | None:
        return max(self.knoten, key=lambda k: k.rssi, default=None)

    @property
    def schluessel(self) -> tuple:
        return (self.typ, self.sonde, self.punkt, self.t, self.uptime)


def _int(v, standard=None):
    try:
        return int(v)
    except (TypeError, ValueError):
        return standard


def messung_aus_dict(d: dict) -> Messung:
    if not isinstance(d, dict) or not d.get("sonde"):
        raise ValueError("Feld 'sonde' fehlt")
    knoten = []
    for k in d.get("knoten") or []:
        b = normalisiere_bssid(k.get("bssid"))
        r = _int(k.get("rssi"))
        if b and r is not None:
            knoten.append(Knoten(bssid=b, rssi=r, kanal=_int(k.get("kanal"), 0)))
    verbunden = bool(d.get("verbunden"))
    antwort = d.get("antwort_ms")
    return Messung(
        typ=d.get("typ", "messung"),
        sonde=str(d["sonde"]),
        bereich=str(d.get("bereich") or ""),
        ort=str(d.get("ort") or ""),
        punkt=str(d["punkt"]) if d.get("punkt") not in (None, "") else None,
        t=_int(d.get("t"), 0),
        uptime=_int(d.get("uptime"), 0),
        intervall=_int(d.get("intervall"), 0),
        verbunden=verbunden,
        bssid=normalisiere_bssid(d.get("bssid")) if verbunden else None,
        kanal=_int(d.get("kanal")) if verbunden else None,
        rssi=_int(d.get("rssi")) if verbunden else None,
        rssi_min=_int(d.get("rssi_min")) if verbunden else None,
        rssi_max=_int(d.get("rssi_max")) if verbunden else None,
        abbrueche=_int(d.get("abbrueche"), 0),
        wechsel=_int(d.get("wechsel"), 0),
        antwort_ms=float(antwort) if isinstance(antwort, (int, float)) else None,
        verlust=_int(d.get("verlust"), 0),
        knoten=knoten,
        fremd=[f for f in d.get("fremd") or [] if isinstance(f, dict) and _int(f.get("kanal"))],
    )


@dataclass
class Ladeergebnis:
    messungen: list[Messung]
    fehlerhaft: int = 0
    doppelt: int = 0


def messungen_laden(pfade) -> Ladeergebnis:
    """Liest JSON-Lines-Dateien (Export der Sonden/Zentrale, Sammler). Doppelte Zeilen
    (z. B. aus mehreren Exporten) werden nur einmal übernommen."""
    erg = Ladeergebnis(messungen=[])
    gesehen = set()
    for pfad in pfade:
        for zeile in Path(pfad).read_text(encoding="utf-8").splitlines():
            zeile = zeile.strip()
            if not zeile:
                continue
            try:
                m = messung_aus_dict(json.loads(zeile))
            except (ValueError, TypeError, KeyError):
                erg.fehlerhaft += 1
                continue
            if m.schluessel in gesehen:
                erg.doppelt += 1
                continue
            gesehen.add(m.schluessel)
            erg.messungen.append(m)
    erg.messungen.sort(key=lambda m: (m.sonde, m.t or 0, m.uptime))
    return erg
