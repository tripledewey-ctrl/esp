"""Auswertung: Abdeckung je Bereich, Messpunkte, Sonden, Mesh-Anbindung, Kanäle, Befunde."""

from __future__ import annotations

import math
import re
import statistics
from collections import Counter, defaultdict
from dataclasses import dataclass, field

from . import STICKY_DB, ZIEL_DBM, stufe
from .daten import Messung, Standort
from .fritz import Mesh

KRITISCH, WARNUNG, HINWEIS = "kritisch", "warnung", "hinweis"


# ---------------------------------------------------------------- Ergebnisse
@dataclass
class Zuordnung:
    geraet: str
    quelle: str            # fritz | standort | abgeleitet | geschätzt | unbekannt


@dataclass
class PunktErgebnis:
    id: str
    bereich: str | None
    x: float | None
    y: float | None
    anzahl: int
    rssi: int | None                 # verbunden
    geraet: str | None               # verbunden mit
    bester_rssi: int | None
    bester_geraet: str | None
    antwort_ms: float | None
    verlust: float
    klebt: bool                      # verbunden deutlich schwächer als bester Knoten
    knoten: dict[str, int]           # Gerät -> RSSI (Median)


@dataclass
class SondenErgebnis:
    name: str
    bereich: str | None
    ort: str
    x: float | None
    y: float | None
    anzahl: int
    zeitraum_s: int
    verbunden_anteil: float
    rssi_median: int | None
    rssi_p10: int | None
    rssi_min: int | None
    bester_median: int | None
    geraete: dict[str, float]        # verbunden mit Gerät -> Anteil
    bester_geraet: str | None
    klebt_anteil: float
    abbrueche: int
    wechsel: int
    antwort_median: float | None
    verlust_mittel: float
    verlauf: list[tuple[float, int | None]]   # (Zeit in s ab Beginn, RSSI)
    rssi_am_ap: int | None = None             # Rückrichtung laut FRITZ!Box


@dataclass
class BereichErgebnis:
    id: str
    name: str
    art: str
    punkte: int
    anteil_ziel: float | None        # Anteil Punkte mit bestem Knoten >= ZIEL_DBM
    anteil_schwach: float | None     # Anteil Punkte < -80 dBm
    median: int | None
    minimum: int | None
    sonden: list[str]
    bewertung: str                   # gut | ausreichend | mangelhaft | keine Daten


@dataclass
class Befund:
    stufe: str
    titel: str
    text: str
    massnahme: str = ""
    bereich: str | None = None


@dataclass
class Ergebnis:
    standort: Standort
    zuordnung: dict[str, Zuordnung]
    punkte: list[PunktErgebnis]
    sonden: list[SondenErgebnis]
    bereiche: list[BereichErgebnis]
    kanaele: dict[str, dict]
    mesh: Mesh | None
    befunde: list[Befund]
    zeitraum: tuple[int, int] | None
    anzahl_messungen: int
    gesehen: list[str] = field(default_factory=list)   # BSSIDs, die in Messungen vorkommen
    heatmap: dict[str, list[tuple[float, float, float, float, int]]] = field(default_factory=dict)

    def geraet(self, bssid: str | None) -> str | None:
        if not bssid:
            return None
        z = self.zuordnung.get(bssid)
        return z.geraet if z else bssid


# ------------------------------------------------------------- Hilfsfunktionen
def _median(werte):
    werte = [w for w in werte if w is not None]
    return round(statistics.median(werte)) if werte else None


def _perzentil(werte, p):
    werte = sorted(w for w in werte if w is not None)
    if not werte:
        return None
    k = (len(werte) - 1) * p
    u, o = math.floor(k), math.ceil(k)
    return round(werte[u] + (werte[o] - werte[u]) * (k - u))


def _geraete_schluessel(bssid: str) -> str:
    """Gleiche Hardware: erste fünf Bytes, ohne 'lokal verwaltet'-Bit (Gast-/Zusatz-BSSIDs)."""
    teile = bssid.split(":")
    teile[0] = f"{int(teile[0], 16) & 0xFD:02X}"
    return ":".join(teile[:5])


