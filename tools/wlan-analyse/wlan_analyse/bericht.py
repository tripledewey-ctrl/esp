"""HTML-Bericht (eine eigenständige Datei, ohne externe Abhängigkeiten)."""

from __future__ import annotations

import datetime as dt
from html import escape

from . import ZIEL_DBM, __version__, stufe
from .analyse import KRITISCH, WARNUNG, Ergebnis, _geraetename

FARBE_STUFE = {KRITISCH: "#cf222e", WARNUNG: "#d4a72c", "hinweis": "#0969da"}
TITEL_STUFE = {KRITISCH: "Kritisch", WARNUNG: "Warnung", "hinweis": "Hinweis"}
FARBE_BEWERTUNG = {"gut": "#1a7f37", "ausreichend": "#d4a72c", "mangelhaft": "#cf222e", "keine Daten": "#8c959f"}

CSS = """
:root{--bg:#fff;--fg:#1f2328;--mut:#59636e;--line:#d1d9e0;--card:#f6f8fa;--plan:#fdfdfd;--wand:#424a53}
@media(prefers-color-scheme:dark){:root{--bg:#0d1117;--fg:#e6edf3;--mut:#9198a1;--line:#30363d;--card:#161b22;--plan:#11161d;--wand:#9198a1}}
*{box-sizing:border-box}
body{margin:0 auto;max-width:72em;padding:20px 16px 60px;font:15px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif;background:var(--bg);color:var(--fg)}
h1{font-size:1.7em;margin:0 0 4px}h2{font-size:1.25em;margin:32px 0 10px;padding-bottom:4px;border-bottom:1px solid var(--line)}
.mut{color:var(--mut)}.klein{font-size:.85em}
.kacheln{display:grid;grid-template-columns:repeat(auto-fit,minmax(14em,1fr));gap:12px;margin:16px 0}
.kachel{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 16px;border-top:5px solid var(--f)}
.kachel b.gross{font-size:2em;display:block;line-height:1.2}
table{width:100%;border-collapse:collapse;font-size:.92em}td,th{text-align:left;padding:6px 8px;border-bottom:1px solid var(--line);vertical-align:top}
th{font-weight:600;background:var(--card)}.n{text-align:right;font-variant-numeric:tabular-nums;white-space:nowrap}
.dot{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px;vertical-align:baseline}
.befund{border:1px solid var(--line);border-left:5px solid var(--f);border-radius:8px;padding:10px 14px;margin:10px 0;background:var(--card)}
.befund h3{margin:0 0 4px;font-size:1em}.befund .stufe{color:var(--f);font-weight:700;margin-right:6px}
.befund p{margin:4px 0}.massnahme{border-top:1px dashed var(--line);margin-top:8px;padding-top:6px}
.plan{border:1px solid var(--line);border-radius:10px;background:var(--plan);overflow-x:auto}
.plan svg{display:block;width:100%;height:auto;min-width:640px}
.legende{display:flex;flex-wrap:wrap;gap:6px 18px;margin:8px 2px;font-size:.88em}
.scroll{overflow-x:auto}
@media print{body{max-width:none}.befund,.kachel,.plan{break-inside:avoid}}
"""


def _punkt(rssi):
    return f'<span class="dot" style="background:{stufe(rssi)[1]}"></span>'


def _dbm(rssi):
    return "–" if rssi is None else f"{_punkt(rssi)}{rssi}&nbsp;dBm"


def _zeit(t):
    return dt.datetime.fromtimestamp(t).strftime("%d.%m.%Y %H:%M") if t else "–"


def _dauer(s):
    return f"{s // 3600} h {s % 3600 // 60} min" if s >= 3600 else f"{s // 60} min"


