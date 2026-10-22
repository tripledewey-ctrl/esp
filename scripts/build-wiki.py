#!/usr/bin/env python3
"""Erzeugt die GitHub-Wiki-Seiten aus der Doku im Repo.

    python3 scripts/build-wiki.py [ausgabeordner]     # Standard: wiki-build/

Die Doku im Repo ist die einzige Quelle. Das Wiki wird daraus generiert und von
.github/workflows/wiki.yml bei jedem Push auf main veröffentlicht. Änderungen
deshalb immer im Repo machen, nie direkt im Wiki.

Was das Skript tut:
  - jede Quelldatei aus SOURCES wird eine Wiki-Seite (Name aus der H1-Überschrift)
  - Links zwischen Doku-Dateien werden zu Wiki-Links umgeschrieben
  - Links auf andere Repo-Dateien zeigen auf GitHub (blob/tree auf main)
  - Bilder werden nach images/ im Wiki kopiert
  - _Sidebar.md (Navigation) und _Footer.md werden erzeugt
"""
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO_URL = "https://github.com/tripledewey-ctrl/esp"
BRANCH = "main"

# (Gruppe in der Seitenleiste, Glob-Muster) – Reihenfolge = Reihenfolge im Wiki
SOURCES = [
    ("Start", "README.md"),
    ("Anleitungen", "docs/0[1-2]-*.md"),
    ("Anleitungen", "docs/0[4-9]-*.md"),
    ("Boards", "docs/boards/README.md"),
    ("Boards", "docs/boards/[!_]*.md"),
    ("WLED", "wled/README.md"),
    ("WLED", "wled/config/README.md"),
    ("Projekte", "projects/[!_]*/README.md"),
    ("Werkzeuge", "tools/*/README.md"),
]
# Feste Seitennamen, wo die Überschrift nicht passt
PAGE_NAMES = {
    "README.md": "Home",
    "docs/02-boards.md": "Board-Vergleich",
    "docs/boards/README.md": "Boards-im-Detail",
    "wled/config/README.md": "WLED-Presets-und-Sicherung",
}

LINK = re.compile(r"(!?)\[([^\]]*)\]\(([^)\s]+)\)")
IMG_TAG = re.compile(r'(<img[^>]+src=")([^"]+)(")')


def collect():
    pages, seen = [], set()
    for group, pattern in SOURCES:
        for path in sorted(ROOT.glob(pattern)):
            rel = path.relative_to(ROOT).as_posix()
            if rel in seen:
                continue
            seen.add(rel)
            pages.append((group, rel, page_name(rel, path), title(path)))
    return pages


def title(path):
    m = re.search(r"^# (.+)$", path.read_text(encoding="utf-8"), flags=re.M)
    t = m.group(1) if m else path.stem
    return re.sub(r"^\d+\s*[–-]\s*", "", t)             # "1 – Einrichtung" -> "Einrichtung"


def page_name(rel, path):
    if rel in PAGE_NAMES:
        return PAGE_NAMES[rel]
    t = title(path)
    if rel.startswith("projects/"):
        t = "Projekt " + t
    elif rel.startswith("tools/"):
        t = "Werkzeug " + t
    t = t.translate(str.maketrans({"ä": "ae", "ö": "oe", "ü": "ue", "Ä": "Ae", "Ö": "Oe", "Ü": "Ue", "ß": "ss"}))
    return re.sub(r"[^A-Za-z0-9.]+", "-", t).strip("-")


def rewrite(text, src_rel, names, images):
    src_dir = (ROOT / src_rel).parent

    def target(url):
        if re.match(r"^(https?:|mailto:|#)", url):
            return url
        path, _, frag = url.partition("#")
        resolved = (src_dir / path).resolve()
        try:
            rel = resolved.relative_to(ROOT).as_posix()
        except ValueError:
            return url
        if rel in names:
            return names[rel] + (f"#{frag}" if frag else "")
        if resolved.is_dir() and (resolved / "README.md").relative_to(ROOT).as_posix() in names:
            return names[(resolved / "README.md").relative_to(ROOT).as_posix()]
        if resolved.suffix.lower() in (".svg", ".png", ".jpg", ".jpeg", ".gif") and resolved.exists():
            images[resolved.name] = resolved
            return f"images/{resolved.name}"
        kind = "tree" if resolved.is_dir() else "blob"
        return f"{REPO_URL}/{kind}/{BRANCH}/{rel}" + (f"#{frag}" if frag else "")

    # Codeblöcke unverändert lassen
    parts = re.split(r"(```.*?```)", text, flags=re.S)
    for i in range(0, len(parts), 2):
        parts[i] = LINK.sub(lambda m: f"{m.group(1)}[{m.group(2)}]({target(m.group(3))})", parts[i])
        parts[i] = IMG_TAG.sub(lambda m: m.group(1) + target(m.group(2)) + m.group(3), parts[i])
    return "".join(parts)


def main(argv):
    out = Path(argv[0]) if argv else ROOT / "wiki-build"
    if out.exists():
        for p in out.iterdir():
            if p.name != ".git":
                shutil.rmtree(p) if p.is_dir() else p.unlink()
    out.mkdir(parents=True, exist_ok=True)

    pages = collect()
    names = {rel: name for _, rel, name, _ in pages}
    images = {}
    for _, rel, name, _ in pages:
        text = (ROOT / rel).read_text(encoding="utf-8")
        # Hinweis "Doku gibt es auch als Wiki" ist im Wiki selbst überflüssig
        text = re.sub(r"^> 📖.*\n(?:>.*\n)*\n?", "", text, flags=re.M)
        text = rewrite(text, rel, names, images)
        text += (f"\n\n---\n<sub>Automatisch erzeugt aus [`{rel}`]({REPO_URL}/blob/{BRANCH}/{rel}). "
                 f"Änderungen bitte im Repo vornehmen.</sub>\n")
        (out / f"{name}.md").write_text(text, encoding="utf-8")

    (out / "images").mkdir(exist_ok=True)
    for fname, src in images.items():
        shutil.copy(src, out / "images" / fname)

    sidebar, last = ["**[[Home]]**", ""], None
    for group, rel, name, label in pages:
        if group == "Start":
            continue
        if group != last:
            sidebar += ["", f"**{group}**", ""]
            last = group
        sidebar.append(f"- [{label}]({name})")
    (out / "_Sidebar.md").write_text("\n".join(sidebar) + "\n", encoding="utf-8")
    (out / "_Footer.md").write_text(
        f"Quelle: [{REPO_URL.split('github.com/')[1]}]({REPO_URL}) · "
        "Wiki wird aus `docs/`, `wled/` und `projects/` erzeugt (`scripts/build-wiki.py`).\n",
        encoding="utf-8")

    print(f"{len(pages)} Seiten, {len(images)} Bilder -> {out}")
    for _, rel, name, _ in pages:
        print(f"  {name:32} <- {rel}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
