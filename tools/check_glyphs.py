#!/usr/bin/env python3
"""Static glyph-coverage check for bundled LVGL bitmap fonts.

A string whose characters are missing from the font is valid UTF-8, compiles,
links, and then shows an empty box on the panel. This check reads the code
points that the generated font sources actually contain (their cmap tables, not
the converter's request) and fails when an application string literal uses a
character outside that set.

Scope and rules:

* Fonts: every C file under the font directories (default `assets/fonts` and
  `main`) that defines an `lv_font_t` with `lv_font_fmt_txt_cmap_t` tables. The
  check uses the union of all bundled fonts; it cannot know which font a widget
  selects at runtime.
* Strings: string literals in C/C++ sources under the source directories
  (default `main`), including `#define` bodies. Comments, `#if 0` blocks,
  `#include` paths, and literals passed to logging/printing calls
  (`ESP_LOG*`, `printf`, `puts`, ...) are skipped. `snprintf` is checked,
  because it often builds display text.
* Code points below U+0080 are not checked: the baseline enables the built-in
  Montserrat fonts for ASCII. `\\uXXXX` and `\\UXXXXXXXX` escapes are decoded;
  `\\x` and octal byte escapes are not.
* A line whose comment contains `glyph-check: ignore` is skipped, for literals
  that are never displayed or are covered by a font outside this repository.
* A tree without a bundled font is skipped, so the BSP baseline and
  applications that use only built-in fonts are unaffected.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import NamedTuple


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_FONT_DIRS = ("assets/fonts", "main")
DEFAULT_SOURCE_DIRS = ("main",)
SOURCE_SUFFIXES = {".c", ".h", ".cc", ".cpp", ".hpp"}
IGNORE_MARKER = "glyph-check: ignore"
ASCII_LIMIT = 0x80

# Calls whose string arguments go to a console or an assertion, not to the panel.
NON_DISPLAY_CALL_RE = re.compile(
    r"^(?:ESP_(?:EARLY_|DRAM_)?LOG[EWIDV]|ESP_LOG_BUFFER\w*|ESP_LOG_LEVEL\w*"
    r"|ESP_(?:RETURN|GOTO)_ON_\w+|ESP_ERROR_CHECK\w*|LV_LOG\w*"
    r"|printf|fprintf|vprintf|vfprintf|puts|fputs|perror"
    r"|assert|static_assert|_Static_assert)$"
)
NON_DISPLAY_DIRECTIVES = {"include", "include_next", "pragma", "error", "warning", "line"}

FONT_DEFINITION_RE = re.compile(r"\blv_font_t\s+\w+\s*=")
CMAP_ARRAY_RE = re.compile(
    r"lv_font_fmt_txt_cmap_t\s+(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\};", re.DOTALL
)
UNICODE_LIST_RE = re.compile(
    r"uint16_t\s+(\w+)\s*\[\s*\]\s*=\s*\{([^}]*)\}", re.DOTALL
)
OFFSET_LIST_RE = re.compile(
    r"uint(?:8|16)_t\s+(\w+)\s*\[\s*\]\s*=\s*\{([^}]*)\}", re.DOTALL
)
INT_RE = re.compile(r"0[xX][0-9a-fA-F]+|[1-9]\d*|0")
COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.DOTALL)
FIELD_RE = re.compile(r"\.(\w+)\s*=\s*([^,}\s]+)")


class FontError(Exception):
    """A font file defines a cmap the checker cannot interpret."""


def parse_ints(body: str) -> list[int]:
    return [int(token, 0) for token in INT_RE.findall(body)]


def split_entries(body: str) -> list[str]:
    """Split the top-level `{...}` entries of a cmap array body."""
    entries: list[str] = []
    depth = 0
    start = 0
    for index, char in enumerate(body):
        if char == "{":
            if depth == 0:
                start = index + 1
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                entries.append(body[start:index])
    return entries


def font_codepoints(text: str, name: str) -> set[int] | None:
    """Return the code points a generated font carries, or None if not a font."""
    if not FONT_DEFINITION_RE.search(text):
        return None
    arrays = CMAP_ARRAY_RE.findall(text)
    if not arrays:
        return None
    text = COMMENT_RE.sub(" ", text)
    arrays = CMAP_ARRAY_RE.findall(text)

    def named_list(pattern: re.Pattern[str], name: str) -> list[int] | None:
        match = re.search(pattern.pattern.replace(r"(\w+)", re.escape(name), 1), text,
                          re.DOTALL)
        return parse_ints(match.group(1)) if match else None

    points: set[int] = set()
    for _, body in arrays:
        for entry in split_entries(body):
            fields = dict(FIELD_RE.findall(entry))
            try:
                start = int(fields["range_start"], 0)
                length = int(fields["range_length"], 0)
                kind = fields["type"]
            except (KeyError, ValueError) as error:
                raise FontError(f"{name}: unreadable cmap entry ({error})") from error
            if kind.endswith("FORMAT0_TINY"):
                points.update(range(start, start + length))
            elif kind.endswith("FORMAT0_FULL"):
                offsets = named_list(OFFSET_LIST_RE, fields.get("glyph_id_ofs_list", ""))
                if offsets is None:
                    raise FontError(f"{name}: missing glyph_id_ofs_list for U+{start:04X}")
                # LVGL treats a zero offset as "missing", except for range_start.
                points.update(
                    start + index
                    for index, offset in enumerate(offsets[:length])
                    if offset or index == 0
                )
            elif kind.endswith("SPARSE_TINY") or kind.endswith("SPARSE_FULL"):
                deltas = named_list(UNICODE_LIST_RE, fields.get("unicode_list", ""))
                if deltas is None:
                    raise FontError(f"{name}: missing unicode_list for U+{start:04X}")
                count = int(fields.get("list_length", str(len(deltas))), 0)
                points.update(start + delta for delta in deltas[:count])
            else:
                raise FontError(f"{name}: unsupported cmap type {kind}")
    return points


class Literal(NamedTuple):
    line: int
    codepoints: list[int]


def decode_literal(body: str) -> list[int]:
    """Decode the code points a C string literal body produces (ASCII escapes ignored)."""
    points: list[int] = []
    index = 0
    while index < len(body):
        char = body[index]
        if char != "\\":
            points.append(ord(char))
            index += 1
            continue
        kind = body[index + 1] if index + 1 < len(body) else ""
        if kind in "uU":
            width = 4 if kind == "u" else 8
            digits = body[index + 2 : index + 2 + width]
            if len(digits) == width and all(c in "0123456789abcdefABCDEF" for c in digits):
                points.append(int(digits, 16))
                index += 2 + width
                continue
        index += 2  # \n, \", \x.., octal: byte-level escapes are not decoded
    return points


def scan_source(text: str) -> list[Literal]:
    """Return the display-candidate string literals of one C/C++ source."""
    literals: list[Literal] = []
    ignored_lines: set[int] = set()
    calls: list[str | None] = []
    skip_depth = 0          # >0 while inside an `#if 0` block
    if_depth = 0            # nesting of conditionals opened inside a skipped block
    last_identifier: str | None = None
    line = 1
    index = 0
    at_line_start = True
    length = len(text)

    def take_literal(start: int, quote: str) -> tuple[str, int]:
        """Return (body, end index after the closing quote)."""
        position = start
        while position < length and text[position] != quote:
            if text[position] == "\\":
                position += 1
            elif text[position] == "\n":
                break  # unterminated; let the compiler report it
            position += 1
        return text[start:position], position + 1

    def directive(position: int) -> int:
        """Handle a preprocessor line starting at `#`; return the index after it."""
        nonlocal skip_depth, if_depth, line
        end = position
        content = []
        while end < length:
            if text.startswith("\\\n", end):
                end += 2
                line += 1
                content.append(" ")
                continue
            if text[end] == "\n":
                break
            if text.startswith("//", end):
                comment_end = text.find("\n", end)
                comment_end = length if comment_end < 0 else comment_end
                if IGNORE_MARKER in text[end:comment_end]:
                    ignored_lines.add(line)
                end = comment_end
                break
            if text.startswith("/*", end):
                close = text.find("*/", end + 2)
                close = length if close < 0 else close + 2
                comment = text[end:close]
                if IGNORE_MARKER in comment:
                    ignored_lines.add(line)
                line += comment.count("\n")
                end = close
                content.append(" ")
                continue
            content.append(text[end])
            end += 1
        words = "".join(content)[1:].split()
        keyword = words[0] if words else ""
        argument = " ".join(words[1:])
        if skip_depth:
            if keyword in {"if", "ifdef", "ifndef"}:
                if_depth += 1
            elif keyword == "endif":
                if if_depth:
                    if_depth -= 1
                else:
                    skip_depth = 0
            elif keyword in {"else", "elif"} and not if_depth:
                skip_depth = 0
            return end
        if keyword == "if" and argument == "0":
            skip_depth = 1
            if_depth = 0
            return end
        if keyword in NON_DISPLAY_DIRECTIVES:
            return end
        # Scan string literals in the remaining directive text (e.g. #define bodies).
        body = "".join(content)
        cursor = 0
        while cursor < len(body):
            char = body[cursor]
            if char in "\"'":
                close = cursor + 1
                while close < len(body) and body[close] != char:
                    close += 2 if body[close] == "\\" else 1
                if char == '"':
                    literals.append(Literal(line, decode_literal(body[cursor + 1 : close])))
                cursor = close + 1
            else:
                cursor += 1
        return end

    while index < length:
        char = text[index]
        if char == "\n":
            line += 1
            index += 1
            at_line_start = True
            continue
        if char in " \t\r\f\v":
            index += 1
            continue
        if at_line_start and char == "#":
            index = directive(index)
            continue
        at_line_start = False
        if text.startswith("//", index):
            end = text.find("\n", index)
            end = length if end < 0 else end
            if IGNORE_MARKER in text[index:end]:
                ignored_lines.add(line)
            index = end
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = length if end < 0 else end + 2
            comment = text[index:end]
            if IGNORE_MARKER in comment:
                ignored_lines.add(line)
            line += comment.count("\n")
            index = end
            continue
        if char == '"' or char == "'":
            start_line = line
            body, index = take_literal(index + 1, char)
            if char == '"' and not skip_depth:
                display = not any(name and NON_DISPLAY_CALL_RE.match(name) for name in calls)
                if display:
                    literals.append(Literal(start_line, decode_literal(body)))
            last_identifier = None
            continue
        if char.isalpha() or char == "_":
            end = index
            while end < length and (text[end].isalnum() or text[end] == "_"):
                end += 1
            word = text[index:end]
            # String prefixes: u8"..", L"..", u"..", U"..".
            if word in {"u8", "L", "u", "U"} and end < length and text[end] == '"':
                index = end
                continue
            last_identifier = word
            index = end
            continue
        if char == "(":
            calls.append(last_identifier)
        elif char == ")":
            if calls:
                calls.pop()
        last_identifier = None
        index += 1

    return [literal for literal in literals if literal.line not in ignored_lines]


def iter_files(root: Path, directories: tuple[str, ...], suffixes: set[str]) -> list[Path]:
    files: set[Path] = set()
    for directory in directories:
        base = root / directory
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if path.is_file() and path.suffix in suffixes:
                files.add(path)
    return sorted(files)


def run(root: Path, font_dirs: tuple[str, ...], source_dirs: tuple[str, ...]) -> int:
    covered: set[int] = set()
    font_files: set[Path] = set()
    errors: list[str] = []
    for path in iter_files(root, font_dirs, {".c"}):
        text = path.read_text(encoding="utf-8", errors="replace")
        try:
            points = font_codepoints(text, path.relative_to(root).as_posix())
        except FontError as error:
            errors.append(str(error))
            continue
        if points is not None:
            covered |= points
            font_files.add(path)

    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    if not font_files:
        print("Glyph coverage: SKIPPED (no bundled LVGL font)")
        return 0

    missing: dict[int, list[str]] = {}
    checked: set[int] = set()
    for path in iter_files(root, source_dirs, SOURCE_SUFFIXES):
        if path in font_files:
            continue
        relative = path.relative_to(root).as_posix()
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as error:
            print(f"ERROR: {relative}: not valid UTF-8 ({error})", file=sys.stderr)
            return 1
        for literal in scan_source(text):
            for point in literal.codepoints:
                if point < ASCII_LIMIT:
                    continue
                checked.add(point)
                if point not in covered:
                    location = f"{relative}:{literal.line}"
                    places = missing.setdefault(point, [])
                    if location not in places:
                        places.append(location)

    names = ", ".join(path.relative_to(root).as_posix() for path in sorted(font_files))
    if missing:
        print(f"Glyph coverage: FAIL ({len(missing)} code point(s) missing from {names})",
              file=sys.stderr)
        for point in sorted(missing):
            shown = chr(point) if chr(point).isprintable() else "?"
            for location in missing[point]:
                print(f"  {location}: U+{point:04X} '{shown}'", file=sys.stderr)
        print("Regenerate the font with these characters, or mark a line that is never "
              f"displayed with a '{IGNORE_MARKER}' comment.", file=sys.stderr)
        return 1
    print(f"Glyph coverage: PASS ({len(checked)} non-ASCII code point(s) covered by "
          f"{len(font_files)} font(s))")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=ROOT, help="repository root")
    parser.add_argument("--fonts", action="append", metavar="DIR",
                        help="font directory, repeatable (default: assets/fonts, main)")
    parser.add_argument("--sources", action="append", metavar="DIR",
                        help="source directory, repeatable (default: main)")
    args = parser.parse_args(argv)
    return run(args.root.resolve(), tuple(args.fonts or DEFAULT_FONT_DIRS),
               tuple(args.sources or DEFAULT_SOURCE_DIRS))


if __name__ == "__main__":
    sys.exit(main())
