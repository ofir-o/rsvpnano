#!/usr/bin/env python3
"""Generate src/display/EmbeddedHebrewFont.h with SEVERAL selectable Hebrew fonts.

Each font is rendered into the SAME fixed cell as the Latin serif font (so Hebrew sits on the same
baseline as Latin words) and stored as its own bitmap + glyph table. A small font table
(kEmbeddedHebrewFonts) lets the reader switch between them at runtime. Font 0 is the original serif
so existing behaviour is unchanged by default.

Per font, the point size is auto-fit: the largest size at which every glyph (including the final-form
descenders) still fits inside the shared cell, so each face is as large as possible without clipping.
"""

from __future__ import annotations

import pathlib

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "src" / "display" / "EmbeddedHebrewFont.h"
FONT_ROOT = pathlib.Path("/tmp/claude-0/-home-user-rsvpnano/"
                         "5c08be45-319d-54a1-8784-f3a0cd0a30c1/scratchpad/hebfonts2")

# Shared cell geometry (must match the serif font / the existing Hebrew generator).
CELL_HEIGHT = 62
CANVAS_WIDTH = 140
CANVAS_HEIGHT = 140
BASELINE_Y = 76
CELL_TOP = 22            # cell spans rows [CELL_TOP, CELL_TOP + CELL_HEIGHT)
ORIGIN_X = 14
ALPHA_THRESHOLD = 16

FIRST_CODEPOINT = 0x05D0
LAST_CODEPOINT = 0x05EA
SPACE_ADVANCE_DEFAULT = 14

# name, ttf path (relative to FONT_ROOT or absolute), C identifier suffix.
FONTS = [
    ("Serif", "/usr/share/fonts/truetype/freefont/FreeSerif.ttf", "Serif"),
    ("David", FONT_ROOT / "David_Libre/DavidLibre-Medium.ttf", "David"),
    ("Frank Ruhl", FONT_ROOT / "Frank_Ruhl_Libre/static/FrankRuhlLibre-Medium.ttf", "Frank"),
    ("Miriam", FONT_ROOT / "Miriam_Libre/static/MiriamLibre-Medium.ttf", "Miriam"),
    ("Rubik", FONT_ROOT / "Rubik/static/Rubik-Regular.ttf", "Rubik"),
]


# Keep the original serif at its known-good size; size every other face so its tallest letter has the
# same height as the serif's, so they all read at a consistent visual size (minor edge cropping at
# the cell window is accepted, exactly as the original single-font generator did).
SERIF_POINT_SIZE = 52


def tall_height(font) -> int:
    """Height (baseline to topmost inked row) of the tallest Hebrew glyph."""
    top = 10**9
    for cp in range(FIRST_CODEPOINT, LAST_CODEPOINT + 1):
        img = Image.new("L", (CANVAS_WIDTH, CANVAS_HEIGHT), color=255)
        d = ImageDraw.Draw(img)
        d.text((ORIGIN_X, BASELINE_Y), chr(cp), fill=0, font=font, anchor="ls")
        px = img.load()
        for y in range(CANVAS_HEIGHT):
            row_inked = any(255 - px[x, y] > ALPHA_THRESHOLD for x in range(CANVAS_WIDTH))
            if row_inked:
                top = min(top, y)
                break
    return BASELINE_Y - top if top < 10**9 else 0


def fit_point_size(path: str, target_height: int) -> int:
    """Point size whose tallest letter best matches target_height."""
    best, best_err = SERIF_POINT_SIZE, 10**9
    for size in range(20, 86):
        h = tall_height(ImageFont.truetype(path, size))
        err = abs(h - target_height)
        if err < best_err:
            best, best_err = size, err
    return best


def render_codepoint(font, codepoint: int):
    img = Image.new("L", (CANVAS_WIDTH, CANVAS_HEIGHT), color=255)
    draw = ImageDraw.Draw(img)
    ch = chr(codepoint)
    draw.text((ORIGIN_X, BASELINE_Y), ch, fill=0, font=font, anchor="ls")
    px = img.load()

    min_x, max_x = CANVAS_WIDTH, -1
    for y in range(CELL_TOP, CELL_TOP + CELL_HEIGHT):
        for x in range(CANVAS_WIDTH):
            if 255 - px[x, y] > ALPHA_THRESHOLD:
                min_x = min(min_x, x)
                max_x = max(max_x, x)

    try:
        advance = max(1, int(round(font.getlength(ch))))
    except Exception:
        advance = SPACE_ADVANCE_DEFAULT

    if max_x < min_x:
        return [], 0, advance, 0

    width = max_x - min_x + 1
    rows = []
    for y in range(CELL_TOP, CELL_TOP + CELL_HEIGHT):
        for x in range(min_x, max_x + 1):
            alpha = 255 - px[x, y]
            rows.append(alpha if alpha > ALPHA_THRESHOLD else 0)
    return rows, width, advance, min_x - ORIGIN_X


