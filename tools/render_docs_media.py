#!/usr/bin/env python3
"""Generate animated GIFs and copy showcase PNGs into docs/media/ for GitHub README."""

from pathlib import Path
import importlib.util
import shutil
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
MEDIA_DIR = ROOT / "docs" / "media"
V2_PREVIEWS = Path(
    r"C:\Users\CASPER\.gemini\antigravity\brain\a86a342a-50c2-4054-8732-48970d2ddd4d\v2_previews"
)
V2_SCRIPT = Path(
    r"C:\Users\CASPER\.gemini\antigravity\brain\a86a342a-50c2-4054-8732-48970d2ddd4d\scratch\render_v2_previews.py"
)

spec = importlib.util.spec_from_file_location("v2p", V2_SCRIPT)
v2p = importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(v2p)

W, H = 320, 240


def make_flip_card(digits: str) -> Image.Image:
    cw, ch = 134, 134
    half = ch // 2
    img = Image.new("RGB", (cw, ch), v2p.BG)
    d = ImageDraw.Draw(img)
    font88 = ImageFont.truetype(v2p.FONT_BOLD, 84)
    d.rounded_rectangle([0, 0, cw - 1, ch - 1], radius=10, fill=(24, 27, 36))
    d.rounded_rectangle([0, 0, cw - 1, half + 5], radius=10, fill=(28, 32, 41))
    d.rectangle([0, half, cw - 1, half + 7], fill=(24, 27, 36))
    d.rounded_rectangle([0, 0, cw - 1, ch - 1], radius=10, outline=v2p.BORDER, width=1)
    d.text((cw // 2, ch // 2 + 2), digits, font=font88, fill=v2p.TEXT, anchor="mm")
    d.rectangle([1, half - 1, cw - 2, half + 1], fill=(0, 0, 0))
    d.rounded_rectangle([3, half - 5, 7, half + 4], radius=2, fill=v2p.MUTED)
    d.rounded_rectangle([cw - 8, half - 5, cw - 4, half + 4], radius=2, fill=v2p.MUTED)
    return img


def build_flip_gif() -> Path:
    hour_c = make_flip_card("16")
    old_c = make_flip_card("29")
    new_c = make_flip_card("30")
    cw, ch = 134, 134
    half = ch // 2

    def base_frame(sec: int) -> Image.Image:
        im = Image.new("RGB", (W, H), v2p.BG)
        d = ImageDraw.Draw(im)
        v2p.draw_topbar(d, "Yaprak Saat", "Tema")
        im.paste(hour_c, (16, 33))
        d.ellipse([156, 82, 164, 90], fill=v2p.ACCENT)
        d.ellipse([156, 110, 164, 118], fill=v2p.ACCENT)
        d.rounded_rectangle([16, 172, W - 17, 195], radius=7, fill=v2p.SURF, outline=v2p.BORDER)
        d.text((26, 184), "25 Kas Çarşamba", font=v2p.F14, fill=v2p.TEXT, anchor="lm")
        d.text((W - 26, 184), "İstanbul 18°C", font=v2p.F14, fill=v2p.AMBER, anchor="rm")
        d.rounded_rectangle([16, 200, W - 16, 204], radius=2, fill=v2p.SURF)
        fw = max(4, ((W - 32) * (sec + 1)) // 60)
        d.rounded_rectangle([16, 200, 16 + fw, 204], radius=2, fill=v2p.ACCENT)
        v2p.draw_bottom_nav(d, 0)
        return im

    frames: list[Image.Image] = []
    durations: list[int] = []

    # Hold 29
    f0 = base_frame(58)
    f0.paste(old_c, (170, 33))
    frames.append(f0.resize((480, 360), Image.NEAREST))
    durations.append(750)

    # Phase 1: upper leaf folds down
    steps = 7
    for s in range(1, steps + 1):
        im = base_frame(59)
        im.paste(old_c, (170, 33))
        cur_h = max(2, (half * (steps - s)) // steps)
        uncovered = half - cur_h
        im.paste(new_c.crop((0, 0, cw, uncovered)), (170, 33))
        leaf = old_c.crop((0, 0, cw, half)).resize((cw, cur_h), Image.BILINEAR)
        scale = 0.45 + 0.55 * (cur_h / half)
        im.paste(leaf.point(lambda p: int(p * scale)), (170, 33 + uncovered))
        frames.append(im.resize((480, 360), Image.NEAREST))
        durations.append(45)

    # Phase 2: lower leaf unfolds
    for s in range(1, steps + 1):
        im = base_frame(0)
        im.paste(old_c, (170, 33))
        im.paste(new_c.crop((0, 0, cw, half)), (170, 33))
        cur_h = max(2, (half * s) // steps)
        leaf = new_c.crop((0, half, cw, ch)).resize((cw, cur_h), Image.BILINEAR)
        scale = 0.45 + 0.55 * (cur_h / half)
        im.paste(leaf.point(lambda p: int(p * scale)), (170, 33 + half))
        frames.append(im.resize((480, 360), Image.NEAREST))
        durations.append(45)

    # Hold 30
    f_end = base_frame(1)
    f_end.paste(new_c, (170, 33))
    frames.append(f_end.resize((480, 360), Image.NEAREST))
    durations.append(1200)

    out = MEDIA_DIR / "anim_flip_clock.gif"
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=durations, loop=0, optimize=True)
    return out


def build_mario_gif() -> Path:
    sky = (92, 148, 252)
    frames: list[Image.Image] = []
    durations: list[int] = []

    def draw_scene(mario_y: int, jumping: bool, block_dy: int, dig_m2: str, coin_y: int | None) -> Image.Image:
        im = Image.new("RGB", (W, H), sky)
        d = ImageDraw.Draw(im)
        v2p.draw_topbar(d, "Mario Saat")
        for cx, cy in [(46, 124), (274, 46)]:
            d.rounded_rectangle([cx - 22, cy - 5, cx + 22, cy + 7], radius=6, fill=(255, 255, 255))
            d.ellipse([cx - 12, cy - 13, cx + 12, cy + 4], fill=(255, 255, 255))
        d.pieslice([-10, 134, 86, 220], 180, 360, fill=(28, 168, 48), outline=(14, 98, 22), width=2)
        px, py = 258, 132
        d.rectangle([px + 4, py + 14, px + 44, 176], fill=(32, 176, 44), outline=(14, 72, 18), width=2)
        d.rectangle([px + 10, py + 15, px + 16, 175], fill=(120, 232, 108))
        d.rectangle([px, py, px + 48, py + 15], fill=(44, 204, 56), outline=(14, 72, 18), width=2)
        for row_y in (176, 192):
            for gx in range(0, W, 16):
                d.rectangle([gx, row_y, gx + 15, row_y + 15], fill=(200, 76, 12), outline=(28, 16, 10))
                d.line([gx + 1, row_y + 1, gx + 14, row_y + 1], fill=(244, 160, 112))
                d.line([gx + 8, row_y + 2, gx + 8, row_y + 8], fill=(28, 16, 10))
                d.line([gx + 1, row_y + 8, gx + 14, row_y + 8], fill=(28, 16, 10))

        d.text((12, 32), "25 KAS ÇAR", font=v2p.F12B, fill=(255, 255, 255), anchor="lt")
        d.ellipse([98, 31, 110, 45], fill=v2p.AMBER, outline=(140, 72, 8))
        d.text((115, 32), "18°C", font=v2p.F12B, fill=(255, 242, 150), anchor="lt")

        # Update pixel_digits table temporarily for '9' if needed
        v2p.PIXEL_DIGITS_5X7["9"] = ["01110", "10001", "10001", "01111", "00001", "00001", "01110"]
        v2p.PIXEL_DIGITS_5X7["2"] = ["01110", "10001", "00001", "00110", "01000", "10000", "11111"]

        for bx, dg, dy in [(24, "1", 0), (72, "6", 0), (140, "3", 0), (188, dig_m2, block_dy)]:
            v2p.draw_smb_block(d, bx, 68 + dy, 42, dg)
        for cy in (80, 96):
            d.rectangle([124, cy, 129, cy + 5], fill=(255, 255, 255), outline=(24, 16, 10))

        if coin_y is not None:
            cx = 209
            d.ellipse([cx - 5, coin_y - 8, cx + 5, coin_y + 8], fill=(252, 212, 48), outline=(120, 68, 8), width=2)
            d.line([cx, coin_y - 4, cx, coin_y + 4], fill=(255, 248, 200), width=2)

        sprite = v2p.MARIO_JUMP_16 if jumping else v2p.MARIO_STAND_16
        mx = 186 if jumping else 193
        v2p.draw_pixel_sprite(d, sprite, mx, mario_y, 2, v2p.MARIO_PAL)
        v2p.draw_bottom_nav(d, 0)
        return im.resize((480, 360), Image.NEAREST)

    frames.append(draw_scene(144, False, 0, "9", None))
    durations.append(750)

    total = 12
    for f in range(1, total + 1):
        t = f / total
        arc = 4.0 * t * (1.0 - t)
        my = 144 - int(arc * 42)
        hit = f >= 6
        bdy = -10 if f in (6, 7) else (-5 if f in (5, 8) else 0)
        cy = (56 - (f - 5) * 4) if (6 <= f <= 11) else None
        frames.append(draw_scene(my, arc > 0.2, bdy, "0" if hit else "9", cy))
        durations.append(55)

    frames.append(draw_scene(144, False, 0, "0", None))
    durations.append(1200)

    out = MEDIA_DIR / "anim_mario_clock.gif"
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=durations, loop=0, optimize=True)
    return out


def build_tetris_gif() -> Path:
    v2p.TETRIS_PIECES_3X5["9"] = ["TTT", "S.L", "OOO", "..J", "JJJ"]
    frames: list[Image.Image] = []
    durations: list[int] = []
    bs = 12
    digit_xs = [20, 66, 124, 170]
    digit_y = 92

    def base_tetris(settled_until_row: int, drop_row: int | None, drop_y: int, digit_m2: str) -> Image.Image:
        im = Image.new("RGB", (W, H), (10, 12, 18))
        d = ImageDraw.Draw(im)
        v2p.draw_topbar(d, "Tetris Saat")
        fx, fy, fw, fh = 10, 28, 212, 177
        d.rectangle([fx, fy, fx + fw, fy + fh], fill=(13, 17, 25), outline=(55, 70, 96), width=2)
        for gx in range(fx + bs, fx + fw, bs):
            d.line([gx, fy + 2, gx, fy + fh - 2], fill=(22, 28, 40))
        for gy in range(fy + bs, fy + fh, bs):
            d.line([fx + 2, gy, fx + fw - 2, gy], fill=(22, 28, 40))

        # 1, 6, 3 completely static
        for idx, ch in enumerate("163"):
            for r_i, row in enumerate(v2p.TETRIS_PIECES_3X5[ch]):
                for c_i, key in enumerate(row):
                    if key != ".":
                        v2p.draw_tetro_cell(d, digit_xs[idx] + c_i * bs, digit_y + r_i * bs, bs, v2p.TETRO_COLORS[key])

        for cy in (106, 130):
            v2p.draw_tetro_cell(d, 108, cy, 10, (250, 220, 55))

        pat = v2p.TETRIS_PIECES_3X5[digit_m2]
        for r_i in range(settled_until_row, 5):
            for c_i, key in enumerate(pat[r_i]):
                if key != ".":
                    v2p.draw_tetro_cell(d, digit_xs[3] + c_i * bs, digit_y + r_i * bs, bs, v2p.TETRO_COLORS[key])

        if drop_row is not None:
            for c_i, key in enumerate(pat[drop_row]):
                if key != ".":
                    v2p.draw_tetro_cell(d, digit_xs[3] + c_i * bs, drop_y, bs, v2p.TETRO_COLORS[key])

        debris_cols = [v2p.TETRO_COLORS["Z"], v2p.TETRO_COLORS["I"], v2p.TETRO_COLORS["O"],
                       v2p.TETRO_COLORS["S"], v2p.TETRO_COLORS["T"], v2p.TETRO_COLORS["L"]]
        for row_i in range(2):
            for i in range(17):
                if row_i == 1 and i in (3, 7, 11, 14):
                    continue
                v2p.draw_tetro_cell(d, fx + 4 + i * bs, fy + fh - 14 - row_i * bs, bs, debris_cols[(i + row_i * 2) % 6])

        sx = 228
        d.rounded_rectangle([sx, 28, W - 10, 205], radius=8, fill=v2p.SURF, outline=v2p.BORDER)
        d.text((sx + 41, 42), "SONRAKİ", font=v2p.F11B, fill=v2p.MUTED, anchor="mm")
        for px, py in [(sx + 26, 60), (sx + 26, 72), (sx + 26, 84), (sx + 38, 84)]:
            v2p.draw_tetro_cell(d, px, py, 11, v2p.AMBER)
        d.line([sx + 10, 110, W - 20, 110], fill=v2p.BORDER)
        d.text((sx + 41, 128), "25 KAS", font=v2p.F12B, fill=v2p.TEXT, anchor="mm")
        d.text((sx + 41, 148), "ÇARŞAMBA", font=v2p.F10, fill=v2p.MUTED, anchor="mm")
        d.text((sx + 41, 178), "18°C", font=v2p.F16B, fill=(40, 220, 255), anchor="mm")
        v2p.draw_bottom_nav(d, 0)
        return im.resize((480, 360), Image.NEAREST)

    frames.append(base_tetris(0, None, 0, "9"))
    durations.append(750)

    # Clear column 3 and drop pieces ONLY into column 3 from bottom row (4) to top row (0)
    for settle_r in range(4, -1, -1):
        target_y = digit_y + settle_r * bs
        steps = max(3, (target_y - 34) // 24)
        for s in range(steps + 1):
            dy = 34 + ((target_y - 34) * s) // steps
            frames.append(base_tetris(settle_r + 1, settle_r, dy, "0"))
            durations.append(50)

    frames.append(base_tetris(0, None, 0, "0"))
    durations.append(1200)

    out = MEDIA_DIR / "anim_tetris_clock.gif"
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=durations, loop=0, optimize=True)
    return out


def main() -> None:
    MEDIA_DIR.mkdir(parents=True, exist_ok=True)
    copies = [
        ("sheet_1_clock_themes.png", "clock_themes_all.png"),
        ("sheet_1b_refined_themes.png", "refined_themes_detail.png"),
        ("sheet_2_new_menus.png", "menus_all.png"),
        ("sheet_3_web_ui.png", "web_ui_dashboard.png"),
    ]
    for src_name, dst_name in copies:
        shutil.copy2(V2_PREVIEWS / src_name, MEDIA_DIR / dst_name)

    for fn in (build_flip_gif, build_mario_gif, build_tetris_gif):
        p = fn()
        print(f"{p.name}: {p.stat().st_size / 1024:.1f} KB")


if __name__ == "__main__":
    main()
