"""FRITZ!Box-Mesh: Topologie per TR-064 abrufen und auswerten.

Die FRITZ!Box liefert über TR-064 (Dienst Hosts:1, Aktion X_AVM-DE_GetMeshListPath)
den Pfad zu einer JSON-Datei mit allen Mesh-Geräten, ihren Schnittstellen
(WLAN-Funkzellen mit BSSID und Kanal, LAN) und den Verbindungen dazwischen.

Voraussetzungen in der FRITZ!Box:
  - Heimnetz -> Netzwerk -> Netzwerkeinstellungen -> "Zugriff für Anwendungen zulassen"
  - FRITZ!Box-Benutzer mit dem Recht "FRITZ!Box Einstellungen"
"""

from __future__ import annotations

import json
import re
import urllib.error
import urllib.request
from collections import deque
from dataclasses import dataclass, field

from .daten import normalisiere_bssid

SOAP_AKTION = "urn:dslforum-org:service:Hosts:1#X_AVM-DE_GetMeshListPath"
SOAP_KOERPER = (
    '<?xml version="1.0" encoding="utf-8"?>'
    '<s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/" '
    's:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"><s:Body>'
    '<u:X_AVM-DE_GetMeshListPath xmlns:u="urn:dslforum-org:service:Hosts:1"/>'
    "</s:Body></s:Envelope>"
)


class FritzFehler(RuntimeError):
    pass


