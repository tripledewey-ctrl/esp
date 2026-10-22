"""WLAN-Analyse: Auswertung der Messungen von wlan-sonde und der FRITZ!Box-Mesh-Topologie.

Aufruf: python3 -m wlan_analyse --help   (aus dem Ordner tools/wlan-analyse)
"""

__version__ = "1.0.0"

# Bewertungsstufen der Signalstärke (gleich wie Firmware und LED-Matrix)
STUFEN = [
    (-55, "sehr gut", "#1a7f37"),
    (-65, "gut", "#6cc644"),
    (-72, "brauchbar", "#d4a72c"),
    (-80, "schwach", "#e16f24"),
    (-200, "kritisch", "#cf222e"),
]

ZIEL_DBM = -67          # Zielwert für Sprache/Video und sauberes Roaming
STICKY_DB = 8           # verbundener Knoten so viel schwächer als der beste -> "klebt"


def stufe(rssi):
    """(Bezeichnung, Farbe) für einen RSSI-Wert in dBm; None -> keine Daten."""
    if rssi is None:
        return "keine Daten", "#8c959f"
    for grenze, name, farbe in STUFEN:
        if rssi >= grenze:
            return name, farbe
    return STUFEN[-1][1], STUFEN[-1][2]