def _zaehler_zuwachs(werte):
    """Summe der Zuwächse eines Zählers, der bei einem Neustart wieder bei 0 beginnt."""
    summe, vorher = 0, None
    for w in werte:
        if vorher is not None:
            summe += w - vorher if w >= vorher else w
        vorher = w
    return summe


def natuerlich(text: str):
    """Sortierschlüssel, der Zahlen als Zahlen behandelt: H-2 vor H-10."""
    return [int(t) if t.isdigit() else t.lower() for t in re.split(r"(\d+)", text)]


def kanal_ueberlappt(a: int, b: int) -> bool:
    """2,4 GHz, 20 MHz breit: Kanäle mit weniger als 5 Kanälen Abstand stören sich."""
    return a != b and abs(a - b) < 5


# ---------------------------------------------------------------- Zuordnung
def bssids_zuordnen(standort: Standort, messungen: list[Messung], mesh: Mesh | None) -> dict[str, Zuordnung]:
    zuordnung: dict[str, Zuordnung] = {}
    if mesh:
        for g in mesh.geraete:
            for b in g.bssids:
                zuordnung[b] = Zuordnung(_geraetename(standort, g.name), "fritz")
    for g in standort.geraete:
        for b in g.bssids:
            zuordnung.setdefault(b, Zuordnung(g.name, "standort"))

    gesehen = {k.bssid for m in messungen for k in m.knoten} | {m.bssid for m in messungen if m.bssid}
    bekannte = {_geraete_schluessel(b): z.geraet for b, z in zuordnung.items()}
    offen = []
    for b in sorted(gesehen):
        if b in zuordnung:
            continue
        if _geraete_schluessel(b) in bekannte:
            zuordnung[b] = Zuordnung(bekannte[_geraete_schluessel(b)], "abgeleitet")
        else:
            offen.append(b)

    # Noch offene BSSIDs: Gerät ohne bekannte BSSID, das am Ort des stärksten Empfangs steht
    ohne = [g for g in standort.geraete if g.name not in {z.geraet for z in zuordnung.values()}]
    for b in offen:
        bester = None
        for m in messungen:
            pos = _position(standort, m)
            for k in m.knoten:
                if k.bssid == b and pos and (bester is None or k.rssi > bester[0]):
                    bester = (k.rssi, pos)
        if bester and ohne:
            (x, y) = bester[1]
            g = min(ohne, key=lambda g: (g.x - x) ** 2 + (g.y - y) ** 2)
            if (g.x - x) ** 2 + (g.y - y) ** 2 <= 20 ** 2:
                zuordnung[b] = Zuordnung(g.name, "geschätzt")
                ohne.remove(g)
                continue
        zuordnung[b] = Zuordnung(f"unbekannt {b[-5:]}", "unbekannt")
    return zuordnung


def _geraetename(standort: Standort, name: str) -> str:
    """Name aus der FRITZ!Box auf ein Gerät im Lageplan abbilden (gleich oder enthalten)."""
    n = name.lower()
    for g in standort.geraete:
        if g.name.lower() == n:
            return g.name
    for g in standort.geraete:
        if n in g.name.lower() or g.name.lower() in n:
            return g.name
    return name


def _position(standort: Standort, m: Messung):
    ort = standort.punkte.get(m.punkt) if m.typ == "punkt" and m.punkt else standort.sonden.get(m.sonde)
    return (ort.x, ort.y) if ort else None


