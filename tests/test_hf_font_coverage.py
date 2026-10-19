#!/usr/bin/env python3
"""Verify the committed font inventory covers the application's UI text."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "assets" / "fonts" / "hf_symbols.txt"
SOURCES = [
    ROOT / "main" / "hf_app_ui.c",
    ROOT / "main" / "prop_model.c",
]
FONT_FILES = [ROOT / "assets" / "fonts" / f"hf_font_{size}.c"
              for size in (14, 16, 20, 28)]
UNICODE_LIST_RE = re.compile(
    r"static const (?:uint16_t|uint32_t) (unicode_list_\d+)\[\] = \{(.*?)\};",
    re.S,
)
CMAP_RE = re.compile(
    r"\.range_start = (\d+), \.range_length = (\d+), "
    r"\.glyph_id_start = \d+,\s*\.unicode_list = (unicode_list_\d+|NULL),"
    r".*?\.type = (\w+)",
    re.S,
)


def source_symbols() -> set[str]:
    symbols: set[str] = set()
    for path in SOURCES:
        text = path.read_text(encoding="utf-8")
        symbols.update(ch for ch in text if ord(ch) >= 0x80 and ch not in "\r\n\t")
    return symbols


def inventory_symbols() -> set[str]:
    if not INVENTORY.is_file():
        raise RuntimeError(f"missing font inventory: {INVENTORY}")
    symbols: set[str] = set()
    for line in INVENTORY.read_text(encoding="utf-8").splitlines():
        match = re.match(r"^U\+([0-9A-Fa-f]+)\s+(.+)$", line)
        if match:
            codepoint = int(match.group(1), 16)
            expected = match.group(2)
            if chr(codepoint) != expected:
                raise RuntimeError(
                    f"inventory mismatch for U+{codepoint:04X}: {expected!r}"
                )
            symbols.add(expected)
    return symbols


def font_codepoints(path: Path) -> set[int]:
    text = path.read_text(encoding="utf-8")
    arrays: dict[str, list[int]] = {}
    for match in UNICODE_LIST_RE.finditer(text):
        arrays[match.group(1)] = [
            int(value, 0)
            for value in re.findall(r"0x[0-9A-Fa-f]+|\d+", match.group(2))
        ]

    covered: set[int] = set()
    for match in CMAP_RE.finditer(text):
        start = int(match.group(1))
        length = int(match.group(2))
        list_name = match.group(3)
        if list_name == "NULL":
            covered.update(range(start, start + length))
            continue
        offsets = arrays.get(list_name)
        if offsets is None:
            raise RuntimeError(f"{path}: missing {list_name}")
        covered.update(start + offset for offset in offsets)
    return covered


def main() -> int:
    missing_files = [path for path in FONT_FILES if not path.is_file()]
    if missing_files:
        for path in missing_files:
            print(f"ERROR: missing generated font: {path}", file=sys.stderr)
        return 1

    required = source_symbols()
    available = inventory_symbols()
    missing = sorted(required - available, key=ord)
    if missing:
        for char in missing:
            print(f"ERROR: U+{ord(char):04X} {char!r} not in font inventory",
                  file=sys.stderr)
        print("Run tools/gen_hf_font.py after changing UI text.", file=sys.stderr)
        return 1

    for path in FONT_FILES:
        text = path.read_text(encoding="utf-8")
        name = path.stem
        if name not in text:
            print(f"ERROR: {path} does not define {name}", file=sys.stderr)
            return 1
        covered = font_codepoints(path)
        missing_in_font = sorted(
            (char for char in required if ord(char) not in covered), key=ord
        )
        if missing_in_font:
            for char in missing_in_font:
                print(
                    f"ERROR: {path.name} lacks U+{ord(char):04X} {char!r}",
                    file=sys.stderr,
                )
            return 1

    print(f"font coverage: PASS ({len(required)} UI glyphs verified in 4 fonts)")
    return 0


if __name__ == "__main__":
    sys.exit(main())