# ------------------------------------------------------------------ Lageplan
def lageplan(e: Ergebnis) -> str:
    st = e.standort
    if not st.bereiche:
        return '<p class="mut">Kein Lageplan in standort.json.</p>'
    xs = [b.x for b in st.bereiche] + [b.x + b.breite for b in st.bereiche]
    ys = [b.y for b in st.bereiche] + [b.y + b.hoehe for b in st.bereiche]
    rand = 4
    x0, y0 = min(xs) - rand, min(ys) - rand - 2
    breite_m, hoehe_m = max(xs) - x0 + rand, max(ys) - y0 + rand + 4
    s = 1000 / breite_m
    w, h = 1000, hoehe_m * s
    X = lambda x: (x - x0) * s  # noqa: E731
    Y = lambda y: (y - y0) * s  # noqa: E731
    teile = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w:.0f} {h:.0f}" role="img" '
             f'aria-label="Lageplan {escape(st.name)}" font-family="system-ui,sans-serif">',
             '<defs><pattern id="schraffur" width="10" height="10" patternUnits="userSpaceOnUse" '
             'patternTransform="rotate(45)"><line x1="0" y1="0" x2="0" y2="10" stroke="#8c959f" '
             'stroke-opacity=".25" stroke-width="3"/></pattern></defs>']

    for b in st.bereiche:                                    # Flächen
        fill = "url(#schraffur)" if b.art == "halle" else "#8c959f"
        deck = "1" if b.art == "halle" else ".08"
        teile.append(f'<rect x="{X(b.x):.1f}" y="{Y(b.y):.1f}" width="{b.breite * s:.1f}" '
                     f'height="{b.hoehe * s:.1f}" fill="{fill}" fill-opacity="{deck}"/>')
    for zellen in e.heatmap.values():                         # interpolierte Abdeckung
        for (x, y, bw, bh, r) in zellen:
            teile.append(f'<rect x="{X(x):.1f}" y="{Y(y):.1f}" width="{bw * s + .6:.1f}" height="{bh * s + .6:.1f}" '
                         f'fill="{stufe(r)[1]}" fill-opacity=".33"/>')
    for b in st.bereiche:                                    # Wände + Beschriftung
        teile.append(f'<rect x="{X(b.x):.1f}" y="{Y(b.y):.1f}" width="{b.breite * s:.1f}" height="{b.hoehe * s:.1f}" '
                     f'fill="none" style="stroke:var(--wand)" stroke-width="{3 if b.art != "halle" else 4}"/>')
        teile.append(f'<text x="{X(b.x) + 8:.1f}" y="{Y(b.y) - 7:.1f}" font-size="17" font-weight="700" '
                     f'fill="currentColor">{escape(b.name)}</text>')

    # Mesh-Geräte mit Position aus dem Lageplan, Anbindung aus der FRITZ!Box
    pos = {g.name: (g.x, g.y) for g in st.geraete}
    if e.mesh:
        for g in e.mesh.geraete:
            a = g.anbindung
            kind, eltern = _geraetename(st, g.name), _geraetename(st, a.zu) if a else None
            if not a or kind not in pos or eltern not in pos:
                continue
            (x1, y1), (x2, y2) = pos[kind], pos[eltern]
            if a.art == "LAN":
                farbe, strich, text = "#57606a", "", "LAN"
            else:
                r = a.rate or 0
                farbe = "#cf222e" if r < 60 else "#e16f24" if r < 150 else "#1a7f37"
                strich, text = ' stroke-dasharray="10 6"', f"WLAN {r:.0f} Mbit/s" if a.rate else "WLAN"
            teile.append(f'<line x1="{X(x1):.1f}" y1="{Y(y1):.1f}" x2="{X(x2):.1f}" y2="{Y(y2):.1f}" '
                         f'stroke="{farbe}" stroke-width="4"{strich} stroke-linecap="round"/>')
            mx, my = (X(x1) + X(x2)) / 2, (Y(y1) + Y(y2)) / 2
            teile.append(f'<text x="{mx:.1f}" y="{my - 9:.1f}" font-size="13" font-weight="600" text-anchor="middle" '
                         f'fill="{farbe}" paint-order="stroke" style="stroke:var(--plan)" stroke-width="4">{escape(text)}</text>')
    master = {_geraetename(st, g.name) for g in e.mesh.geraete if g.rolle == "master"} if e.mesh else set()
    for g in st.geraete:
        x, y = X(g.x), Y(g.y)
        if g.name in master:
            teile.append(f'<rect x="{x - 11:.1f}" y="{y - 11:.1f}" width="22" height="22" rx="4" fill="#0969da" '
                         'stroke="#fff" stroke-width="2.5"/>')
        else:
            teile.append(f'<path d="M{x:.1f} {y - 13:.1f} L{x + 12:.1f} {y + 9:.1f} L{x - 12:.1f} {y + 9:.1f} Z" '
                         'fill="#0969da" stroke="#fff" stroke-width="2.5" stroke-linejoin="round"/>')
        teile.append(f'<text x="{x:.1f}" y="{y + 28:.1f}" font-size="13" font-weight="700" text-anchor="middle" '
                     f'fill="#0969da" paint-order="stroke" style="stroke:var(--plan)" stroke-width="4">{escape(g.name)}</text>')

    for p in e.punkte:                                       # Messpunkte der Begehung
        if p.x is None:
            continue
        x, y = X(p.x), Y(p.y)
        teile.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="7" fill="{stufe(p.bester_rssi)[1]}" stroke="#24292f" '
                     f'stroke-width="1.2"><title>{escape(p.id)}: bester {p.bester_rssi} dBm ({escape(str(p.bester_geraet))}), '
                     f'verbunden {p.rssi} dBm ({escape(str(p.geraet))})</title></circle>')
        if p.klebt:
            teile.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="12" fill="none" stroke="#e16f24" stroke-width="2.5" '
                         'stroke-dasharray="4 3"/>')
        teile.append(f'<text x="{x + 10:.1f}" y="{y - 8:.1f}" font-size="11" fill="currentColor" '
                     f'paint-order="stroke" style="stroke:var(--plan)" stroke-width="3">{escape(p.id)}</text>')

    for so in e.sonden:                                      # feste Sonden
        if so.x is None:
            continue
        x, y = X(so.x), Y(so.y)
        teile.append(f'<rect x="{x - 9:.1f}" y="{y - 9:.1f}" width="18" height="18" transform="rotate(45 {x:.1f} {y:.1f})" '
                     f'fill="{stufe(so.rssi_median)[1]}" stroke="#24292f" stroke-width="1.5"><title>{escape(so.name)}: '
                     f'Median {so.rssi_median} dBm, 10 %-Wert {so.rssi_p10} dBm</title></rect>')
        teile.append(f'<text x="{x + 15:.1f}" y="{y + 4:.1f}" font-size="12" font-weight="600" '
                     f'fill="currentColor" paint-order="stroke" style="stroke:var(--plan)" stroke-width="3">{escape(so.name)}</text>')

    lx, ly = 16, h - 18                                      # Maßstab 10 m
    teile.append(f'<line x1="{lx}" y1="{ly:.1f}" x2="{lx + 10 * s:.1f}" y2="{ly:.1f}" stroke="currentColor" stroke-width="3"/>'
                 f'<text x="{lx + 5 * s:.1f}" y="{ly - 7:.1f}" font-size="12" text-anchor="middle" fill="currentColor">10 m</text>')
    teile.append("</svg>")
    return '<div class="plan" style="color:var(--fg)">' + "".join(teile) + "</div>"