# ---------------------------------------------------------------- Auswertung
def auswerten(standort: Standort, messungen: list[Messung], mesh: Mesh | None = None) -> Ergebnis:
    zuordnung = bssids_zuordnen(standort, messungen, mesh)
    name = lambda b: zuordnung[b].geraet if b in zuordnung else b  # noqa: E731

    # --- Messpunkte (Begehung), mehrere Messungen je Punkt werden zusammengefasst
    je_punkt = defaultdict(list)
    for m in messungen:
        if m.typ == "punkt" and m.punkt:
            je_punkt[m.punkt].append(m)
    punkte = []
    for pid, ms in je_punkt.items():
        ort = standort.punkte.get(pid)
        knoten = defaultdict(list)
        for m in ms:
            for k in m.knoten:
                knoten[name(k.bssid)].append(k.rssi)
        knoten_median = {g: _median(w) for g, w in knoten.items()}
        bester_geraet = max(knoten_median, key=knoten_median.get) if knoten_median else None
        verbunden = [m for m in ms if m.verbunden]
        rssi = _median(m.rssi for m in verbunden)
        geraet = Counter(name(m.bssid) for m in verbunden).most_common(1)[0][0] if verbunden else None
        bester_rssi = knoten_median.get(bester_geraet) if bester_geraet else rssi
        if bester_rssi is None or (rssi is not None and rssi > bester_rssi):
            bester_rssi, bester_geraet = rssi, geraet
        punkte.append(PunktErgebnis(
            id=pid,
            bereich=standort.bereich_zuordnen(ort.bereich if ort else ms[-1].bereich,
                                              ort.x if ort else None, ort.y if ort else None)
            or standort.bereich_zuordnen(ms[-1].bereich),
            x=ort.x if ort else None, y=ort.y if ort else None, anzahl=len(ms),
            rssi=rssi, geraet=geraet, bester_rssi=bester_rssi, bester_geraet=bester_geraet,
            antwort_ms=_median_f(m.antwort_ms for m in ms),
            verlust=round(statistics.mean(m.verlust for m in ms), 1),
            klebt=bool(rssi is not None and bester_rssi is not None and geraet != bester_geraet
                       and bester_rssi - rssi >= STICKY_DB),
            knoten=knoten_median))
    punkte.sort(key=lambda p: (p.bereich or "", natuerlich(p.id)))

    # --- Sonden (Dauermessung)
    je_sonde = defaultdict(list)
    for m in messungen:
        if m.typ != "punkt":
            je_sonde[m.sonde].append(m)
    clients = {c.name.lower(): c for c in mesh.clients} if mesh else {}
    sonden = []
    alle_t = [m.t for m in messungen if m.t]
    for sname, ms in sorted(je_sonde.items()):
        ort = standort.sonden.get(sname)
        verbunden = [m for m in ms if m.verbunden]
        bester = [m.bester.rssi for m in ms if m.bester]
        klebt = [m for m in verbunden if m.bester and m.bester.bssid != m.bssid
                 and m.bester.rssi - (m.rssi or 0) >= STICKY_DB]
        geraete = Counter(name(m.bssid) for m in verbunden)
        bester_g = Counter(name(m.bester.bssid) for m in ms if m.bester).most_common(1)
        t0 = next((m.t for m in ms if m.t), 0)
        verlauf = [((m.t - t0) if (m.t and t0) else m.uptime - ms[0].uptime, m.rssi) for m in ms]
        zeitraum = int(max(v[0] for v in verlauf) - min(v[0] for v in verlauf)) if verlauf else 0
        client = clients.get(sname.lower())
        sonden.append(SondenErgebnis(
            name=sname,
            bereich=standort.bereich_zuordnen(ort.bereich if ort else ms[-1].bereich,
                                              ort.x if ort else None, ort.y if ort else None)
            or standort.bereich_zuordnen(ms[-1].bereich),
            ort=(ort.name if ort and ort.name else ms[-1].ort),
            x=ort.x if ort else None, y=ort.y if ort else None,
            anzahl=len(ms), zeitraum_s=zeitraum,
            verbunden_anteil=len(verbunden) / len(ms),
            rssi_median=_median(m.rssi for m in verbunden),
            rssi_p10=_perzentil([m.rssi for m in verbunden], 0.1),
            rssi_min=min((m.rssi_min if m.rssi_min is not None else m.rssi for m in verbunden), default=None),
            bester_median=_median(bester),
            geraete={g: n / len(verbunden) for g, n in geraete.most_common()} if verbunden else {},
            bester_geraet=bester_g[0][0] if bester_g else None,
            klebt_anteil=len(klebt) / len(verbunden) if verbunden else 0.0,
            abbrueche=_zaehler_zuwachs([m.abbrueche for m in ms]),
            wechsel=_zaehler_zuwachs([m.wechsel for m in ms]),
            antwort_median=_median_f(m.antwort_ms for m in ms),
            # Verlust nur über verbundene Messungen – Ausfälle zählen schon als Abbruch
            verlust_mittel=round(statistics.mean(m.verlust for m in verbunden), 1) if verbunden else 100.0,
            verlauf=verlauf,
            rssi_am_ap=client.rssi_am_ap if client else None))

    # --- Bereiche
    bereiche = []
    for b in standort.bereiche:
        werte = [p.bester_rssi for p in punkte if p.bereich == b.id and p.bester_rssi is not None]
        werte += [s.bester_median for s in sonden if s.bereich == b.id and s.bester_median is not None]
        anteil = sum(w >= ZIEL_DBM for w in werte) / len(werte) if werte else None
        schwach = sum(w < -80 for w in werte) / len(werte) if werte else None
        if anteil is None:
            bewertung = "keine Daten"
        elif anteil >= 0.9 and not schwach:
            bewertung = "gut"
        elif anteil >= 0.7:
            bewertung = "ausreichend"
        else:
            bewertung = "mangelhaft"
        bereiche.append(BereichErgebnis(
            id=b.id, name=b.name, art=b.art, punkte=len(werte), anteil_ziel=anteil, anteil_schwach=schwach,
            median=_median(werte), minimum=min(werte) if werte else None,
            sonden=[s.name for s in sonden if s.bereich == b.id], bewertung=bewertung))

    # --- Kanäle
    eigene = Counter()
    for m in messungen:
        for k in m.knoten:
            eigene[k.kanal] += 1
    fremd: dict[int, dict] = {}
    for m in messungen:
        for f in m.fremd:
            ch = int(f.get("kanal", 0))
            if not 1 <= ch <= 13:
                continue
            e = fremd.setdefault(ch, {"anzahl": 0, "max": -127})
            e["anzahl"] = max(e["anzahl"], int(f.get("anzahl", 0)))
            e["max"] = max(e["max"], int(f.get("max", -127)))
    kanaele = {"eigen": dict(eigene), "fremd": fremd}

    erg = Ergebnis(standort=standort, zuordnung=zuordnung, punkte=punkte, sonden=sonden, bereiche=bereiche,
                   kanaele=kanaele, mesh=mesh, befunde=[],
                   zeitraum=(min(alle_t), max(alle_t)) if alle_t else None, anzahl_messungen=len(messungen))
    erg.gesehen = sorted({k.bssid for m in messungen for k in m.knoten} | {m.bssid for m in messungen if m.bssid})
    erg.heatmap = heatmap(standort, punkte, sonden)
    erg.befunde = befunde(erg)
    return erg


