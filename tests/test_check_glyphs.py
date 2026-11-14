#!/usr/bin/env python3
"""Host tests for tools/check_glyphs.py (static LVGL glyph coverage)."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("check_glyphs", ROOT / "tools" / "check_glyphs.py")
assert SPEC and SPEC.loader
GLYPHS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GLYPHS)

# Shaped like lv_font_conv 1.5 output for LVGL 9: ASCII as FORMAT0_TINY, a dense
# block with one hole as FORMAT0_FULL, and scattered CJK as SPARSE_TINY.
# Comments deliberately contain characters and numbers that are not code points.
FONT_TEMPLATE = """\
/* Opts: --range 0x20-0x7E --symbols 中文显 (U+4E2D U+6587 08) */
#include "lvgl.h"
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+4E2D "中" */
    0x08, 0x09, 0xff
};
static const uint8_t glyph_id_ofs_list_1[] = {
    0, 1, 0, 2
};
static const uint16_t unicode_list_2[] = {
    0x0, 0x1, %(sparse)s
};
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 12289, .range_length = 4, .glyph_id_start = 96,
        .unicode_list = NULL, .glyph_id_ofs_list = glyph_id_ofs_list_1, .list_length = 4, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL
    },
    {
        .range_start = 20013, .range_length = 7000, .glyph_id_start = 99,
        .unicode_list = unicode_list_2, .glyph_id_ofs_list = NULL, .list_length = %(count)d, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};
const lv_font_t app_font_20 = {
    .line_height = 20,
};
"""
# 20013 = U+4E2D. Sparse deltas: 中 (0x0), 丮 (0x1), 文 U+6587 (0x175a), 显 U+663E (0x1811).
# FORMAT0_FULL: U+3001 、 (index 0), U+3002 。 (offset 1), U+3003 missing, U+3004 (offset 2).
ALL_SPARSE = "0x175a, 0x1811"


def font(sparse: str = ALL_SPARSE) -> str:
    return FONT_TEMPLATE % {"sparse": sparse, "count": 2 + len(sparse.split(","))}


class GlyphCoverageTest(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="ai-passport-glyph-tests-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()

    def write(self, name: str, content: str) -> None:
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def check(self) -> tuple[int, str]:
        output = io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            code = GLYPHS.main(["--root", str(self.root)])
        return code, output.getvalue()

    def test_font_cmap_parsing(self) -> None:
        points = GLYPHS.font_codepoints(font(), "font.c")
        self.assertIsNotNone(points)
        assert points is not None
        self.assertTrue(set(range(0x20, 0x7F)) <= points)
        self.assertTrue({0x3001, 0x3002, 0x3004} <= points)
        self.assertNotIn(0x3003, points)          # zero offset after range_start
        self.assertTrue({0x4E2D, 0x4E2E, 0x6587, 0x663E} <= points)
        self.assertNotIn(0x4E00, points)
        self.assertIsNone(GLYPHS.font_codepoints('const char *s = "中";', "x.c"))

    def test_without_bundled_font_is_skipped(self) -> None:
        self.write("main/app.c", 'void f(void) { lv_label_set_text(l, "中文"); }\n')
        code, output = self.check()
        self.assertEqual(code, 0)
        self.assertIn("SKIPPED", output)

    def test_clean_tree_passes(self) -> None:
        self.write("assets/fonts/app_font_20.c", font())
        self.write("main/app.c", 'void f(void) { lv_label_set_text(l, "中文显。ABC"); }\n')
        code, output = self.check()
        self.assertEqual(code, 0, output)
        self.assertIn("PASS", output)

    def test_deleted_glyph_fails_with_character_and_line(self) -> None:
        self.write("assets/fonts/app_font_20.c", font("0x175a"))   # 显 U+663E removed
        self.write("main/app.c", "\n".join([
            "#include \"lvgl.h\"",
            "void f(void) {",
            '    lv_label_set_text(title, "中文");',
            '    lv_label_set_text(body, "显");',
            "}",
        ]) + "\n")
        code, output = self.check()
        self.assertEqual(code, 1)
        self.assertIn("main/app.c:4: U+663E '显'", output)
        self.assertNotIn("U+4E2D", output)

    def test_escapes_defines_and_snprintf_are_checked(self) -> None:
        self.write("main/app_font.c", font())   # fonts may live in main/ too
        self.write("main/ui.h", '#define TAB_TITLE "\\u4E00"\n')
        self.write("main/ui.c", "\n".join([
            "void f(char *b) {",
            '    snprintf(b, 8, "%d\\U00009F98", 1);',
            '    lv_label_set_text(l, u8"\\u4E2D\\x41\\n");',
            "}",
        ]) + "\n")
        code, output = self.check()
        self.assertEqual(code, 1)
        self.assertIn("main/ui.h:1: U+4E00", output)
        self.assertIn("main/ui.c:2: U+9F98", output)
        self.assertNotIn("U+4E2D", output)
        self.assertNotIn("main/app_font.c", output.split("missing from")[0])

    def test_non_display_text_is_skipped(self) -> None:
        self.write("assets/fonts/app_font_20.c", font())
        self.write("main/app.c", "\n".join([
            '#include "龘.h"',
            "// lv_label_set_text(l, \"龘\");",
            "/* \"龘\" */",
            "#if 0",
            "#ifdef X",
            'const char *a = "龘";',
            "#endif",
            'const char *b = "龘";',
            "#endif",
            "void f(int n) {",
            '    ESP_LOGI(TAG, "龘 %d",',
            '             g("龘"));',
            '    printf("龘");',
            "    char q = '\"';",
            '    lv_label_set_text(l, "龘"); // glyph-check: ignore',
            "}",
        ]) + "\n")
        code, output = self.check()
        self.assertEqual(code, 0, output)

    def test_if_zero_else_branch_is_checked(self) -> None:
        self.write("assets/fonts/app_font_20.c", font())
        self.write("main/app.c", "\n".join([
            "#if 0",
            'const char *a = "龘";',
            "#else",
            'const char *b = "龘";',
            "#endif",
        ]) + "\n")
        code, output = self.check()
        self.assertEqual(code, 1)
        self.assertIn("main/app.c:4: U+9F98", output)
        self.assertNotIn("main/app.c:2:", output)

    def test_unknown_cmap_type_is_an_error(self) -> None:
        self.write("assets/fonts/app_font_20.c",
                   font().replace("SPARSE_TINY", "MYSTERY"))
        self.write("main/app.c", 'const char *s = "中";\n')
        code, output = self.check()
        self.assertEqual(code, 1)
        self.assertIn("unsupported cmap type", output)


if __name__ == "__main__":
    unittest.main()
