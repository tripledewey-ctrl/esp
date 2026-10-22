"""Tests für tools/wlan-analyse:  python3 -m unittest discover -s tests  (im Ordner tools/wlan-analyse)"""

import json
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from html.parser import HTMLParser
from pathlib import Path

ORDNER = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ORDNER))

from wlan_analyse import __main__ as cli  # noqa: E402
from wlan_analyse.analyse import (_zaehler_zuwachs, auswerten, bssids_zuordnen,  # noqa: E402
                                  kanal_ueberlappt, natuerlich)
from wlan_analyse.bericht import bericht_html  # noqa: E402
from wlan_analyse.daten import (messung_aus_dict, messungen_laden, normalisiere_bssid,  # noqa: E402
                                standort_laden)
from wlan_analyse.fritz import FritzFehler, mesh_abrufen, mesh_auswerten, mesh_laden  # noqa: E402
from http.server import BaseHTTPRequestHandler, HTTPServer  # noqa: E402
from wlan_analyse.sammler import sammeln  # noqa: E402

BEISPIEL = ORDNER / "beispiel"

# Zeilen genau im Format von messungAlsJson() der Firmware (projects/wlan-sonde/src/messung.cpp)
FIRMWARE_MESSUNG = ('{"typ":"messung","sonde":"sonde-halle","bereich":"Halle","ort":"Mitte","t":1792000000,'
                    '"uptime":120,"intervall":30,"verbunden":true,"bssid":"E0:28:6D:44:55:66","kanal":6,"rssi":-70,'
                    '"rssi_min":-74,"rssi_max":-67,"abbrueche":1,"wechsel":0,"antwort_ms":8.4,"verlust":0,'
                    '"knoten":[{"bssid":"DC:15:C8:77:88:99","rssi":-66,"kanal":6},'
                    '{"bssid":"E0:28:6D:44:55:66","rssi":-69,"kanal":6}],"fremd":[{"kanal":1,"anzahl":2,"max":-80}]}')
FIRMWARE_OFFLINE = ('{"typ":"messung","sonde":"sonde-halle","bereich":"Halle","ort":"Mitte","t":1792000030,'
                    '"uptime":150,"intervall":30,"verbunden":false,"abbrueche":2,"wechsel":0,"antwort_ms":null,'
                    '"verlust":100,"knoten":[],"fremd":[]}')
FIRMWARE_PUNKT = ('{"typ":"punkt","sonde":"sonde-mobil","bereich":"A","ort":"","punkt":"A-EG-1","t":0,"uptime":300,'
                  '"intervall":30,"verbunden":true,"bssid":"3C:A6:2F:11:22:33","kanal":6,"rssi":-58,"rssi_min":-61,'
                  '"rssi_max":-55,"abbrueche":0,"wechsel":0,"antwort_ms":3.1,"verlust":0,'
                  '"knoten":[{"bssid":"3C:A6:2F:11:22:33","rssi":-57,"kanal":6}],"fremd":[]}')


class TestDaten(unittest.TestCase):
    def test_firmware_zeilen(self):
        m = messung_aus_dict(json.loads(FIRMWARE_MESSUNG))
        self.assertEqual((m.typ, m.sonde, m.rssi, m.kanal, m.antwort_ms), ("messung", "sonde-halle", -70, 6, 8.4))
        self.assertEqual(m.bester.bssid, "DC:15:C8:77:88:99")
        off = messung_aus_dict(json.loads(FIRMWARE_OFFLINE))
        self.assertFalse(off.verbunden)
        self.assertIsNone(off.rssi)
        self.assertIsNone(off.antwort_ms)
        p = messung_aus_dict(json.loads(FIRMWARE_PUNKT))
        self.assertEqual((p.typ, p.punkt, p.bereich), ("punkt", "A-EG-1", "A"))

    def test_laden_doppelt_und_fehlerhaft(self):
        with tempfile.TemporaryDirectory() as d:
            f = Path(d) / "m.jsonl"
            f.write_text("\n".join([FIRMWARE_MESSUNG, FIRMWARE_MESSUNG, "kaputt", '{"ohne":"sonde"}', FIRMWARE_PUNKT]))
            erg = messungen_laden([f, f])
        self.assertEqual(len(erg.messungen), 2)
        self.assertEqual(erg.doppelt, 4)          # 1 in der ersten Datei + 3 beim zweiten Lesen
        self.assertEqual(erg.fehlerhaft, 4)

    def test_bssid(self):
        self.assertEqual(normalisiere_bssid("3c-a6-2f-11-22-33"), "3C:A6:2F:11:22:33")
        self.assertIsNone(normalisiere_bssid("3C:A6:2F:11:22"))
        self.assertIsNone(normalisiere_bssid(None))

    def test_bereich_zuordnen(self):
        st = standort_laden(BEISPIEL / "standort.json")
        self.assertEqual(st.bereich_zuordnen("Gebäude A"), "A")
        self.assertEqual(st.bereich_zuordnen("Halle"), "halle")
        self.assertEqual(st.bereich_zuordnen("geb-b"), "B")
        self.assertEqual(st.bereich_zuordnen("falsch", 40, 5), "halle")   # Position hat Vorrang
        self.assertIsNone(st.bereich_zuordnen("Keller"))


