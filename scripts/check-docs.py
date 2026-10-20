#!/usr/bin/env python3
"""Prüft die Dokumentation und Konfiguration des Repos auf Konsistenz.

    python3 scripts/check-docs.py

Geprüft wird:
  - relative Links und Bilder in allen Markdown-Dateien zeigen auf vorhandene Dateien
  - jede Board-Seite in docs/boards/ ist in docs/boards/README.md verlinkt und hat ein Bild
  - jedes Bild aus docs/boards/specs/*.json ist aktuell (scripts/board-image.py)
  - jede in Projekten genutzte Basis (extends = …) existiert in boards/boards.ini
  - jedes Projekt unter projects/ wird in der GitHub Action gebaut
Exit-Code 1 bei Fehlern.
"""
import configparser
import importlib.util
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOARDS_DOC = ROOT / "docs" / "boards"
errors = []


def err(msg):
    errors.append(msg)


def md_files():
    for p in ROOT.rglob("*.md"):
        rel = p.relative_to(ROOT)
        if any(part in (".git", ".pio", "build", "node_modules") for part in rel.parts):
            continue
        yield p


LINK = re.compile(r"!?\[[^\]]*\]\(([^)\s]+)\)|<img[^>]+src=\"([^\"]+)\"")


def check_links():
    for md in md_files():
        if md.name == "_vorlage.md":
            continue
        text = md.read_text(encoding="utf-8")
        text = re.sub(r"```.*?```", "", text, flags=re.S)  # Codeblöcke ignorieren
        for m in LINK.finditer(text):
            target = m.group(1) or m.group(2)
            if re.match(r"^(https?:|mailto:|#)", target):
                continue
            path = target.split("#")[0]
            if not path:
                continue
            if not (md.parent / path).exists():
                err(f"{md.relative_to(ROOT)}: Link-Ziel fehlt: {target}")


def check_board_pages():
    index = (BOARDS_DOC / "README.md").read_text(encoding="utf-8")
    for page in sorted(BOARDS_DOC.glob("*.md")):
        if page.name in ("README.md", "_vorlage.md"):
            continue
        if f"]({page.name})" not in index:
            err(f"docs/boards/README.md: Board-Seite {page.name} fehlt in der Übersicht")
        text = page.read_text(encoding="utf-8")
        if "img/" not in text:
            err(f"docs/boards/{page.name}: kein Bild eingebunden")
        if "## Von der Familie zum Board" not in text:
            err(f"docs/boards/{page.name}: Abschnitt 'Von der Familie zum Board' fehlt")


def check_images():
    spec = importlib.util.spec_from_file_location("board_image", ROOT / "scripts" / "board-image.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    import json
    for s in sorted((BOARDS_DOC / "specs").glob("*.json")):
        svg = BOARDS_DOC / "img" / (s.stem + ".svg")
        expected = mod.render(json.loads(s.read_text(encoding="utf-8")))
        if not svg.exists():
            err(f"{svg.relative_to(ROOT)} fehlt – python3 scripts/board-image.py {s.relative_to(ROOT)}")
        elif svg.read_text(encoding="utf-8") != expected:
            err(f"{svg.relative_to(ROOT)} veraltet – python3 scripts/board-image.py {s.relative_to(ROOT)}")


def check_bases_and_ci():
    boards = configparser.ConfigParser(inline_comment_prefixes=(";",))
    boards.read(ROOT / "boards" / "boards.ini", encoding="utf-8")
    workflow = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")
    m = re.search(r"project:\s*\[([^\]]*)\]", workflow)
    ci_projects = {p.strip() for p in m.group(1).split(",")} if m else set()
    for ini in sorted((ROOT / "projects").glob("*/platformio.ini")):
        name = ini.parent.name
        if name not in ci_projects:
            err(f".github/workflows/build.yml: Projekt {name} fehlt in matrix.project")
        for base in re.findall(r"^extends\s*=\s*(\S+)", ini.read_text(encoding="utf-8"), flags=re.M):
            if not base.startswith("env:") and not boards.has_section(base):
                err(f"{ini.relative_to(ROOT)}: Basis '{base}' fehlt in boards/boards.ini")


def main():
    check_links()
    check_board_pages()
    check_images()
    check_bases_and_ci()
    if errors:
        print("Doku-Prüfung fehlgeschlagen:")
        for e in errors:
            print("  -", e)
        return 1
    print("Doku-Prüfung ok.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
