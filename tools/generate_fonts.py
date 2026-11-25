#!/usr/bin/env python3
"""Generate TFT_eSPI .vlw PROGMEM C headers with Turkish UTF-8 glyph support."""

from pathlib import Path
import struct
from PIL import Image, ImageDraw, ImageFont

OUT_DIR = Path(__file__).resolve().parent.parent / "include" / "fonts"
FONT_REG = Path(r"C:\Windows\Fonts\segoeui.ttf")
FONT_BOLD = Path(r"C:\Windows\Fonts\segoeuib.ttf")

ASCII_VISIBLE = "".join(chr(c) for c in range(0x21, 0x7F))
TURKISH_EXTRA = "çÇğĞıİöÖşŞüÜ°–—‘’“”₺€…•·"
FULL_TR_CHARSET = ASCII_VISIBLE + TURKISH_EXTRA

FONTS = [
    ("SegoeUI14TR", FONT_REG, 14, FULL_TR_CHARSET),
    ("SegoeUIBold18TR", FONT_BOLD, 18, FULL_TR_CHARSET),
    ("SegoeUIBold42Temp", FONT_BOLD, 42, "0123456789.-°C"),
    ("SegoeUIBold88Digits", FONT_BOLD, 88, "0123456789:"),
]


def unique_chars(chars: str) -> str:
    seen = set()
    out = []
    for ch in chars:
        cp = ord(ch)
        if cp not in seen and cp <= 0xFFFF:
            seen.add(cp)
            out.append(ch)
    out.sort(key=ord)
    return "".join(out)


def build_vlw(font_path: Path, size: int, charset: str, symbol_name: str) -> bytes:
    font = ImageFont.truetype(str(font_path), size=size)
    font_ascent, font_descent = font.getmetrics()
    chars = unique_chars(charset)

    glyphs = []
    max_gdy = font_ascent
    max_desc = font_descent

    for ch in chars:
        bbox = font.getbbox(ch, anchor="ls")
        if bbox is None:
            continue
        left, top, right, bottom = bbox
        gw = right - left
        gh = bottom - top
        if gw <= 0 or gh <= 0:
            continue
        if gw > 255 or gh > 255:
            raise ValueError(f"Glyph {ch!r} in {symbol_name} exceeds 255px: {gw}x{gh}")

        gdx = max(-128, min(127, int(left)))
        gdy = int(-top)
        gx_adv = max(1, min(255, int(round(font.getlength(ch)))))

        img = Image.new("L", (gw, gh), 0)
        draw = ImageDraw.Draw(img)
        draw.text((-left, -top), ch, font=font, fill=255, anchor="ls")
        alpha = img.tobytes()

        max_gdy = max(max_gdy, gdy)
        max_desc = max(max_desc, gh - gdy)
        glyphs.append((ord(ch), gh, gw, gx_adv, gdy, gdx, alpha))

    # Header: gCount, version(11), fontSize, 0, ascent, descent
    buf = bytearray()
    buf += struct.pack(">6I", len(glyphs), 11, size, 0, max_gdy, max_desc)

    for cp, gh, gw, gx_adv, gdy, gdx, _ in glyphs:
        buf += struct.pack(">7i", cp, gh, gw, gx_adv, gdy, gdx, 0)

    for _, _, _, _, _, _, alpha in glyphs:
        buf += alpha

    name_bytes = symbol_name.encode("ascii")
    buf.append(len(name_bytes))
    buf += name_bytes + b"\x00"
    buf.append(len(name_bytes))
    buf += name_bytes + b"\x00"
    buf.append(1)  # anti-aliased
    return bytes(buf)


def write_header(symbol_name: str, data: bytes) -> Path:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    header_path = OUT_DIR / f"{symbol_name}.h"
    lines = [
        "#pragma once",
        "#include <pgmspace.h>",
        "#include <stdint.h>",
        "",
        f"const uint8_t {symbol_name}[] PROGMEM = {{",
    ]
    for i in range(0, len(data), 16):
        chunk = ", ".join(f"0x{b:02X}" for b in data[i : i + 16])
        lines.append(f"  {chunk},")
    lines.append("};")
    lines.append("")
    header_path.write_text("\n".join(lines), encoding="utf-8")
    return header_path


def main() -> None:
    total = 0
    for sym, fpath, size, chars in FONTS:
        if not fpath.exists():
            raise FileNotFoundError(f"Missing font: {fpath}")
        data = build_vlw(fpath, size, chars, sym)
        out_path = write_header(sym, data)
        total += len(data)
        print(f"{sym}: {len(data)} bytes -> {out_path.name}")
    print(f"Total PROGMEM font size: {total} bytes ({total / 1024:.1f} KB)")


if __name__ == "__main__":
    main()
