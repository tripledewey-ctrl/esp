#!/usr/bin/env python3
"""Erzeugt eine schematische Board-Abbildung (SVG) aus einer JSON-Beschreibung.

    python3 scripts/board-image.py docs/boards/specs/esp32-devkitc.json
    python3 scripts/board-image.py --all          # alle Specs neu erzeugen

Ausgabe: docs/boards/img/<name>.svg (Name = Dateiname der Spec)

Spec-Format (siehe docs/boards/specs/*.json):
  title, subtitle       Überschrift und Untertitel
  module                Beschriftung des Moduls/Chips auf der Platine
  top / bottom          Beschriftungen oben (Antenne) bzw. unten (USB)
  buttons               Liste von Tastern, z. B. ["BOOT", "EN"]
  led                   Text zur Onboard-LED (optional)
  matrix                Kantenlänge einer LED-Matrix (z. B. 8) statt Modul (optional)
  left / right          Stiftleisten von oben nach unten: [Label, Typ, Hinweis?]
  internal              Intern belegte GPIOs: [GPIO, Funktion]
Pin-Typen: power, gnd, gpio, input, strap, flash, ctrl, uart, usb
"""
import json
import sys
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SPECS = ROOT / "docs" / "boards" / "specs"
IMG = ROOT / "docs" / "boards" / "img"

COLORS = {
    "power": ("#d62828", "Versorgung"),
    "gnd":   ("#222222", "Masse (GND)"),
    "gpio":  ("#2a9d8f", "GPIO frei"),
    "input": ("#1d4ed8", "nur Eingang"),
    "strap": ("#e76f51", "Strapping-Pin (Boot-Pegel beachten)"),
    "flash": ("#9ca3af", "intern belegt – nicht verwenden"),
    "ctrl":  ("#7b2cbf", "Steuerung (Reset)"),
    "uart":  ("#b08900", "UART0 (Upload/Monitor)"),
    "usb":   ("#0f766e", "USB D-/D+"),
}

ROW = 22          # Abstand der Pins
PIN_R = 6
BOARD_W = 230
LABEL_W = 190     # Platz für Beschriftung je Seite
FONT = "font-family='DejaVu Sans, Arial, sans-serif'"


def pin_rows(side, pins, board_x, top_y):
    out = []
    left = side == "left"
    px = board_x + 18 if left else board_x + BOARD_W - 18
    for i, pin in enumerate(pins):
        label, kind = pin[0], pin[1]
        note = pin[2] if len(pin) > 2 else ""
        color = COLORS[kind][0]
        y = top_y + i * ROW
        out.append(f"<circle cx='{px}' cy='{y}' r='{PIN_R}' fill='#f5c542' stroke='#8a6d00'/>")
        # Fähnchen mit Pinname außerhalb der Platine
        fw = 74
        fx = board_x - 10 - fw if left else board_x + BOARD_W + 10
        lx = fx + fw if left else fx
        out.append(f"<line x1='{px}' y1='{y}' x2='{lx}' y2='{y}' stroke='{color}' stroke-width='1.5'/>")
        out.append(f"<rect x='{fx}' y='{y - 9}' width='{fw}' height='18' rx='4' fill='{color}'/>")
        out.append(f"<text x='{fx + fw / 2}' y='{y + 4}' text-anchor='middle' font-size='11' "
                   f"fill='#fff' font-weight='bold' {FONT}>{escape(label)}</text>")
        if note:
            nx = fx - 6 if left else fx + fw + 6
            anchor = "end" if left else "start"
            out.append(f"<text x='{nx}' y='{y + 4}' text-anchor='{anchor}' font-size='10' "
                       f"fill='#374151' {FONT}>{escape(note)}</text>")
    return out