def _median_f(werte):
    werte = [w for w in werte if w is not None]
    return round(statistics.median(werte), 1) if werte else None


# ------------------------------------------------------------------ Heatmap
def heatmap(standort: Standort, punkte, sonden, zelle: float = 1.0):
    """Inverse-Distanz-Interpolation des besten RSSI je Bereich (nur aus Messwerten desselben
    Bereichs – Wände trennen). Ergebnis: Bereich -> [(x, y, b, h, rssi)]."""
    erg = {}
    for b in standort.bereiche:
        stuetz = [(p.x, p.y, p.bester_rssi) for p in punkte
                  if p.bereich == b.id and p.x is not None and p.bester_rssi is not None]
        stuetz += [(s.x, s.y, s.bester_median) for s in sonden
                   if s.bereich == b.id and s.x is not None and s.bester_median is not None]
        if len(stuetz) < 2:
            continue
        schritt = max(zelle, max(b.breite, b.hoehe) / 60)
        zellen = []
        y = b.y
        while y < b.y + b.hoehe - 1e-9:
            h = min(schritt, b.y + b.hoehe - y)
            x = b.x
            while x < b.x + b.breite - 1e-9:
                w = min(schritt, b.x + b.breite - x)
                cx, cy = x + w / 2, y + h / 2
                summe = gewicht = 0.0
                for sx, sy, r in stuetz:
                    d2 = (sx - cx) ** 2 + (sy - cy) ** 2
                    g = 1.0 / max(d2, 0.25)
                    summe += g * r
                    gewicht += g
                zellen.append((x, y, w, h, round(summe / gewicht)))
                x += schritt
            y += schritt
        erg[b.id] = zellen
    return erg


# ------------------------------------------------------------------ Befunde
def _prozent(a):
    return f"{a * 100:.0f} %"