def legende() -> str:
    farben = "".join(f'<span><span class="dot" style="background:{f}"></span>{n} ({"≥ " + str(g) if g > -200 else "< −80"} dBm)</span>'
                     for g, n, f in [(-55, "sehr gut", "#1a7f37"), (-65, "gut", "#6cc644"), (-72, "brauchbar", "#d4a72c"),
                                     (-80, "schwach", "#e16f24"), (-200, "kritisch", "#cf222e")])
    return (f'<div class="legende">{farben}</div><div class="legende mut">'
            '<span>■ Mesh-Master</span><span>▲ Repeater/Access Point</span><span>◆ feste Sonde (Median)</span>'
            '<span>● Messpunkt (bester Knoten)</span><span style="color:#e16f24">◌ verbunden mit schwächerem Knoten</span>'
            '<span>— LAN · - - WLAN-Anbindung</span><span>Flächenfarbe: interpoliert, nur innerhalb eines Bereichs</span></div>')


# ------------------------------------------------------------------ Diagramme
def verlauf_svg(verlauf, breite=180, hoehe=36) -> str:
    werte = [(t, r) for t, r in verlauf]
    if len(werte) < 2:
        return ""
    t0, t1 = min(t for t, _ in werte), max(t for t, _ in werte) or 1
    X = lambda t: (t - t0) / max(t1 - t0, 1) * (breite - 2) + 1  # noqa: E731
    Y = lambda r: (-40 - max(-95, min(-40, r))) / 55 * (hoehe - 2) + 1  # noqa: E731
    pfade, akt = [], []
    for t, r in werte:
        if r is None:
            if akt:
                pfade.append(akt)
            akt = []
        else:
            akt.append(f"{X(t):.1f},{Y(r):.1f}")
    if akt:
        pfade.append(akt)
    linien = "".join(f'<polyline points="{" ".join(p)}" fill="none" stroke="currentColor" stroke-width="1.4"/>' for p in pfade)
    ausfall = "".join(f'<rect x="{X(t) - 1:.1f}" y="0" width="2" height="{hoehe}" fill="#cf222e" opacity=".6"/>'
                      for t, r in werte if r is None)
    return (f'<svg width="{breite}" height="{hoehe}" viewBox="0 0 {breite} {hoehe}" aria-label="RSSI-Verlauf">'
            f'<line x1="0" x2="{breite}" y1="{Y(ZIEL_DBM):.1f}" y2="{Y(ZIEL_DBM):.1f}" stroke="#1a7f37" stroke-dasharray="3 3"/>'
            f"{ausfall}{linien}</svg>")