def render(spec):
    left, right = spec.get("left", []), spec.get("right", [])
    internal = spec.get("internal", [])
    rows = max(len(left), len(right))
    board_x = 20 + LABEL_W + 80
    by = 80                                   # Oberkante Platine
    if spec.get("matrix"):
        body_h = 260                          # Matrix + Chip
    else:
        body_h = 190                          # Modul
    if spec.get("led"):
        body_h += 24                          # Platz für Onboard-LED unter dem Modul
    pin_y = by + body_h                       # erster Pin
    board_h = body_h + max(rows - 1, 0) * ROW + 100
    bot = by + board_h
    width = board_x * 2 + BOARD_W
    legend_kinds = sorted({p[1] for p in left + right}, key=list(COLORS).index)
    internal_h = (len(internal) * 20 + 40) if internal else 0
    height = bot + 56 + internal_h + (len(legend_kinds) + 1) // 2 * 20 + 30

    s = [f"<svg xmlns='http://www.w3.org/2000/svg' width='{width}' height='{height}' "
         f"viewBox='0 0 {width} {height}'>",
         f"<rect width='100%' height='100%' fill='#ffffff'/>",
         f"<text x='{width / 2}' y='34' text-anchor='middle' font-size='20' font-weight='bold' "
         f"fill='#111' {FONT}>{escape(spec['title'])}</text>",
         f"<text x='{width / 2}' y='56' text-anchor='middle' font-size='13' fill='#555' {FONT}>"
         f"{escape(spec.get('subtitle', ''))}</text>"]

    # Platine
    s.append(f"<rect x='{board_x}' y='{by}' width='{BOARD_W}' height='{board_h}' rx='10' "
             f"fill='#1f3d2b' stroke='#0d1f15' stroke-width='2'/>")
    cx = board_x + BOARD_W / 2
    if spec.get("top"):
        s.append(f"<text x='{cx}' y='{by - 8}' text-anchor='middle' font-size='11' fill='#555' {FONT}>"
                 f"{escape(spec['top'])}</text>")

    if spec.get("matrix"):
        n = spec["matrix"]
        cell = min(150 / n, 20)
        gx = cx - n * cell / 2
        gy = by + 40
        s.append(f"<rect x='{gx - 6}' y='{gy - 6}' width='{n * cell + 12}' height='{n * cell + 12}' "
                 f"fill='#111' rx='4'/>")
        for r in range(n):
            for c in range(n):
                hue = (r * n + c) * 360 / (n * n)
                s.append(f"<circle cx='{gx + c * cell + cell / 2}' cy='{gy + r * cell + cell / 2}' "
                         f"r='{cell * 0.32}' fill='hsl({hue:.0f},85%,60%)'/>")
        s.append(f"<text x='{cx}' y='{gy - 12}' text-anchor='middle' font-size='10' fill='#e5e7eb' {FONT}>"
                 f"{n}×{n} RGB-LEDs</text>")
        my = gy + n * cell + 30
        s.append(f"<rect x='{cx - 70}' y='{my}' width='140' height='34' rx='4' fill='#2b2b2b' stroke='#888'/>")
        s.append(f"<text x='{cx}' y='{my + 21}' text-anchor='middle' font-size='11' fill='#eee' {FONT}>"
                 f"{escape(spec['module'])}</text>")
    else:
        mw, mh = BOARD_W - 70, 150
        mx, my = cx - mw / 2, by + 14
        s.append(f"<rect x='{mx}' y='{my}' width='{mw}' height='{mh}' rx='4' fill='#c0c4c8' stroke='#666'/>")
        s.append(f"<rect x='{mx}' y='{my}' width='{mw}' height='34' rx='4' fill='#8d949b'/>")
        s.append(f"<path d='M{mx + 12} {my + 24} h20 v-14 h20 v14 h20 v-14 h20 v14 h20 v-14 h20 v14' "
                 f"fill='none' stroke='#333' stroke-width='2'/>")
        s.append(f"<text x='{cx}' y='{my + 95}' text-anchor='middle' font-size='12' font-weight='bold' "
                 f"fill='#222' {FONT}>{escape(spec['module'])}</text>")

    # Taster, LED, USB
    for i, b in enumerate(spec.get("buttons", [])):
        bx = board_x + 40 + i * (BOARD_W - 80 - 36)
        s.append(f"<rect x='{bx}' y='{bot - 70}' width='36' height='24' rx='3' fill='#ddd' stroke='#555'/>")
        s.append(f"<circle cx='{bx + 18}' cy='{bot - 58}' r='7' fill='#333'/>")
        s.append(f"<text x='{bx + 18}' y='{bot - 78}' text-anchor='middle' font-size='10' fill='#fff' {FONT}>"
                 f"{escape(b)}</text>")
    if spec.get("led"):
        ly = pin_y - 30
        s.append(f"<circle cx='{board_x + 44}' cy='{ly}' r='5' fill='#ff4fd8' stroke='#fff'/>")
        s.append(f"<text x='{board_x + 56}' y='{ly + 4}' font-size='10' fill='#fff' {FONT}>"
                 f"{escape(spec['led'])}</text>")
    s.append(f"<rect x='{cx - 30}' y='{bot - 22}' width='60' height='30' rx='4' fill='#b8bec4' stroke='#555'/>")
    if spec.get("bottom"):
        s.append(f"<text x='{cx}' y='{bot + 26}' text-anchor='middle' font-size='11' fill='#555' {FONT}>"
                 f"{escape(spec['bottom'])}</text>")

    s += pin_rows("left", left, board_x, pin_y)
    s += pin_rows("right", right, board_x, pin_y)

    y = bot + 56
    if internal:
        s.append(f"<text x='{board_x - 60}' y='{y}' font-size='13' font-weight='bold' fill='#111' {FONT}>"
                 f"Intern belegt</text>")
        for i, (gpio, func) in enumerate(internal):
            yy = y + 22 + i * 20
            s.append(f"<rect x='{board_x - 60}' y='{yy - 13}' width='80' height='17' rx='3' fill='#7b2cbf'/>")
            s.append(f"<text x='{board_x - 20}' y='{yy}' text-anchor='middle' font-size='11' fill='#fff' "
                     f"font-weight='bold' {FONT}>{escape(gpio)}</text>")
            s.append(f"<text x='{board_x + 28}' y='{yy}' font-size='11' fill='#222' {FONT}>{escape(func)}</text>")
        y += internal_h

    # Legende
    for i, kind in enumerate(legend_kinds):
        col, text = COLORS[kind]
        lx = 30 + (i % 2) * (width / 2)
        ly = y + (i // 2) * 20
        s.append(f"<rect x='{lx}' y='{ly - 11}' width='14' height='14' rx='3' fill='{col}'/>")
        s.append(f"<text x='{lx + 20}' y='{ly}' font-size='11' fill='#222' {FONT}>{escape(text)}</text>")

    s.append(f"<text x='{width - 10}' y='{height - 8}' text-anchor='end' font-size='9' fill='#999' {FONT}>"
             f"Schematisch, nicht maßstabsgetreu – erzeugt mit scripts/board-image.py</text>")
    s.append("</svg>")
    return "\n".join(s) + "\n"


def build(spec_path):
    spec = json.loads(Path(spec_path).read_text(encoding="utf-8"))
    out = IMG / (Path(spec_path).stem + ".svg")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(render(spec), encoding="utf-8")
    print(f"{spec_path} -> {out.relative_to(ROOT)}")


def main(argv):
    if argv == ["--all"]:
        paths = sorted(SPECS.glob("*.json"))
    elif argv:
        paths = argv
    else:
        print(__doc__)
        return 1
    for p in paths:
        build(p)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