def befunde(e: Ergebnis) -> list[Befund]:
    liste: list[Befund] = []
    st = e.standort
    namen = {b.id: b.name for b in st.bereiche}

    if not e.punkte and not e.sonden:
        liste.append(Befund(KRITISCH, "Keine Messdaten",
                            "Es wurden keine verwertbaren Messungen gefunden.",
                            "Messungen mit der wlan-sonde aufnehmen (Begehung oder feste Sonden) "
                            "und als JSONL exportieren."))
        return liste

    # Abdeckung je Bereich
    gut = {b.id for b in e.bereiche if b.bewertung == "gut"}
    for b in e.bereiche:
        if b.anteil_ziel is None:
            liste.append(Befund(HINWEIS, f"{b.name}: keine Messwerte",
                                "Für diesen Bereich liegen weder Messpunkte noch Sonden vor.",
                                "Bei der nächsten Begehung Messpunkte im Raster von 5–10 m aufnehmen.", b.id))
            continue
        if b.bewertung == "gut":
            continue
        stufe_ = KRITISCH if b.anteil_ziel < 0.5 else WARNUNG
        text = (f"Nur {_prozent(b.anteil_ziel)} der Messstellen erreichen {ZIEL_DBM} dBm "
                f"(Median {b.median} dBm, schwächste {b.minimum} dBm).")
        if b.art == "halle":
            andere = [x for x in e.bereiche if x.id != b.id and x.art != "halle"]
            zwischen = andere and all(x.id in gut for x in andere)
            massnahme = ("Eigenen Mesh-Knoten in der Halle vorsehen: FRITZ!Repeater oder Access Point "
                         "zentral und erhöht (3–4 m) montieren, möglichst per LAN-Kabel angebunden "
                         "(FRITZ!Repeater: Betriebsart „LAN-Brücke“). Stahlbau, Sandwichpaneele, Regale "
                         "und Rolltore dämpfen stark – Funk aus den Nachbargebäuden reicht selten bis in "
                         "die Hallenmitte.")
            if zwischen:
                text += " Die Gebäude daneben sind gut versorgt – die Lücke liegt genau in der Halle."
        else:
            massnahme = ("Zusätzlichen Repeater im schwach versorgten Teil des Gebäudes aufstellen. Der "
                         "Repeater selbst sollte am Aufstellort noch mindestens −65 dBm von seinem "
                         "Elternknoten empfangen – oder per LAN angebunden werden.")
        liste.append(Befund(stufe_, f"{b.name}: Abdeckung {b.bewertung}", text, massnahme, b.id))

    # Funklöcher
    loecher = [p for p in e.punkte if p.bester_rssi is None or p.bester_rssi < -80]
    if loecher:
        je = defaultdict(list)
        for p in loecher:
            je[namen.get(p.bereich, p.bereich or "?")].append(f"{p.id} ({p.bester_rssi if p.bester_rssi is not None else '–'} dBm)")
        liste.append(Befund(KRITISCH, f"{len(loecher)} Funkloch-Messpunkte (< −80 dBm)",
                            "; ".join(f"{b}: {', '.join(v)}" for b, v in je.items()),
                            "Hier brechen Verbindungen ab. Mesh-Knoten so verschieben oder ergänzen, dass "
                            "an diesen Stellen mindestens −72 dBm erreicht werden."))

    # Mesh-Anbindung (FRITZ!Box-Daten)
    if e.mesh:
        for g in e.mesh.geraete:
            a = g.anbindung
            if not a:
                continue
            if a.art == "WLAN" and a.rate is not None and a.rate < 150:
                liste.append(Befund(
                    KRITISCH if a.rate < 60 else WARNUNG,
                    f"{_geraetename(st, g.name)}: schwache Funkanbindung ({a.rate:.0f} Mbit/s)",
                    f"Der Repeater ist per WLAN ({a.band or 'Funk'}) an {_geraetename(st, a.zu)} angebunden; "
                    f"aktuell nur {a.rate:.0f} Mbit/s. Alles, was über diesen Knoten läuft, ist dadurch "
                    "begrenzt – oft schlechter als das angezeigte Signal vermuten lässt.",
                    "Repeater näher an den Elternknoten setzen (Ziel: ≥ 300 Mbit/s auf der Funkstrecke) oder "
                    "per LAN-Kabel anbinden (LAN-Brücke). Zwischen zwei Gebäuden ist ein Kabel oder eine "
                    "Richtfunkstrecke meist die einzige stabile Lösung."))
            if a.art == "WLAN" and g.tiefe >= 2:
                liste.append(Befund(
                    WARNUNG, f"{_geraetename(st, g.name)}: Funk-Kaskade über {g.tiefe} Strecken",
                    f"{_geraetename(st, g.name)} hängt per WLAN an {_geraetename(st, a.zu)}, der selbst per Funk "
                    "angebunden ist. Jede Funkstrecke halbiert ungefähr den nutzbaren Durchsatz und erhöht "
                    "die Antwortzeit.",
                    "Mindestens die erste Strecke (Mesh-Master → Repeater) per LAN anbinden; "
                    "dann hängen weitere Repeater direkt an einem kabelgebundenen Knoten."))

    # Sonden: Abbrüche, Verluste, Schwankungen, "klebende" Verbindung
    for s in e.sonden:
        ort = f"{s.name} ({namen.get(s.bereich, s.bereich or '?')})"
        if s.verbunden_anteil < 0.98 or s.abbrueche >= 3:
            liste.append(Befund(KRITISCH if s.verbunden_anteil < 0.9 else WARNUNG,
                                f"{ort}: Verbindungsabbrüche",
                                f"{s.abbrueche} Abbrüche, verbunden in {_prozent(s.verbunden_anteil)} der "
                                f"Messungen (Zeitraum {s.zeitraum_s // 60} min).",
                                "Abdeckung am Sondenort verbessern; Ursache im Ereignisprotokoll der Sonde "
                                "(Beacon-Timeout = Signal verloren).", s.bereich))
        if s.verlust_mittel > 2:
            liste.append(Befund(WARNUNG, f"{ort}: Paketverlust {s.verlust_mittel:.1f} %",
                                f"Im Mittel gehen {s.verlust_mittel:.1f} % der Anfragen an den Router verloren "
                                f"(Antwortzeit Median {s.antwort_median} ms).",
                                "Funkstrecke zwischen Sonde und Router prüfen – häufig liegt die Ursache in "
                                "einer schwachen Repeater-Anbindung, nicht am Sondenort selbst.", s.bereich))
        if s.rssi_p10 is not None and s.rssi_median is not None and s.rssi_median - s.rssi_p10 >= 8:
            liste.append(Befund(HINWEIS, f"{ort}: stark schwankendes Signal",
                                f"Median {s.rssi_median} dBm, aber in 10 % der Zeit {s.rssi_p10} dBm oder "
                                "schlechter.",
                                "Typisch für Hallen mit Fahrverkehr, Rolltoren oder wechselnder Lagerbelegung. "
                                "Knoten höher montieren, freie Sicht zu den Arbeitsbereichen.", s.bereich))
        if s.klebt_anteil >= 0.3 and s.bester_geraet:
            liste.append(Befund(WARNUNG, f"{ort}: bleibt am schwächeren Knoten",
                                f"In {_prozent(s.klebt_anteil)} der Messungen war {s.bester_geraet} mindestens "
                                f"{STICKY_DB} dB stärker als der verbundene Knoten "
                                f"({', '.join(s.geraete)}).",
                                "Das Mesh Steering der FRITZ!Box lenkt Geräte mit 802.11k/v zum besseren Knoten; "
                                "einfache Geräte wie der ESP32 wechseln erst bei Verbindungsverlust. Mit einem "
                                "Smartphone am selben Ort gegenprüfen. Bleibt das Problem, Knoten so verschieben, "
                                "dass sich ihre Funkzellen weniger überlappen.", s.bereich))
        if s.rssi_am_ap is not None and s.rssi_median is not None and s.rssi_am_ap < s.rssi_median - 10:
            liste.append(Befund(HINWEIS, f"{ort}: Rückrichtung schwächer",
                                f"Die Sonde empfängt {s.rssi_median} dBm, kommt beim Access Point aber nur mit "
                                f"{s.rssi_am_ap} dBm an (Angabe der FRITZ!Box).",
                                "Kleine Geräte senden schwächer als Router. Für Endgeräte mit kleiner Antenne "
                                "(Scanner, Sensoren) zählt die Rückrichtung.", s.bereich))

    # Messpunkte mit "klebender" Verbindung
    klebende = [p for p in e.punkte if p.klebt]
    if klebende:
        liste.append(Befund(WARNUNG, f"{len(klebende)} Messpunkte am falschen Knoten",
                            ", ".join(f"{p.id}: {p.geraet} {p.rssi} dBm statt {p.bester_geraet} "
                                      f"{p.bester_rssi} dBm" for p in klebende[:8])
                            + (" …" if len(klebende) > 8 else ""),
                            "Geräte, die hier arbeiten, sollten zum stärkeren Knoten wechseln. Mit einem Smartphone "
                            "prüfen, ob das Mesh Steering der FRITZ!Box greift. Bei der Begehung zeigt der Wert "
                            "„bester Knoten“, was an dieser Stelle erreichbar ist."))

    # Kanäle
    eigene = [k for k, _ in Counter(e.kanaele["eigen"]).most_common()]
    for k in eigene:
        stoerer = [(ch, f) for ch, f in e.kanaele["fremd"].items() if kanal_ueberlappt(k, ch) and f["max"] >= -75]
        if stoerer:
            liste.append(Befund(WARNUNG, f"Kanal {k}: überlappende Fremdnetze",
                                "Fremde Netze auf " + ", ".join(f"Kanal {ch} (bis {f['max']} dBm)" for ch, f in
                                                                sorted(stoerer))
                                + f" überlappen mit Kanal {k} und stören stärker als Netze auf demselben Kanal.",
                                "Im 2,4-GHz-Band nur die Kanäle 1, 6 oder 11 verwenden. In der FRITZ!Box die "
                                "Funkkanal-Einstellung auf „automatisch“ lassen oder den am wenigsten belegten "
                                "dieser drei Kanäle fest wählen."))
        gleich = e.kanaele["fremd"].get(k)
        if gleich and gleich["anzahl"] >= 6:
            liste.append(Befund(HINWEIS, f"Kanal {k}: {gleich['anzahl']} Fremdnetze auf demselben Kanal",
                                "Viele Netze teilen sich die Sendezeit.",
                                "Wechsel auf einen weniger belegten Kanal (1, 6 oder 11) prüfen."))

    # BSSIDs mit eigener SSID, die keinem Gerät zugeordnet werden konnten
    unbekannt = sorted({b for b, z in e.zuordnung.items() if z.quelle == "unbekannt"})
    if unbekannt:
        liste.append(Befund(HINWEIS, f"{len(unbekannt)} Funkzellen ohne Zuordnung",
                            "Diese BSSIDs senden die eigene SSID, sind aber keinem Gerät zugeordnet: "
                            + ", ".join(unbekannt),
                            "FRITZ!Box-Mesh einlesen (python3 -m wlan_analyse fritz …) oder die BSSIDs in "
                            "standort.json beim passenden Gerät unter \"bssids\" eintragen. Ein fremdes Gerät "
                            "mit gleicher SSID stört das Mesh."))

    ohne_pos = [p.id for p in e.punkte if p.x is None]
    if ohne_pos:
        liste.append(Befund(HINWEIS, f"{len(ohne_pos)} Messpunkte ohne Position",
                            "Nicht im Lageplan eingetragen: " + ", ".join(ohne_pos[:15]),
                            "Punkte in standort.json unter \"punkte\" mit x/y ergänzen, dann erscheinen sie im "
                            "Lageplan und in der Heatmap."))

    liste.append(Befund(HINWEIS, "Messung nur im 2,4-GHz-Band",
                        "Der ESP32 misst nur 2,4 GHz. 5-GHz-Funkzellen reichen typischerweise 30–50 % weniger "
                        "weit; wo 2,4 GHz knapp ist, fehlt 5 GHz meist ganz.",
                        "Für 5 GHz die Werte am Smartphone (FRITZ!App WLAN) an den gleichen Messpunkten "
                        "gegenprüfen."))

    reihenfolge = {KRITISCH: 0, WARNUNG: 1, HINWEIS: 2}
    liste.sort(key=lambda b: reihenfolge[b.stufe])
    return liste


def bewertungstext(rssi):
    return stufe(rssi)[0]
