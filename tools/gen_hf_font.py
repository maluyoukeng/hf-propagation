#!/usr/bin/env python3
"""Generate the shortwave application's LVGL font subsets.

The glyph inventory is every non-ASCII character used by the UI sources
(main/hf_app_ui.c and main/prop_model.c), plus the common CJK punctuation used
by the UI. Browser-only WiFi portal strings in app_wifi.c are not part of
the on-device font. The generated C files are committed so firmware builds do
not require Node.js.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = ROOT / "assets" / "fonts"
SIZES = (14, 16, 20, 28)
EXTRA_PUNCTUATION = "，。、：；！？（）《》“”‘’…—·°"


TEXT_SOURCES = (
    ROOT / "main" / "hf_app_ui.c",
    ROOT / "main" / "prop_model.c",
)


def collect_symbols() -> list[str]:
    chars: set[str] = set()
    for path in TEXT_SOURCES:
        text = path.read_text(encoding="utf-8")
        for char in text:
            if ord(char) >= 0x80 and char not in "\r\n\t":
                chars.add(char)
    chars.update(EXTRA_PUNCTUATION)
    return sorted(chars, key=ord)


def write_inventory(symbols: list[str]) -> Path:
    FONT_DIR.mkdir(parents=True, exist_ok=True)
    path = FONT_DIR / "hf_symbols.txt"
    lines = ["# Shortwave UI glyph inventory. Regenerate with tools/gen_hf_font.py."]
    for char in symbols:
        lines.append(f"U+{ord(char):04X} {char}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def generate(font: Path, symbols: list[str], npx: str) -> None:
    text = "".join(symbols)
    for size in SIZES:
        output = FONT_DIR / f"hf_font_{size}.c"
        command = [
            npx,
            "--yes",
            "lv_font_conv@1.5.2",
            "--font",
            str(font),
            "--range",
            "0x20-0x7E",
            "--symbols",
            text,
            "--size",
            str(size),
            "--bpp",
            "4",
            "--format",
            "lvgl",
            "--lv-include",
            "lvgl.h",
            "--no-compress",
            "--output",
            str(output),
        ]
        print(f"generating {output.name} ({len(symbols)} glyphs)", flush=True)
        subprocess.run(command, check=True)
        if not output.exists() or output.stat().st_size == 0:
            raise RuntimeError(f"converter produced no output for {output}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--font",
        default=os.environ.get("HF_FONT_SOURCE", "NotoSansSC-Regular.otf"),
        help="licensed TTF/OTF containing the required glyphs; "
             "or set HF_FONT_SOURCE",
    )
    args = parser.parse_args()

    font = Path(args.font)
    if not font.is_file():
        print(f"ERROR: font not found: {font}", file=sys.stderr)
        print("Set HF_FONT_SOURCE or pass --font.", file=sys.stderr)
        return 1

    npx = shutil.which("npx")
    if not npx:
        print("ERROR: npx was not found; install Node.js first.", file=sys.stderr)
        return 1

    symbols = collect_symbols()
    inventory = write_inventory(symbols)
    print(f"wrote {inventory} ({len(symbols)} glyphs)")
    generate(font, symbols, npx)
    print("font generation complete")
    return 0


if __name__ == "__main__":
    sys.exit(main())