class TestFritz(unittest.TestCase):
    def test_beispiel_mesh(self):
        mesh = mesh_laden(BEISPIEL / "fritz-mesh.json")
        namen = [g.name for g in mesh.geraete]
        self.assertEqual(namen, ["FRITZ!Box 7590", "Repeater Halle", "Repeater B"])
        box, halle, b = mesh.geraete
        self.assertEqual(box.rolle, "master")
        self.assertIsNone(box.anbindung)
        self.assertNotIn("3E:A6:2F:11:22:33", box.bssids)              # Gastnetz ausgeblendet
        self.assertEqual((halle.anbindung.art, halle.anbindung.zu, halle.anbindung.rate), ("WLAN", "FRITZ!Box 7590", 86.0))
        self.assertEqual((b.anbindung.zu, b.tiefe, b.anbindung.rate), ("Repeater Halle", 2, 26.0))
        self.assertEqual(mesh.geraet_zu_bssid()["DC:15:C8:77:88:99"], "Repeater B")
        tor = next(c for c in mesh.clients if c.name == "sonde-halle-tor")
        self.assertEqual((tor.an, tor.rssi_am_ap), ("Repeater B", -79))

    def test_lan_wird_bevorzugt_und_fehlende_felder(self):
        roh = {"nodes": [
            {"uid": "m", "device_name": "Box", "is_meshed": True, "mesh_role": "master", "node_interfaces": [
                {"uid": "m-lan", "type": "LAN", "node_links": [
                    {"uid": "l1", "type": "LAN", "node_1_uid": "m", "node_2_uid": "r",
                     "node_interface_1_uid": "m-lan", "node_interface_2_uid": "r-lan"}]},
                {"uid": "m-5", "name": "AP:5G:0", "type": "WLAN", "opmode": "AP", "mac_address": "00:11:22:33:44:55",
                 "node_links": [{"uid": "l2", "type": "WLAN", "node_1_uid": "m", "node_2_uid": "r",
                                 "node_interface_1_uid": "m-5", "node_interface_2_uid": "r-5",
                                 "cur_data_rate_rx": 400000, "cur_data_rate_tx": 300000}]}]},
            {"uid": "r", "device_name": "Rep", "is_meshed": True, "mesh_role": "slave", "node_interfaces": []}]}
        mesh = mesh_auswerten(roh)
        rep = mesh.geraete[1]
        self.assertEqual(rep.anbindung.art, "LAN")


class _FritzAttrappe(BaseHTTPRequestHandler):
    """Antwortet wie die FRITZ!Box auf X_AVM-DE_GetMeshListPath (ohne Digest-Anmeldung)."""
    passwort_falsch = False

    def log_message(self, *a):
        pass

    def do_POST(self):
        laenge = int(self.headers.get("Content-Length", 0))
        koerper = self.rfile.read(laenge).decode()
        if self.passwort_falsch:
            self.send_response(401)
            self.end_headers()
            return
        ok = (self.path == "/upnp/control/hosts" and "X_AVM-DE_GetMeshListPath" in self.headers.get("SOAPAction", "")
              and "X_AVM-DE_GetMeshListPath" in koerper)
        antwort = ('<?xml version="1.0"?><s:Envelope><s:Body><u:X_AVM-DE_GetMeshListPathResponse>'
                   '<NewX_AVM-DE_MeshListPath>/meshlist.lua?sid=abc</NewX_AVM-DE_MeshListPath>'
                   '</u:X_AVM-DE_GetMeshListPathResponse></s:Body></s:Envelope>').encode()
        self.send_response(200 if ok else 500)
        self.send_header("Content-Length", str(len(antwort)))
        self.end_headers()
        self.wfile.write(antwort)

    def do_GET(self):
        daten = (BEISPIEL / "fritz-mesh.json").read_bytes() if self.path == "/meshlist.lua?sid=abc" else b"{}"
        self.send_response(200 if daten != b"{}" else 404)
        self.send_header("Content-Length", str(len(daten)))
        self.end_headers()
        self.wfile.write(daten)


