#!/usr/bin/env python3
"""Generate the minimal 16 px CJK font used by The Last Room."""

from pathlib import Path
import os
import re
import subprocess


ROOT = Path(__file__).resolve().parents[1]
TEXT_SOURCE = ROOT / "main" / "game_i18n.c"
FONT_SOURCE = (
    ROOT
    / "managed_components"
    / "lvgl__lvgl"
    / "tests"
    / "src"
    / "test_files"
    / "fonts"
    / "noto"
    / "NotoSansSC-Regular.ttf"
)
ASSET_DIR = ROOT / "assets" / "fonts"
INVENTORY = ASSET_DIR / "game_cjk_characters.txt"
OUTPUT = ASSET_DIR / "game_cjk_16.c"


def main() -> None:
    if not FONT_SOURCE.is_file():
        raise SystemExit(
            "Noto Sans SC source is missing; restore the pinned LVGL managed dependency first"
        )

    source = TEXT_SOURCE.read_text(encoding="utf-8")
    string_literals = re.findall(r'"(?:\\.|[^"\\])*"', source)
    symbols = "".join(sorted({
        char for literal in string_literals for char in literal if ord(char) > 0x7F
    }))
    if not symbols:
        raise SystemExit("no non-ASCII game text found")

    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    INVENTORY.write_text(symbols + "\n", encoding="utf-8")

    environment = os.environ.copy()
    environment.setdefault("npm_config_cache", "/tmp/the-last-room-npm-cache")
    subprocess.run(
        [
            "npx",
            "--yes",
            "lv_font_conv@1.5.3",
            "--no-compress",
            "--no-prefilter",
            "--bpp",
            "4",
            "--size",
            "16",
            "--font",
            str(FONT_SOURCE),
            "--symbols",
            symbols,
            "--format",
            "lvgl",
            "--lv-font-name",
            "game_cjk_16",
            "--lv-include",
            "lvgl.h",
            "--output",
            str(OUTPUT),
        ],
        check=True,
        cwd=ROOT,
        env=environment,
    )
    generated = OUTPUT.read_text(encoding="utf-8")
    OUTPUT.write_text(
        generated.replace(str(ROOT) + os.sep, ""),
        encoding="utf-8",
    )
    print(f"generated {OUTPUT.relative_to(ROOT)} with {len(symbols)} glyphs")


if __name__ == "__main__":
    main()
