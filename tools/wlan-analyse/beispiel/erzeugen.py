#!/usr/bin/env python3
"""Erzeugt die Beispieldaten (standort.json, fritz-mesh.json, messungen.jsonl).

Szenario: Gebäude A und Gebäude B, dazwischen eine Halle (Sandwichpaneele, Regale).
FRITZ!Box in Gebäude A, ein Repeater an der Hallenwand zu A, ein Repeater in Gebäude B,
beide per WLAN angebunden (B als Kaskade über den Hallen-Repeater). Die Werte stammen
aus einem einfachen Ausbreitungsmodell, sind also plausibel, aber keine echte Messung.

    python3 beispiel/erzeugen.py
"""

import datetime as dt
import json
import math
import random
from pathlib import Path

ZIEL = Path(__file__).resolve().parent
rnd = random.Random(20261022)
SSID = "FRITZ!Box 7590 Musterbetrieb"

BEREICHE = [
    {"id": "A", "name": "Gebäude A", "x": 0, "y": 0, "breite": 20, "hoehe": 16, "art": "gebaeude"},
    {"id": "halle", "name": "Halle", "x": 24, "y": -4, "breite": 40, "hoehe": 24, "art": "halle"},
    {"id": "B", "name": "Gebäude B", "x": 68, "y": 0, "breite": 20, "hoehe": 16, "art": "gebaeude"},
]
GERAETE = {
    "FRITZ!Box 7590": {"bereich": "A", "x": 6, "y": 7, "bssid": "3C:A6:2F:11:22:33", "hops": 0},
    "Repeater Halle": {"bereich": "halle", "x": 26, "y": 6, "bssid": "E0:28:6D:44:55:66", "hops": 1},
    "Repeater B": {"bereich": "B", "x": 70, "y": 8, "bssid": "DC:15:C8:77:88:99", "hops": 2},
}
WAND_DB = {"gebaeude": 6.0, "halle": 6.0}        # je Durchgang durch eine Außenwand
INNEN_DB_M = {"gebaeude": 0.4, "halle": 0.35}    # Innenwände bzw. Regale je Meter
FREMD = [  # (Kanal, x, y, Sendestärke bei 1 m) – Nachbarnetze
    (1, -10, 4, -40), (1, -8, 14, -42), (3, 2, -6, -34), (11, 40, 30, -38), (11, 50, 30, -40),
    (11, 95, 4, -42), (9, 96, 12, -44), (6, 30, 34, -46), (6, 90, -6, -48),
]


def _innen(bereich, x, y):
    return bereich["x"] <= x <= bereich["x"] + bereich["breite"] and bereich["y"] <= y <= bereich["y"] + bereich["hoehe"]


def rssi(gx, gy, px, py, p0=-38.0, rauschen=2.0):
    d = math.hypot(px - gx, py - gy)
    verlust = 10 * 2.0 * math.log10(max(d, 1.0))
    schritte = 200
    for b in BEREICHE:
        vorher, laenge = _innen(b, gx, gy), 0.0
        for i in range(1, schritte + 1):
            x, y = gx + (px - gx) * i / schritte, gy + (py - gy) * i / schritte
            jetzt = _innen(b, x, y)
            if jetzt != vorher:
                verlust += WAND_DB[b["art"]]
            if jetzt:
                laenge += d / schritte
            vorher = jetzt
        verlust += INNEN_DB_M[b["art"]] * laenge
    return round(p0 - verlust + rnd.gauss(0, rauschen))


def knoten_an(x, y):
    k = [{"bssid": g["bssid"], "rssi": rssi(g["x"], g["y"], x, y), "kanal": 6, "_name": n}
         for n, g in GERAETE.items()]
    return sorted([e for e in k if e["rssi"] > -93], key=lambda e: -e["rssi"])