def kanaele_svg(e: Ergebnis) -> str:
    fremd, eigen = e.kanaele["fremd"], set(e.kanaele["eigen"])
    if not fremd and not eigen:
        return '<p class="mut">Keine Scandaten.</p>'
    hoechst = max([f["anzahl"] for f in fremd.values()] + [1])
    bw, h, oben = 44, 130, 24
    teile = [f'<svg viewBox="0 0 {13 * bw + 20} {h + oben + 34}" width="100%" style="max-width:640px" '
             'font-family="system-ui,sans-serif">']
    for ch in range(1, 14):
        x = 10 + (ch - 1) * bw
        f = fremd.get(ch)
        if f:
            hh = f["anzahl"] / hoechst * h
            teile.append(f'<rect x="{x + 6}" y="{oben + h - hh:.1f}" width="{bw - 12}" height="{hh:.1f}" rx="3" '
                         f'fill="{stufe(f["max"])[1]}"><title>Kanal {ch}: {f["anzahl"]} Fremdnetze, stärkstes {f["max"]} dBm'
                         f'</title></rect><text x="{x + bw / 2}" y="{oben + h - hh - 5:.1f}" font-size="11" text-anchor="middle" '
                         f'fill="currentColor">{f["anzahl"]}</text>')
        if ch in eigen:
            teile.append(f'<rect x="{x + 2}" y="{oben - 18}" width="{bw - 4}" height="{h + 22}" fill="none" stroke="#0969da" '
                         'stroke-width="2.5" rx="4"/>')
        teile.append(f'<text x="{x + bw / 2}" y="{oben + h + 22}" font-size="13" text-anchor="middle" '
                     f'fill="{"#0969da" if ch in eigen else "currentColor"}" font-weight="{700 if ch in eigen else 400}">{ch}</text>')
    teile.append("</svg>")
    return ("".join(teile) + '<p class="mut klein">Balken: Anzahl fremder Netze je Kanal, Farbe = stärkstes Fremdnetz. '
            'Blau umrandet: Kanäle des eigenen Mesh-Netzes. Ideal sind die Kanäle 1, 6 und 11.</p>')