def mesh_abrufen(adresse: str = "fritz.box", benutzer: str = "", passwort: str = "",
                 port: int = 49000, timeout: float = 10) -> dict:
    """Lädt die Mesh-Liste (JSON) von der FRITZ!Box."""
    basis = f"http://{adresse}:{port}"
    passwoerter = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    passwoerter.add_password(None, basis, benutzer, passwort)
    oeffner = urllib.request.build_opener(
        urllib.request.ProxyHandler({}),                       # Heimnetz: nie über Proxy
        urllib.request.HTTPDigestAuthHandler(passwoerter),
    )
    anfrage = urllib.request.Request(
        basis + "/upnp/control/hosts", data=SOAP_KOERPER.encode(),
        headers={"Content-Type": 'text/xml; charset="utf-8"', "SOAPAction": SOAP_AKTION})
    try:
        antwort = oeffner.open(anfrage, timeout=timeout).read().decode("utf-8", "replace")
        treffer = re.search(r"<NewX_AVM-DE_MeshListPath>(.*?)</NewX_AVM-DE_MeshListPath>", antwort, re.S)
        if not treffer:
            raise FritzFehler("Antwort ohne Mesh-Pfad – FRITZ!OS zu alt oder kein Mesh-Master?")
        pfad = treffer.group(1).strip()
        return json.loads(oeffner.open(basis + pfad, timeout=timeout).read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        if e.code == 401:
            raise FritzFehler("Anmeldung abgelehnt: Benutzer/Passwort prüfen, Benutzer braucht das "
                              "Recht 'FRITZ!Box Einstellungen'") from e
        raise FritzFehler(f"HTTP-Fehler {e.code} von {basis}") from e
    except urllib.error.URLError as e:
        raise FritzFehler(f"{basis} nicht erreichbar ({e.reason}). In der FRITZ!Box unter Heimnetz -> "
                          "Netzwerk -> Netzwerkeinstellungen 'Zugriff für Anwendungen zulassen' "
                          "aktivieren.") from e


# -------------------------------------------------------------- Auswertung
@dataclass
class Funkzelle:
    bssid: str
    band: str              # "2,4 GHz" | "5 GHz" | "6 GHz"
    kanal: int | None
    ssid: str


@dataclass
class Anbindung:
    """Verbindung eines Mesh-Geräts zu seinem Elternknoten (Richtung Mesh-Master)."""
    zu: str
    art: str               # "LAN" | "WLAN"
    rate_rx: float | None  # Mbit/s
    rate_tx: float | None
    max_rx: float | None
    max_tx: float | None
    band: str = ""

    @property
    def rate(self) -> float | None:
        werte = [r for r in (self.rate_rx, self.rate_tx) if r]
        return min(werte) if werte else None


@dataclass
class MeshGeraet:
    uid: str
    name: str
    modell: str
    mac: str
    rolle: str             # "master" | "slave"
    funkzellen: list[Funkzelle] = field(default_factory=list)
    anbindung: Anbindung | None = None
    tiefe: int = 0         # Funk-/Kabelstrecken bis zum Master

    @property
    def bssids(self) -> list[str]:
        return [f.bssid for f in self.funkzellen]


@dataclass
class Client:
    name: str
    mac: str
    an: str                # Name des Mesh-Geräts
    band: str
    rate_rx: float | None
    rate_tx: float | None
    rssi_am_ap: int | None  # wie stark der Client beim Access Point ankommt (falls geliefert)


@dataclass
class Mesh:
    geraete: list[MeshGeraet]
    clients: list[Client]

    def geraet_zu_bssid(self) -> dict[str, str]:
        return {b: g.name for g in self.geraete for b in g.bssids}


def _band(schnittstelle: dict) -> str:
    name = str(schnittstelle.get("name", "")).upper()
    kanal = schnittstelle.get("current_channel")
    if "6G" in name:
        return "6 GHz"
    if "5G" in name or (isinstance(kanal, int) and kanal > 14):
        return "5 GHz"
    return "2,4 GHz"


def _mbit(wert) -> float | None:
    # Die Mesh-Liste gibt Datenraten in kbit/s an
    return round(wert / 1000, 1) if isinstance(wert, (int, float)) and wert > 0 else None


def _dbm(wert) -> int | None:
    return int(wert) if isinstance(wert, (int, float)) and -110 <= wert < 0 else None


def mesh_auswerten(roh: dict, ssid: str | None = None) -> Mesh:
    """Wertet die JSON-Mesh-Liste der FRITZ!Box aus. Unbekannte Felder werden ignoriert,
    fehlende tolerant behandelt."""
    knoten = {n.get("uid"): n for n in roh.get("nodes", []) if isinstance(n, dict) and n.get("uid")}
    ist_mesh = {uid for uid, n in knoten.items()
                if n.get("is_meshed") and str(n.get("mesh_role", "")).lower() in ("master", "slave")}
    schnittstelle = {}                                  # Schnittstellen-uid -> (Knoten-uid, Daten)
    for uid, n in knoten.items():
        for s in n.get("node_interfaces", []) or []:
            if isinstance(s, dict) and s.get("uid"):
                schnittstelle[s["uid"]] = (uid, s)

    # Eigene SSID: die 2,4-GHz-Funkzelle des Masters, falls nicht vorgegeben
    master = next((uid for uid in ist_mesh if str(knoten[uid].get("mesh_role")).lower() == "master"), None)
    if not ssid and master:
        for s in knoten[master].get("node_interfaces", []) or []:
            if s.get("type") == "WLAN" and str(s.get("opmode", "")).upper() == "AP" and _band(s) == "2,4 GHz":
                ssid = s.get("ssid") or None
                break

    geraete: dict[str, MeshGeraet] = {}
    for uid in ist_mesh:
        n = knoten[uid]
        g = MeshGeraet(uid=uid, name=n.get("device_name") or uid, modell=n.get("device_model", ""),
                       mac=normalisiere_bssid(n.get("device_mac_address")) or "",
                       rolle=str(n.get("mesh_role", "")).lower())
        for s in n.get("node_interfaces", []) or []:
            if s.get("type") != "WLAN" or str(s.get("opmode", "")).upper() != "AP":
                continue
            if ssid and s.get("ssid") and s.get("ssid") != ssid:
                continue                                 # Gastnetz u. ä.
            b = normalisiere_bssid(s.get("mac_address"))
            if b:
                g.funkzellen.append(Funkzelle(bssid=b, band=_band(s), kanal=s.get("current_channel"),
                                              ssid=s.get("ssid", "")))
        geraete[uid] = g

    # Alle Verbindungen sammeln (jede steht in beiden Endpunkten, daher über uid entdoppeln)
    links = {}
    for uid, (knoten_uid, s) in schnittstelle.items():
        for l in s.get("node_links", []) or []:
            if isinstance(l, dict) and str(l.get("state", "CONNECTED")).upper() == "CONNECTED":
                links[l.get("uid") or id(l)] = l

    # Baum der Mesh-Geräte ab dem Master (Breitensuche) -> Elternknoten = Anbindung
    nachbarn: dict[str, list[tuple[str, dict]]] = {uid: [] for uid in ist_mesh}
    clients = []
    for l in links.values():
        a, b = l.get("node_1_uid"), l.get("node_2_uid")
        if a in ist_mesh and b in ist_mesh:
            nachbarn[a].append((b, l))
            nachbarn[b].append((a, l))
        elif (a in ist_mesh) != (b in ist_mesh) and l.get("type") == "WLAN":
            ap_uid, cl_uid = (a, b) if a in ist_mesh else (b, a)
            cl = knoten.get(cl_uid, {})
            ap_s = schnittstelle.get(l.get("node_interface_1_uid") if a == ap_uid
                                     else l.get("node_interface_2_uid"), (None, {}))[1]
            # Raten aus Sicht des Access Points: rx = vom Client empfangen
            clients.append(Client(
                name=cl.get("device_name") or cl_uid,
                mac=normalisiere_bssid(cl.get("device_mac_address")) or "",
                an=geraete[ap_uid].name, band=_band(ap_s),
                rate_rx=_mbit(l.get("cur_data_rate_rx")), rate_tx=_mbit(l.get("cur_data_rate_tx")),
                rssi_am_ap=_dbm(l.get("rx_rcpi")) or _dbm(l.get("rssi"))))

    if master:
        besucht = {master}
        warteschlange = deque([master])
        while warteschlange:
            uid = warteschlange.popleft()
            # LAN-Verbindungen bevorzugen, falls ein Gerät auf mehreren Wegen erreichbar ist
            for nb, l in sorted(nachbarn[uid], key=lambda e: e[1].get("type") != "LAN"):
                if nb in besucht:
                    continue
                besucht.add(nb)
                s_eltern = schnittstelle.get(l.get("node_interface_1_uid") if l.get("node_1_uid") == uid
                                             else l.get("node_interface_2_uid"), (None, {}))[1]
                # Raten aus Sicht des Kindes: rx = vom Elternknoten empfangen
                kind_ist_1 = l.get("node_1_uid") == nb
                rx, tx = ("cur_data_rate_rx", "cur_data_rate_tx") if kind_ist_1 else ("cur_data_rate_tx", "cur_data_rate_rx")
                mrx, mtx = ("max_data_rate_rx", "max_data_rate_tx") if kind_ist_1 else ("max_data_rate_tx", "max_data_rate_rx")
                art = "LAN" if l.get("type") == "LAN" else "WLAN"
                geraete[nb].anbindung = Anbindung(
                    zu=geraete[uid].name, art=art, rate_rx=_mbit(l.get(rx)), rate_tx=_mbit(l.get(tx)),
                    max_rx=_mbit(l.get(mrx)), max_tx=_mbit(l.get(mtx)),
                    band=_band(s_eltern) if art == "WLAN" else "")
                geraete[nb].tiefe = geraete[uid].tiefe + 1
                warteschlange.append(nb)

    reihenfolge = sorted(geraete.values(), key=lambda g: (g.rolle != "master", g.tiefe, g.name.lower()))
    return Mesh(geraete=reihenfolge, clients=sorted(clients, key=lambda c: c.name.lower()))


def mesh_laden(pfad, ssid: str | None = None) -> Mesh:
    with open(pfad, encoding="utf-8") as f:
        return mesh_auswerten(json.load(f), ssid)


def mesh_text(mesh: Mesh) -> str:
    """Kurze Übersicht für die Konsole."""
    zeilen = []
    for g in mesh.geraete:
        einzug = "  " * g.tiefe
        if g.anbindung:
            a = g.anbindung
            rate = f", {a.rate:.0f} Mbit/s" if a.rate else ""
            anb = f"  <- {a.art}{' ' + a.band if a.band else ''} von {a.zu}{rate}"
        else:
            anb = "  (Mesh-Master)"
        zeilen.append(f"{einzug}{g.name} [{g.modell}]{anb}")
        for f in g.funkzellen:
            zeilen.append(f"{einzug}    {f.band:7} Kanal {f.kanal!s:>3}  {f.bssid}")
    if mesh.clients:
        zeilen.append("")
        zeilen.append(f"{len(mesh.clients)} WLAN-Clients:")
        for c in mesh.clients:
            extra = f", {c.rssi_am_ap} dBm am AP" if c.rssi_am_ap is not None else ""
            zeilen.append(f"  {c.name:28} an {c.an} ({c.band}{extra})")
    return "\n".join(zeilen)