def fremd_an(x, y):
    je = {}
    for ch, fx, fy, p0 in FREMD:
        r = rssi(fx, fy, x, y, p0=p0, rauschen=1.5)
        if r > -92:
            e = je.setdefault(ch, {"kanal": ch, "anzahl": 0, "max": -127})
            e["anzahl"] += 1
            e["max"] = max(e["max"], r)
    return [je[k] for k in sorted(je)]


def antwort(hops, r):
    basis = [3.0, 8.0, 17.0][hops] + max(0, -72 - r) * 0.6
    verlust = 0 if r > -76 else min(40, int((-76 - r) * 3 + rnd.random() * 6))
    return round(basis * (0.8 + rnd.random() * 0.5), 1), verlust


def zeile(**d):
    for k in [k for k in d if k.startswith("_")]:
        d.pop(k)
    return json.dumps(d, ensure_ascii=False, separators=(",", ":"))


def main():
    beginn = int(dt.datetime(2026, 10, 21, 7, 0, tzinfo=dt.timezone.utc).timestamp())

    # ---------------------------------------------------------------- Begehung
    punkte, zeilen = [], []
    raster = [("A", [(4, 3), (10, 3), (16, 3), (4, 13), (10, 13), (16, 13)]),
              ("halle", [(30, 0), (30, 16), (38, 0), (38, 16), (44, 10), (50, 0), (50, 16), (56, 10), (60, 0), (60, 16),
                         (34, 10), (62, 10)]),
              ("B", [(72, 3), (78, 3), (84, 3), (72, 13), (78, 13), (84, 13)])]
    verbunden = "FRITZ!Box 7590"
    t = beginn + 3600 * 3
    for bereich, liste in raster:
        for i, (x, y) in enumerate(liste, 1):
            pid = f"{'H' if bereich == 'halle' else bereich}-{i}"
            punkte.append({"id": pid, "bereich": bereich, "x": x, "y": y})
            wiederholen = 2 if pid in ("H-5", "H-8") else 1
            for _ in range(wiederholen):
                kn = knoten_an(x, y)
                # Das Messgerät bleibt am bisherigen Knoten, bis er zu schwach wird (wie ein ESP32)
                aktuell = next((k for k in kn if k["_name"] == verbunden), None)
                if aktuell is None or aktuell["rssi"] < -84:
                    aktuell = kn[0]
                    verbunden = aktuell["_name"]
                ms, verl = antwort(GERAETE[verbunden]["hops"], aktuell["rssi"])
                r = aktuell["rssi"]
                zeilen.append(zeile(typ="punkt", sonde="sonde-mobil", bereich="Halle" if bereich == "halle" else bereich,
                                    ort="", punkt=pid, t=t, uptime=t - beginn, intervall=30, verbunden=True,
                                    bssid=aktuell["bssid"], kanal=6, rssi=r, rssi_min=r - rnd.randint(1, 4),
                                    rssi_max=r + rnd.randint(1, 3), abbrueche=0, wechsel=0, antwort_ms=ms,
                                    verlust=verl, knoten=[{k: v for k, v in e.items() if k != "_name"} for e in kn],
                                    fremd=fremd_an(x, y)))
                t += 75

    # ------------------------------------------------------------- feste Sonden
    sonden = [
        {"name": "wlan-zentrale", "bereich": "A", "x": 12, "y": 9.5, "ort": "Büro EG, Regal (Waveshare-Matrix)"},
        {"name": "sonde-halle-mitte", "bereich": "halle", "x": 44, "y": 14, "ort": "Halle Mitte, Säule 4, 3 m"},
        {"name": "sonde-halle-tor", "bereich": "halle", "x": 56, "y": 18, "ort": "Halle, Rolltor Süd"},
        {"name": "sonde-b-eg", "bereich": "B", "x": 79, "y": 8, "ort": "Gebäude B, Werkstatt"},
    ]
    for s in sonden:
        verbunden = {"wlan-zentrale": "FRITZ!Box 7590", "sonde-b-eg": "Repeater B"}.get(s["name"], "Repeater Halle")
        abbrueche = wechsel = 0
        for i in range(180):                                   # 3 h, alle 60 s
            tt = beginn + i * 60
            kn = knoten_an(s["x"], s["y"])
            # Hallenbetrieb: Stapler/Tor dämpfen zeitweise das Signal
            if s["bereich"] == "halle" and (i % 37) in (0, 1, 2):
                for k in kn:
                    k["rssi"] -= 9
            aktuell = next((k for k in kn if k["_name"] == verbunden), None)
            if aktuell is None or aktuell["rssi"] < -86:
                abbrueche += 1
                zeilen.append(zeile(typ="messung", sonde=s["name"], bereich="Halle" if s["bereich"] == "halle" else s["bereich"],
                                    ort=s["ort"], t=tt, uptime=i * 60 + 20, intervall=60, verbunden=False,
                                    abbrueche=abbrueche, wechsel=wechsel, antwort_ms=None, verlust=100,
                                    knoten=[], fremd=[]))
                neu = kn[0]["_name"]                         # nach dem Abbruch: stärkster Knoten
                if neu != verbunden:
                    wechsel += 1
                    verbunden = neu
                continue
            ms, verl = antwort(GERAETE[verbunden]["hops"], aktuell["rssi"])
            r = aktuell["rssi"]
            zeilen.append(zeile(typ="messung", sonde=s["name"], bereich="Halle" if s["bereich"] == "halle" else s["bereich"],
                                ort=s["ort"], t=tt, uptime=i * 60 + 20, intervall=60, verbunden=True,
                                bssid=aktuell["bssid"], kanal=6, rssi=r, rssi_min=r - rnd.randint(1, 5),
                                rssi_max=r + rnd.randint(1, 3), abbrueche=abbrueche, wechsel=wechsel, antwort_ms=ms,
                                verlust=verl, knoten=[{k: v for k, v in e.items() if k != "_name"} for e in kn],
                                fremd=fremd_an(s["x"], s["y"])))

    standort = {
        "name": "Musterbetrieb – Gebäude A, Halle, Gebäude B",
        "ssid": SSID,
        "hinweis": "Koordinaten in Metern, x nach rechts, y nach unten (wie im Lageplan).",
        "bereiche": BEREICHE,
        "geraete": [{"name": n, "bereich": g["bereich"], "x": g["x"], "y": g["y"]} for n, g in GERAETE.items()],
        "sonden": sonden,
        "punkte": punkte,
    }
    (ZIEL / "standort.json").write_text(json.dumps(standort, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    (ZIEL / "messungen.jsonl").write_text("\n".join(zeilen) + "\n", encoding="utf-8")
    (ZIEL / "fritz-mesh.json").write_text(json.dumps(mesh(), ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(f"{len(punkte)} Messpunkte, {len(zeilen)} Zeilen geschrieben")


def mesh():
    """Mesh-Liste im Format der FRITZ!Box (X_AVM-DE_GetMeshListPath), gekürzt."""
    def ap(uid, name, mac, ssid, kanal, links=()):
        return {"uid": uid, "name": name, "type": "WLAN", "mac_address": mac, "opmode": "AP", "ssid": ssid,
                "current_channel": kanal, "steering_enabled": True, "node_links": list(links)}

    def link(uid, a, b, ia, ib, typ, rx, tx, mx, **extra):
        return {"uid": uid, "type": typ, "state": "CONNECTED", "node_1_uid": a, "node_2_uid": b,
                "node_interface_1_uid": ia, "node_interface_2_uid": ib, "cur_data_rate_rx": rx,
                "cur_data_rate_tx": tx, "max_data_rate_rx": mx, "max_data_rate_tx": mx, **extra}

    l_rh = link("nl-1", "n-box", "n-rh", "ni-box-5g", "ni-rh-up", "WLAN", 104000, 86000, 1201000)
    l_rb = link("nl-2", "n-rh", "n-rb", "ni-rh-5g", "ni-rb-up", "WLAN", 39000, 26000, 2402000)
    clients = [("n-c1", "wlan-zentrale", "24:0A:C4:A1:00:01", "n-box", "ni-box-2g", -63),
               ("n-c2", "sonde-halle-mitte", "24:0A:C4:A1:00:02", "n-rh", "ni-rh-2g", -75),
               ("n-c3", "sonde-halle-tor", "24:0A:C4:A1:00:03", "n-rb", "ni-rb-2g", -79),
               ("n-c4", "sonde-b-eg", "24:0A:C4:A1:00:04", "n-rb", "ni-rb-2g", -66)]
    c_links = {cid: link(f"nl-{cid}", ap_uid, cid, ap_if, f"ni-{cid}", "WLAN", 65000, 72200, 72200, rx_rcpi=rcpi)
               for cid, _, _, ap_uid, ap_if, rcpi in clients}

    def an(ap_if):
        return [c_links[cid] for cid, _, _, _, i, _ in clients if i == ap_if]

    nodes = [
        {"uid": "n-box", "device_name": "FRITZ!Box 7590", "device_model": "FRITZ!Box 7590", "device_manufacturer": "AVM",
         "device_mac_address": "3C:A6:2F:11:22:30", "is_meshed": True, "mesh_role": "master",
         "node_interfaces": [
             {"uid": "ni-box-lan", "name": "LAN:1", "type": "LAN", "mac_address": "3C:A6:2F:11:22:30", "node_links": []},
             ap("ni-box-2g", "AP:2G:0", "3C:A6:2F:11:22:33", SSID, 6, an("ni-box-2g")),
             ap("ni-box-5g", "AP:5G:0", "3C:A6:2F:11:22:34", SSID, 36, [l_rh]),
             ap("ni-box-gast", "AP:2G:1", "3E:A6:2F:11:22:33", "FRITZ!Box Gastzugang", 6)]},
        {"uid": "n-rh", "device_name": "Repeater Halle", "device_model": "FRITZ!Repeater 1200 AX",
         "device_manufacturer": "AVM", "device_mac_address": "E0:28:6D:44:55:60", "is_meshed": True, "mesh_role": "slave",
         "node_interfaces": [
             {"uid": "ni-rh-up", "name": "UPLINK:5G:0", "type": "WLAN", "mac_address": "E0:28:6D:44:55:65",
              "opmode": "REPEATER", "ssid": "", "current_channel": 36, "node_links": [l_rh]},
             ap("ni-rh-2g", "AP:2G:0", "E0:28:6D:44:55:66", SSID, 6, an("ni-rh-2g")),
             ap("ni-rh-5g", "AP:5G:0", "E0:28:6D:44:55:67", SSID, 36, [l_rb])]},
        {"uid": "n-rb", "device_name": "Repeater B", "device_model": "FRITZ!Repeater 3000 AX",
         "device_manufacturer": "AVM", "device_mac_address": "DC:15:C8:77:88:90", "is_meshed": True, "mesh_role": "slave",
         "node_interfaces": [
             {"uid": "ni-rb-up", "name": "UPLINK:5G:0", "type": "WLAN", "mac_address": "DC:15:C8:77:88:95",
              "opmode": "REPEATER", "ssid": "", "current_channel": 36, "node_links": [l_rb]},
             ap("ni-rb-2g", "AP:2G:0", "DC:15:C8:77:88:99", SSID, 6, an("ni-rb-2g")),
             ap("ni-rb-5g", "AP:5G:0", "DC:15:C8:77:88:9A", SSID, 36)]},
    ]
    for cid, name, mac, _, _, _ in clients:
        nodes.append({"uid": cid, "device_name": name, "device_model": "", "device_mac_address": mac,
                      "is_meshed": False, "mesh_role": "unknown",
                      "node_interfaces": [{"uid": f"ni-{cid}", "name": "", "type": "WLAN", "mac_address": mac,
                                           "opmode": "STA", "node_links": [c_links[cid]]}]})
    return {"schema_version": "5.2", "nodes": nodes}


if __name__ == "__main__":
    main()