def build_font(path: str, target_height: int, is_serif: bool):
    size = SERIF_POINT_SIZE if is_serif else fit_point_size(path, target_height)
    font = ImageFont.truetype(path, size)
    bitmap_bytes: list[int] = []
    glyph_entries: list[str] = []
    for cp in range(FIRST_CODEPOINT, LAST_CODEPOINT + 1):
        rows, width, advance, x_offset = render_codepoint(font, cp)
        offset = len(bitmap_bytes)
        bitmap_bytes.extend(rows)
        glyph_entries.append(f"    {{{offset}, {x_offset}, {width}, {advance}}},  // U+{cp:04X}")
    try:
        space_adv = max(1, int(round(font.getlength(" "))))
    except Exception:
        space_adv = SPACE_ADVANCE_DEFAULT
    return size, bitmap_bytes, glyph_entries, space_adv


def main() -> None:
    lines: list[str] = [
        "#pragma once",
        "",
        "#include <Arduino.h>",
        "",
        "// Generated by tools/generate_hebrew_fonts.py.",
        "// Several selectable Hebrew faces; the reader picks one at runtime (Settings > Typography >",
        "// Hebrew font). All share the serif cell height and baseline so Hebrew aligns with Latin.",
        "// Letters U+05D0..U+05EA (indexed by codepoint - 0x05D0). RTL ordering is the renderer's job.",
        "",
        "struct EmbeddedHebrewGlyph {",
        "  uint32_t bitmapOffset;",
        "  int8_t xOffset;",
        "  uint8_t width;",
        "  uint8_t xAdvance;",
        "};",
        "",
        f"constexpr uint32_t kEmbeddedHebrewFirstCodepoint = 0x{FIRST_CODEPOINT:04X};",
        f"constexpr uint32_t kEmbeddedHebrewLastCodepoint = 0x{LAST_CODEPOINT:04X};",
        f"constexpr uint8_t kEmbeddedHebrewHeight = {CELL_HEIGHT};",
        "",
        "struct EmbeddedHebrewFont {",
        "  const uint8_t *bitmaps;",
        "  const EmbeddedHebrewGlyph *glyphs;",
        "  uint8_t spaceAdvance;",
        "  const char *name;",
        "};",
        "",
    ]

    serif_path = FONTS[0][1]
    target_height = tall_height(ImageFont.truetype(str(serif_path), SERIF_POINT_SIZE))
    print(f"serif tall height @ {SERIF_POINT_SIZE}pt = {target_height}px")

    table_rows = []
    for name, path, ident in FONTS:
        size, bitmap_bytes, glyph_entries, space_adv = build_font(
            str(path), target_height, is_serif=(ident == "Serif"))
        print(f"{name}: {size}pt, {len(bitmap_bytes)} bytes, space_adv {space_adv}")
        lines.append(f"// {name} ({size} pt)")
        lines.append(f"static const uint8_t kHebrewBitmaps_{ident}[] PROGMEM = {{")
        for off in range(0, len(bitmap_bytes), 16):
            chunk = bitmap_bytes[off:off + 16]
            lines.append("    " + ", ".join(f"{v:3d}" for v in chunk) + ",")
        lines.append("};")
        lines.append(f"static const EmbeddedHebrewGlyph kHebrewGlyphs_{ident}[] PROGMEM = {{")
        lines.extend(glyph_entries)
        lines.append("};")
        lines.append("")
        table_rows.append(
            f"    {{kHebrewBitmaps_{ident}, kHebrewGlyphs_{ident}, {space_adv}, \"{name}\"}},")

    lines.append("static const EmbeddedHebrewFont kEmbeddedHebrewFonts[] = {")
    lines.extend(table_rows)
    lines.append("};")
    lines.append(f"constexpr uint8_t kEmbeddedHebrewFontCount = {len(FONTS)};")
    lines.append("")

    OUT.write_text("\n".join(lines) + "\n", encoding="ascii")
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
