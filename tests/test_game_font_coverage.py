#!/usr/bin/env python3

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
text_source = (ROOT / "main" / "game_i18n.c").read_text(encoding="utf-8")
string_literals = re.findall(r'"(?:\\.|[^"\\])*"', text_source)
required = {
    char for literal in string_literals for char in literal if ord(char) > 0x7F
}

inventory_path = ROOT / "assets" / "fonts" / "game_cjk_characters.txt"
inventory = set(inventory_path.read_text(encoding="utf-8").rstrip("\n"))
assert inventory == required, (
    "font inventory is stale; run python3 tools/generate_game_font.py"
)

font_source = (ROOT / "assets" / "fonts" / "game_cjk_16.c").read_text(
    encoding="utf-8"
)
covered = {
    chr(int(codepoint, 16))
    for codepoint in re.findall(r"/\* U\+([0-9A-Fa-f]{4,6})", font_source)
}
missing = sorted(required - covered)
assert not missing, "missing generated glyphs: " + " ".join(
    f"U+{ord(char):04X}" for char in missing
)
assert "龘" not in covered, "negative coverage probe unexpectedly exists"

print(f"game font coverage: PASS ({len(required)} glyphs)")
