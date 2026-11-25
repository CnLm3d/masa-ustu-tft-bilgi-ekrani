#!/usr/bin/env python3
"""Render pixel-accurate 320x240 Touch TFT firmware UI previews using Pillow."""

from pathlib import Path
import math
import shutil
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
DOCS_PREVIEWS = ROOT / "docs" / "previews"
ARTIFACT_PREVIEWS = Path(
    r"C:\Users\CASPER\.gemini\antigravity\brain\a86a342a-50c2-4054-8732-48970d2ddd4d\previews"
)

FONT_REG_PATH = r"C:\Windows\Fonts\segoeui.ttf"
FONT_BOLD_PATH = r"C:\Windows\Fonts\segoeuib.ttf"

W, H = 320, 240


def rgb565(val: int) -> tuple[int, int, int]:
    r = ((val >> 11) & 0x1F) * 255 // 31
    g = ((val >> 5) & 0x3F) * 255 // 63
    b = (val & 0x1F) * 255 // 31
    return (r, g, b)


COL_BG = rgb565(0x0842)
COL_SURFACE = rgb565(0x10A3)
COL_CARD_TOP = rgb565(0x18E4)
COL_CARD_BOT = rgb565(0x18C4)
COL_BORDER = rgb565(0x2967)
COL_HINGE = rgb565(0x0000)
COL_ACCENT = rgb565(0x3C9F)
COL_ACCENT_2 = rgb565(0xFD20)
COL_TEXT = rgb565(0xF7BE)
COL_MUTED = rgb565(0x8C71)
COL_OK = rgb565(0x2E8C)

F14 = ImageFont.truetype(FONT_REG_PATH, 14)
F18B = ImageFont.truetype(FONT_BOLD_PATH, 18)
F42B = ImageFont.truetype(FONT_BOLD_PATH, 42)
F88B = ImageFont.truetype(FONT_BOLD_PATH, 88)


def draw_top_bar(draw: ImageDraw.ImageDraw, title: str, fetching: bool = False) -> None:
    draw.rectangle([0, 0, W - 1, 26], fill=COL_SURFACE)
    draw.line([0, 26, W - 1, 26], fill=COL_BORDER)
    draw.text((10, 13), title, font=F14, fill=COL_TEXT, anchor="lm")

    # Tappable [Ayar] pill
    draw.rounded_rectangle([214, 4, 263, 21], radius=6, fill=COL_BG, outline=COL_BORDER)
    draw.text((239, 13), "Ayar", font=F14, fill=COL_TEXT, anchor="mm")

    draw.ellipse([271, 10, 277, 16], fill=COL_ACCENT_2 if fetching else COL_BORDER)

    for b in range(4):
        bx = 286 + b * 6
        bh = 4 + b * 3
        by = 20 - bh
        draw.rectangle([bx, by, bx + 3, 19], fill=COL_OK)