# ------------------------------------------------------------------ Bericht
def bericht_html(e: Ergebnis) -> str:
    st = e.standort
    namen = {b.id: b.name for b in st.bereiche}
    zeitraum = f"{_zeit(e.zeitraum[0])} – {_zeit(e.zeitraum[1])}" if e.zeitraum else "ohne Zeitstempel"
    teile = [f"""<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WLAN-Analyse – {escape(st.name)}</title><style>{CSS}</style></head><body>
<h1>WLAN-Analyse: {escape(st.name)}</h1>
<p class="mut">SSID „{escape(st.ssid)}“ · Messzeitraum {zeitraum} · {len(e.punkte)} Messpunkte · {len(e.sonden)} Sonden ·
{e.anzahl_messungen} Messungen{" · Mesh-Daten der FRITZ!Box" if e.mesh else ""} · erstellt {dt.datetime.now():%d.%m.%Y %H:%M}</p>"""]

    # Kacheln je Bereich
    teile.append('<div class="kacheln">')
    for b in e.bereiche:
        f = FARBE_BEWERTUNG[b.bewertung]
        anteil = f"{b.anteil_ziel * 100:.0f} %" if b.anteil_ziel is not None else "–"
        teile.append(f'<div class="kachel" style="--f:{f}"><span class="mut">{escape(b.name)}</span>'
                     f'<b class="gross" style="color:{f}">{anteil}</b>'
                     f'<span>mit ≥ {ZIEL_DBM} dBm · <b>{b.bewertung}</b></span><br>'
                     f'<span class="mut klein">Median {b.median if b.median is not None else "–"} dBm · '
                     f'schwächste {b.minimum if b.minimum is not None else "–"} dBm · {b.punkte} Messstellen'
                     f'{" · Sonden: " + escape(", ".join(b.sonden)) if b.sonden else ""}</span></div>')
    teile.append("</div>")

    teile.append("<h2>Lageplan und Abdeckung</h2>" + lageplan(e) + legende())

    # Befunde
    zaehler = {k: sum(1 for b in e.befunde if b.stufe == k) for k in (KRITISCH, WARNUNG, "hinweis")}
    teile.append(f"<h2>Befunde und Empfehlungen</h2><p class='mut'>{zaehler[KRITISCH]} kritisch · "
                 f"{zaehler[WARNUNG]} Warnungen · {zaehler['hinweis']} Hinweise</p>")
    for b in e.befunde:
        bereich = f' <span class="mut">· {escape(namen.get(b.bereich, b.bereich))}</span>' if b.bereich else ""
        teile.append(f'<div class="befund" style="--f:{FARBE_STUFE[b.stufe]}"><h3><span class="stufe">'
                     f'{TITEL_STUFE[b.stufe]}</span>{escape(b.titel)}{bereich}</h3><p>{escape(b.text)}</p>'
                     + (f'<p class="massnahme"><b>Maßnahme:</b> {escape(b.massnahme)}</p>' if b.massnahme else "")
                     + "</div>")

    # Mesh
    teile.append("<h2>Mesh-Topologie</h2>")
    if e.mesh:
        teile.append('<div class="scroll"><table><tr><th>Gerät</th><th>Modell</th><th>Anbindung</th>'
                     '<th class="n">Rate</th><th>Funkzellen (BSSID · Kanal)</th></tr>')
        for g in e.mesh.geraete:
            a = g.anbindung
            anb = "Mesh-Master" if g.rolle == "master" else (
                f"{a.art}{' ' + a.band if a.band else ''} an {escape(_geraetename(st, a.zu))}" if a else "nicht verbunden")
            rate = f"{a.rate:.0f} Mbit/s" if a and a.rate else ("–" if a and a.art == "WLAN" else "")
            zellen = "<br>".join(f"{f.band}: {f.bssid} · {f.kanal}" for f in g.funkzellen) or "–"
            teile.append(f"<tr><td><b>{escape(_geraetename(st, g.name))}</b></td><td>{escape(g.modell)}</td>"
                         f"<td>{'&nbsp;' * 2 * g.tiefe}{anb}</td><td class='n'>{rate}</td><td class='klein'>{zellen}</td></tr>")
        teile.append("</table></div>")
    else:
        teile.append('<p class="mut">Keine FRITZ!Box-Daten. Mit <code>python3 -m wlan_analyse fritz</code> einlesen, '
                     "dann zeigt der Bericht Repeater-Anbindung, Datenraten und die Zuordnung der BSSIDs.</p>")
    zuord = sorted(((b, z) for b, z in e.zuordnung.items() if b in e.gesehen), key=lambda kv: (kv[1].geraet, kv[0]))
    if zuord:
        teile.append('<p class="klein"><b>Gemessene Funkzellen (BSSID → Gerät, Quelle der Zuordnung):</b> '
                     + " · ".join(f"{b} → {escape(z.geraet)} <span class='mut'>({z.quelle})</span>" for b, z in zuord) + "</p>")

    # Sonden
    if e.sonden:
        teile.append('<h2>Feste Sonden</h2><div class="scroll"><table><tr><th>Sonde</th><th class="n">Median</th>'
                     '<th class="n">10 %</th><th class="n">min</th><th>Verlauf</th><th>verbunden mit</th>'
                     '<th class="n">Abbr. / Wechsel</th><th class="n">Antwort</th><th class="n">Verlust</th></tr>')
        for s in e.sonden:
            geraete = ", ".join(f"{escape(g)} {a * 100:.0f} %" for g, a in s.geraete.items()) or "–"
            if s.bester_geraet and s.bester_geraet not in s.geraete:
                geraete += f'<br><span class="mut">stärkster: {escape(s.bester_geraet)} ({s.bester_median} dBm)</span>'
            ap = f'<br><span class="mut">am AP: {s.rssi_am_ap} dBm</span>' if s.rssi_am_ap is not None else ""
            teile.append(f"<tr><td><b>{escape(s.name)}</b><br><span class='mut'>{escape(namen.get(s.bereich, s.bereich or ''))}"
                         f"{' · ' + escape(s.ort) if s.ort else ''} · {s.anzahl} Messungen, {_dauer(s.zeitraum_s)}</span></td>"
                         f"<td class='n'>{_dbm(s.rssi_median)}{ap}</td><td class='n'>{s.rssi_p10 if s.rssi_p10 is not None else '–'}</td>"
                         f"<td class='n'>{s.rssi_min if s.rssi_min is not None else '–'}</td><td>{verlauf_svg(s.verlauf)}</td>"
                         f"<td class='klein'>{geraete}</td><td class='n'>{s.abbrueche} / {s.wechsel}</td>"
                         f"<td class='n'>{s.antwort_median if s.antwort_median is not None else '–'} ms</td>"
                         f"<td class='n'>{s.verlust_mittel:.1f} %</td></tr>")
        teile.append('</table></div><p class="mut klein">Verlauf: Signalstärke über die Zeit, gestrichelt '
                     f'{ZIEL_DBM} dBm, rote Striche = nicht verbunden.</p>')

    # Messpunkte
    if e.punkte:
        teile.append('<h2>Messpunkte der Begehung</h2><div class="scroll"><table><tr><th>Punkt</th><th>Bereich</th>'
                     '<th class="n">verbunden</th><th>mit</th><th class="n">bester Knoten</th><th>Gerät</th>'
                     '<th class="n">Antwort</th><th class="n">Verlust</th></tr>')
        for p in e.punkte:
            ort = st.punkte.get(p.id)
            hinweis = ' <b style="color:#e16f24">⚠ falscher Knoten</b>' if p.klebt else ""
            teile.append(f"<tr><td><b>{escape(p.id)}</b>{'<br><span class=mut>' + escape(ort.name) + '</span>' if ort and ort.name else ''}</td>"
                         f"<td>{escape(namen.get(p.bereich, p.bereich or '–'))}</td><td class='n'>{_dbm(p.rssi)}</td>"
                         f"<td>{escape(p.geraet or '–')}{hinweis}</td><td class='n'>{_dbm(p.bester_rssi)}</td>"
                         f"<td>{escape(p.bester_geraet or '–')}</td>"
                         f"<td class='n'>{p.antwort_ms if p.antwort_ms is not None else '–'} ms</td>"
                         f"<td class='n'>{p.verlust:.0f} %</td></tr>")
        teile.append("</table></div>")

    teile.append("<h2>Kanalbelegung 2,4 GHz</h2>" + kanaele_svg(e))

    teile.append(f"""<h2>Methode</h2><ul class="klein">
<li>Messgerät: ESP32 mit wlan-sonde. Gemessen wird die Empfangsstärke (RSSI) des verbundenen Mesh-Knotens und aller Knoten
mit gleicher SSID. Die Antwortzeit ist der TCP-Verbindungsaufbau zum Router, also etwa ein Ping.</li>
<li>Bewertung: Ziel ≥ {ZIEL_DBM} dBm (Sprache, Video, sauberes Roaming); unter −80 dBm brechen Verbindungen ab. Die Abdeckung eines
Bereichs zählt den jeweils <i>besten</i> erreichbaren Knoten je Messstelle.</li>
<li>Die ESP32-Antenne empfängt schwächer als ein Smartphone; die Werte eignen sich für Vergleiche innerhalb des Standorts.</li>
<li>Die Flächenfarben im Lageplan sind aus den Messstellen interpoliert, getrennt je Bereich. Zwischen weit auseinander liegenden
Messstellen sind sie eine Schätzung.</li></ul>
<p class="mut klein">wlan-analyse {__version__} · github.com/tripledewey-ctrl/esp</p></body></html>""")
    return "\n".join(teile)
