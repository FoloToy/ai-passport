#!/usr/bin/env python3
"""Fail if radio C string literals use non-ASCII glyphs absent from radio_font_symbols.txt."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [
    ROOT / "main/radio_ui.c",
    ROOT / "main/radio_player.c",
    ROOT / "main/radio_stations.c",
]
SYMBOLS = ROOT / "main/radio_font_symbols.txt"
STR_RE = re.compile(r'"((?:\\.|[^"\\])*)"')


def unescape_c_string(s: str) -> str:
    out: list[str] = []
    i = 0
    while i < len(s):
        if s[i] != "\\" or i + 1 >= len(s):
            out.append(s[i])
            i += 1
            continue
        nxt = s[i + 1]
        simple = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"', "'": "'", "0": "\0"}
        if nxt in simple:
            out.append(simple[nxt])
            i += 2
            continue
        if nxt == "x" and i + 3 < len(s):
            out.append(chr(int(s[i + 2 : i + 4], 16)))
            i += 4
            continue
        out.append(nxt)
        i += 2
    return "".join(out)


def main() -> int:
    allowed = set(SYMBOLS.read_text(encoding="utf-8"))
    missing: dict[str, set[str]] = {}
    for path in SOURCES:
        text = path.read_text(encoding="utf-8")
        for m in STR_RE.finditer(text):
            raw = unescape_c_string(m.group(1))
            for ch in raw:
                o = ord(ch)
                if o <= 0x7E or ch.isspace():
                    continue
                if 0x80 <= o < 0xA0:
                    continue
                if ch not in allowed:
                    missing.setdefault(ch, set()).add(path.relative_to(ROOT).as_posix())
    if not missing:
        print("radio font glyphs: PASS")
        return 0
    print("radio font glyphs: FAIL — missing from main/radio_font_symbols.txt:", file=sys.stderr)
    for ch in sorted(missing, key=ord):
        files = ", ".join(sorted(missing[ch]))
        print(f"  U+{ord(ch):04X} '{ch}' in {files}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