def draw_bottom_tabs(draw: ImageDraw.ImageDraw, active_idx: int) -> None:
    labels = ("Saat", "Hava", "Haber")
    xs = (4, 109, 214)
    tab_y, tab_w, tab_h = 208, 102, 30
    for i, label in enumerate(labels):
        active = i == active_idx
        fill_col = COL_ACCENT if active else COL_SURFACE
        border_col = COL_ACCENT if active else COL_BORDER
        text_col = COL_TEXT if active else COL_MUTED
        x = xs[i]
        draw.rounded_rectangle([x, tab_y, x + tab_w - 1, tab_y + tab_h - 1],
                               radius=7, fill=fill_col, outline=border_col)
        draw.text((x + tab_w // 2, tab_y + tab_h // 2), label, font=F14, fill=text_col, anchor="mm")


def draw_cloud(draw: ImageDraw.ImageDraw, cx: int, cy: int, r: int, col: tuple[int, int, int]) -> None:
    r1 = r * 42 // 100
    r2 = r * 54 // 100
    r3 = r * 38 // 100
    c1 = (cx - r * 35 // 100, cy + r * 12 // 100)
    c2 = (cx, cy - r * 8 // 100)
    c3 = (cx + r * 38 // 100, cy + r * 15 // 100)
    draw.ellipse([c1[0] - r1, c1[1] - r1, c1[0] + r1, c1[1] + r1], fill=col)
    draw.ellipse([c2[0] - r2, c2[1] - r2, c2[0] + r2, c2[1] + r2], fill=col)
    draw.ellipse([c3[0] - r3, c3[1] - r3, c3[0] + r3, c3[1] + r3], fill=col)
    draw.rounded_rectangle(
        [cx - r * 62 // 100, cy + r * 10 // 100, cx + r * 62 // 100, cy + r * 52 // 100],
        radius=max(1, r * 18 // 100),
        fill=col,
    )


def draw_weather_icon(draw: ImageDraw.ImageDraw, cx: int, cy: int, r: int, wmo: int) -> None:
    sun_col = rgb565(0xFEA0)
    cloud_col = rgb565(0xDEFB)
    rain_col = COL_ACCENT

    if wmo == 0:
        sr = r * 48 // 100
        draw.ellipse([cx - sr, cy - sr, cx + sr, cy + sr], fill=sun_col)
        for a in range(8):
            rad = a * math.pi / 4.0
            x1 = cx + int(math.cos(rad) * r * 64 / 100)
            y1 = cy + int(math.sin(rad) * r * 64 / 100)
            x2 = cx + int(math.cos(rad) * r * 88 / 100)
            y2 = cy + int(math.sin(rad) * r * 88 / 100)
            draw.line([x1, y1, x2, y2], fill=sun_col, width=1)
    elif wmo in (1, 2):
        sr = r * 30 // 100
        scx, scy = cx - r * 28 // 100, cy - r * 26 // 100
        draw.ellipse([scx - sr, scy - sr, scx + sr, scy + sr], fill=sun_col)
        draw_cloud(draw, cx + r * 8 // 100, cy + r * 8 // 100, r * 78 // 100, cloud_col)
    elif wmo == 3:
        draw_cloud(draw, cx, cy, r * 90 // 100, cloud_col)
    elif 51 <= wmo <= 67 or 80 <= wmo <= 82:
        draw_cloud(draw, cx, cy - r * 18 // 100, r * 82 // 100, cloud_col)
        for i in (-1, 0, 1):
            rx = cx + i * (r * 28 // 100)
            draw.line([rx, cy + r * 26 // 100, rx - 4, cy + r * 58 // 100], fill=rain_col, width=2)


def make_flip_card(digits: str) -> Image.Image:
    cw, ch = 134, 134
    half = ch // 2
    img = Image.new("RGB", (cw, ch), COL_BG)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, cw - 1, ch - 1], radius=10, fill=COL_CARD_BOT)
    d.rounded_rectangle([0, 0, cw - 1, half + 5], radius=10, fill=COL_CARD_TOP)
    d.rectangle([0, half, cw - 1, half + 7], fill=COL_CARD_BOT)
    d.rounded_rectangle([0, 0, cw - 1, ch - 1], radius=10, outline=COL_BORDER, width=1)
    d.text((cw // 2, ch // 2 + 2), digits, font=F88B, fill=COL_TEXT, anchor="mm")
    d.rectangle([1, half - 1, cw - 2, half + 1], fill=COL_HINGE)
    d.rounded_rectangle([3, half - 5, 7, half + 4], radius=2, fill=COL_MUTED)
    d.rounded_rectangle([cw - 8, half - 5, cw - 4, half + 4], radius=2, fill=COL_MUTED)
    return img


def render_clock(mid_flip: bool = False) -> Image.Image:
    img = Image.new("RGB", (W, H), COL_BG)
    d = ImageDraw.Draw(img)
    draw_top_bar(d, "Yaprak Saat", fetching=False)

    hour_card = make_flip_card("16")
    img.paste(hour_card, (16, 33))

    if not mid_flip:
        min_card = make_flip_card("30")
        img.paste(min_card, (170, 33))
        sec = 42
    else:
        old_card = make_flip_card("29")
        new_card = make_flip_card("30")
        img.paste(old_card, (170, 33))

        cw, ch = 134, 134
        half = ch // 2
        cur_h = 20
        uncovered = half - cur_h
        img.paste(new_card.crop((0, 0, cw, uncovered)), (170, 33))
        leaf = old_card.crop((0, 0, cw, half)).resize((cw, cur_h), Image.BILINEAR)
        shaded = leaf.point(lambda p: int(p * 0.62))
        img.paste(shaded, (170, 33 + uncovered))
        d.line([172, 33 + uncovered, 170 + cw - 3, 33 + uncovered], fill=COL_HINGE)
        d.line([172, 33 + half, 170 + cw - 3, 33 + half], fill=COL_HINGE)
        sec = 0

    d.ellipse([156, 82, 164, 90], fill=COL_ACCENT)
    d.ellipse([156, 110, 164, 118], fill=COL_ACCENT)

    d.rounded_rectangle([16, 172, W - 17, 195], radius=7, fill=COL_SURFACE, outline=COL_BORDER)
    d.text((26, 184), "25 Kas Çarşamba", font=F14, fill=COL_TEXT, anchor="lm")
    d.text((W - 26, 184), "İstanbul 18°C", font=F14, fill=COL_ACCENT_2, anchor="rm")

    bar_x, bar_y, bar_w, bar_h = 16, 200, W - 32, 4
    d.rounded_rectangle([bar_x, bar_y, bar_x + bar_w - 1, bar_y + bar_h - 1], radius=2, fill=COL_SURFACE)
    fill_w = max(4, (bar_w * (sec + 1)) // 60)
    d.rounded_rectangle([bar_x, bar_y, bar_x + fill_w - 1, bar_y + bar_h - 1], radius=2, fill=COL_ACCENT)

    draw_bottom_tabs(d, 0)
    return img


def render_weather() -> Image.Image:
    img = Image.new("RGB", (W, H), COL_BG)
    d = ImageDraw.Draw(img)
    draw_top_bar(d, "İstanbul", fetching=False)

    d.rounded_rectangle([12, 32, W - 13, 119], radius=10, fill=COL_SURFACE, outline=COL_BORDER)
    draw_weather_icon(d, 46, 63, 20, wmo=2)
    d.text((78, 35), "18°C", font=F42B, fill=COL_TEXT, anchor="lt")

    d.text((W - 22, 38), "Hissedilen: 17°", font=F14, fill=COL_MUTED, anchor="rt")
    d.text((W - 22, 55), "Nem: %64", font=F14, fill=COL_MUTED, anchor="rt")
    d.text((W - 22, 72), "Rüzgar: 14 km/s", font=F14, fill=COL_MUTED, anchor="rt")

    d.line([22, 92, W - 22, 92], fill=COL_BORDER)
    d.text((22, 96), "Parçalı Bulutlu", font=F18B, fill=COL_ACCENT, anchor="lt")

    d.rounded_rectangle([240, 95, 299, 114], radius=6, fill=COL_BG, outline=COL_ACCENT)
    d.text((270, 105), "Yenile", font=F14, fill=COL_TEXT, anchor="mm")

    days = [
        ("Yarın", 61, "15° / 9°"),
        ("Cum", 0, "19° / 10°"),
        ("Cmt", 2, "17° / 11°"),
    ]
    card_w, card_h, gap, start_x, card_y = 94, 76, 7, 12, 126
    for idx, (label, wmo, rng) in enumerate(days):
        cx = start_x + idx * (card_w + gap)
        d.rounded_rectangle([cx, card_y, cx + card_w - 1, card_y + card_h - 1],
                            radius=8, fill=COL_SURFACE, outline=COL_BORDER)
        d.text((cx + card_w // 2, card_y + 6), label, font=F14, fill=COL_TEXT, anchor="mt")
        draw_weather_icon(d, cx + card_w // 2, card_y + 38, 13, wmo=wmo)
        d.text((cx + card_w // 2, card_y + card_h - 5), rng, font=F14, fill=COL_ACCENT_2, anchor="mb")

    draw_bottom_tabs(d, 1)
    return img


def wrap_lines(draw: ImageDraw.ImageDraw, text: str, font: ImageFont.FreeTypeFont, max_w: int, max_lines: int) -> list[str]:
    words = text.split()
    lines: list[str] = []
    cur = ""
    for w in words:
        cand = w if not cur else f"{cur} {w}"
        if draw.textlength(cand, font=font) <= max_w:
            cur = cand
        else:
            if cur:
                lines.append(cur)
            cur = w
            if len(lines) == max_lines:
                break
    if cur and len(lines) < max_lines:
        lines.append(cur)
    if len(lines) == max_lines and len(" ".join(lines).split()) < len(words):
        last = lines[-1]
        while last and draw.textlength(last + "...", font=font) > max_w:
            last = last[:-1].rstrip()
        lines[-1] = last + "..."
    return lines


def render_news() -> Image.Image:
    img = Image.new("RGB", (W, H), COL_BG)
    d = ImageDraw.Draw(img)
    draw_top_bar(d, "Haber • Gündem", fetching=False)

    cx, cy, cw, ch = 12, 32, W - 24, 136
    d.rounded_rectangle([cx, cy, cx + cw - 1, cy + ch - 1], radius=10, fill=COL_SURFACE, outline=COL_BORDER)

    d.text((cx + 12, cy + 8), "Gündem", font=F14, fill=COL_ACCENT, anchor="lt")
    d.text((cx + cw - 12, cy + 8), "1 / 12", font=F14, fill=COL_MUTED, anchor="rt")

    headline = "İstanbul ve Marmara genelinde yarından itibaren sağanak yağış bekleniyor"
    lines = wrap_lines(d, headline, F18B, cw - 24, 3)
    for i, ln in enumerate(lines):
        d.text((cx + 12, cy + 30 + i * 23), ln, font=F18B, fill=COL_TEXT, anchor="lt")

    d.line([cx + 12, cy + ch - 27, cx + cw - 12, cy + ch - 27], fill=COL_BORDER)
    d.text((cx + 12, cy + ch - 21), "TRT Haber", font=F14, fill=COL_ACCENT_2, anchor="lt")
    d.text((cx + cw - 12, cy + ch - 21), "16:15", font=F14, fill=COL_MUTED, anchor="rt")

    btns = [
        (12, 173, 94, 30, "< Önceki", COL_BORDER),
        (112, 173, 96, 30, "Konu Değiş", COL_ACCENT),
        (214, 173, 94, 30, "Sonraki >", COL_BORDER),
    ]
    for bx, by, bw, bh, lbl, bcol in btns:
        d.rounded_rectangle([bx, by, bx + bw - 1, by + bh - 1], radius=7, fill=COL_SURFACE, outline=bcol)
        d.text((bx + bw // 2, by + bh // 2), lbl, font=F14, fill=COL_TEXT, anchor="mm")

    draw_bottom_tabs(d, 2)
    return img


def make_showcase(frames: list[tuple[str, Image.Image]]) -> Image.Image:
    pad = 24
    title_h = 34
    cell_w, cell_h = 640, 480
    total_w = pad * 3 + cell_w * 2
    total_h = pad * 3 + (cell_h + title_h) * 2 + 46
    sheet = Image.new("RGB", (total_w, total_h), (14, 17, 23))
    d = ImageDraw.Draw(sheet)

    title_font = ImageFont.truetype(FONT_BOLD_PATH, 24)
    cap_font = ImageFont.truetype(FONT_BOLD_PATH, 18)
    d.text((total_w // 2, 24), "ESP32 DeskHub • Dokunmatik 2.4\" ILI9341 (320×240)",
           font=title_font, fill=COL_TEXT, anchor="mm")

    for idx, (caption, im) in enumerate(frames):
        col = idx % 2
        row = idx // 2
        x = pad + col * (cell_w + pad)
        y = 48 + pad + row * (cell_h + title_h + pad)
        d.text((x + cell_w // 2, y + 14), caption, font=cap_font, fill=COL_ACCENT, anchor="mm")
        bezel = [x - 4, y + title_h - 4, x + cell_w + 3, y + title_h + cell_h + 3]
        d.rounded_rectangle(bezel, radius=10, outline=(60, 68, 82), width=3)
        up = im.resize((cell_w, cell_h), Image.NEAREST)
        sheet.paste(up, (x, y + title_h))
    return sheet


def main() -> None:
    DOCS_PREVIEWS.mkdir(parents=True, exist_ok=True)
    ARTIFACT_PREVIEWS.mkdir(parents=True, exist_ok=True)

    screens = [
        ("screen_1_clock.png", "1. Yaprak Saat (Dokunmatik Sekmeli)", render_clock(mid_flip=False)),
        ("screen_1b_clock_flip.png", "1b. Dakika Yaprak Dönüşü (29 → 30)", render_clock(mid_flip=True)),
        ("screen_2_weather.png", "2. Hava Durumu (Yenile Butonlu)", render_weather()),
        ("screen_3_news.png", "3. Google Haberler (Önceki / Konu / Sonraki)", render_news()),
    ]

    for fname, _, im in screens:
        up = im.resize((W * 2, H * 2), Image.NEAREST)
        dst_docs = DOCS_PREVIEWS / fname
        up.save(dst_docs)
        shutil.copy2(dst_docs, ARTIFACT_PREVIEWS / fname)

    showcase = make_showcase([(cap, im) for _, cap, im in screens])
    showcase_docs = DOCS_PREVIEWS / "all_screens_preview.png"
    showcase.save(showcase_docs)
    shutil.copy2(showcase_docs, ARTIFACT_PREVIEWS / "all_screens_preview.png")
    print(f"Saved 5 touch previews to {DOCS_PREVIEWS} and {ARTIFACT_PREVIEWS}")


if __name__ == "__main__":
    main()