class TestFritzAbruf(unittest.TestCase):
    def _server(self, falsch=False):
        handler = type("H", (_FritzAttrappe,), {"passwort_falsch": falsch})
        server = HTTPServer(("127.0.0.1", 0), handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        return server.server_address[1]

    def test_abruf(self):
        roh = mesh_abrufen("127.0.0.1", "admin", "geheim", port=self._server(), timeout=5)
        self.assertEqual(len(mesh_auswerten(roh).geraete), 3)

    def test_falsches_passwort(self):
        with self.assertRaises(FritzFehler) as f:
            mesh_abrufen("127.0.0.1", "admin", "falsch", port=self._server(falsch=True), timeout=5)
        self.assertIn("Anmeldung abgelehnt", str(f.exception))

    def test_nicht_erreichbar(self):
        with self.assertRaises(FritzFehler) as f:
            mesh_abrufen("127.0.0.1", "admin", "x", port=1, timeout=2)
        self.assertIn("Zugriff für Anwendungen", str(f.exception))


class TestAnalyse(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.standort = standort_laden(BEISPIEL / "standort.json")
        cls.messungen = messungen_laden([BEISPIEL / "messungen.jsonl"]).messungen
        cls.mesh = mesh_laden(BEISPIEL / "fritz-mesh.json")
        cls.erg = auswerten(cls.standort, cls.messungen, cls.mesh)

    def test_bereiche(self):
        bew = {b.id: b.bewertung for b in self.erg.bereiche}
        self.assertEqual(bew, {"A": "gut", "halle": "mangelhaft", "B": "gut"})

    def test_befunde_fuer_halle_und_repeater(self):
        titel = [(b.stufe, b.titel) for b in self.erg.befunde]
        self.assertIn(("kritisch", "Halle: Abdeckung mangelhaft"), titel)
        self.assertTrue(any(s == "kritisch" and t.startswith("Repeater B: schwache Funkanbindung") for s, t in titel))
        self.assertTrue(any(t.startswith("Repeater B: Funk-Kaskade") for _, t in titel))
        halle = next(b for b in self.erg.befunde if b.titel == "Halle: Abdeckung mangelhaft")
        self.assertIn("LAN", halle.massnahme)
        self.assertIn("Lücke liegt genau in der Halle", halle.text)
        # Befunde sind nach Schwere sortiert
        stufen = [b.stufe for b in self.erg.befunde]
        self.assertEqual(stufen, sorted(stufen, key=["kritisch", "warnung", "hinweis"].index))

    def test_falscher_knoten(self):
        klebt = {p.id for p in self.erg.punkte if p.klebt}
        self.assertEqual(klebt, {"H-10", "H-12"})
        h12 = next(p for p in self.erg.punkte if p.id == "H-12")
        self.assertEqual((h12.geraet, h12.bester_geraet), ("Repeater Halle", "Repeater B"))

    def test_mehrfachmessung_wird_zusammengefasst(self):
        h5 = next(p for p in self.erg.punkte if p.id == "H-5")
        self.assertEqual(h5.anzahl, 2)

    def test_sonden(self):
        so = {s.name: s for s in self.erg.sonden}
        tor = so["sonde-halle-tor"]
        self.assertGreater(tor.abbrueche, 0)
        self.assertLess(tor.verbunden_anteil, 1)
        self.assertEqual(tor.rssi_am_ap, -79)
        self.assertEqual(so["wlan-zentrale"].geraete, {"FRITZ!Box 7590": 1.0})

    def test_zuordnung_ohne_fritz(self):
        z = bssids_zuordnen(self.standort, self.messungen, None)
        self.assertEqual(z["3C:A6:2F:11:22:33"].geraet, "FRITZ!Box 7590")
        self.assertEqual(z["DC:15:C8:77:88:99"].geraet, "Repeater B")
        self.assertEqual(z["E0:28:6D:44:55:66"].geraet, "Repeater Halle")
        self.assertTrue(all(v.quelle == "geschätzt" for v in z.values()))

    def test_ohne_daten(self):
        erg = auswerten(self.standort, [], None)
        self.assertEqual(erg.befunde[0].titel, "Keine Messdaten")

    def test_hilfsfunktionen(self):
        self.assertTrue(kanal_ueberlappt(6, 3))
        self.assertFalse(kanal_ueberlappt(1, 6))
        self.assertFalse(kanal_ueberlappt(6, 6))
        self.assertEqual(_zaehler_zuwachs([0, 1, 3, 0, 2]), 5)      # Neustart dazwischen
        self.assertEqual(sorted(["H-10", "H-2", "H-1"], key=natuerlich), ["H-1", "H-2", "H-10"])


class _Pruefer(HTMLParser):
    def __init__(self):
        super().__init__()
        self.skripte = 0

    def handle_starttag(self, tag, attrs):
        self.skripte += tag == "script"


class TestBericht(unittest.TestCase):
    def test_bericht_und_escaping(self):
        st = standort_laden(BEISPIEL / "standort.json")
        st.name = '<script>alert("x")</script>'
        m = messung_aus_dict(json.loads(FIRMWARE_PUNKT.replace("A-EG-1", "<img src=x onerror=1>")))
        html = bericht_html(auswerten(st, [m], None))
        p = _Pruefer()
        p.feed(html)
        self.assertEqual(p.skripte, 0)
        self.assertNotIn("<img src=x", html)
        self.assertIn("&lt;script&gt;", html)

    def test_beispielbericht(self):
        st = standort_laden(BEISPIEL / "standort.json")
        erg = auswerten(st, messungen_laden([BEISPIEL / "messungen.jsonl"]).messungen,
                        mesh_laden(BEISPIEL / "fritz-mesh.json"))
        html = bericht_html(erg)
        for teil in ("Lageplan und Abdeckung", "Befunde und Empfehlungen", "Mesh-Topologie", "Feste Sonden",
                     "Messpunkte der Begehung", "Kanalbelegung", "<svg"):
            self.assertIn(teil, html)
        self.assertTrue(erg.heatmap["halle"])


class TestSammlerUndCli(unittest.TestCase):
    def test_sammler(self):
        with tempfile.TemporaryDirectory() as d:
            ziel = Path(d) / "s.jsonl"
            server = sammeln(0, ziel, host="127.0.0.1")
            threading.Thread(target=server.serve_forever, daemon=True).start()
            url = f"http://127.0.0.1:{server.server_address[1]}"
            oeffner = urllib.request.build_opener(urllib.request.ProxyHandler({}))
            try:
                r = oeffner.open(urllib.request.Request(url + "/api/sonde", data=FIRMWARE_OFFLINE.encode(),
                                                        headers={"Content-Type": "application/json"}))
                self.assertEqual(r.status, 204)
                with self.assertRaises(urllib.error.HTTPError) as fehler:
                    oeffner.open(urllib.request.Request(url + "/api/sonde", data=b'{"kein":"json mit sonde"}'))
                self.assertEqual(fehler.exception.code, 400)
                status = json.loads(oeffner.open(url + "/api/status").read())
                self.assertEqual(status["sonden"]["sonde-halle"]["messungen"], 1)
            finally:
                server.shutdown()
                server.server_close()
            zeile = json.loads(ziel.read_text().strip())
            self.assertEqual(zeile["sonde"], "sonde-halle")
            self.assertIn("empfangen", zeile)

    def test_cli_demo(self):
        with tempfile.TemporaryDirectory() as d:
            ausgabe = Path(d) / "b.html"
            self.assertEqual(cli.main(["demo", "-o", str(ausgabe), "--json", str(Path(d) / "e.json")]), 0)
            self.assertGreater(ausgabe.stat().st_size, 20000)
            self.assertIn("befunde", json.loads((Path(d) / "e.json").read_text()))


if __name__ == "__main__":
    unittest.main()
