#!/usr/bin/env python3
"""Generate support-free, watertight 3D-printable STLs and 4-view studio render for ESP32 DeskHub."""

from __future__ import annotations

import math
import pathlib
import shutil
import struct
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import manifold3d as m3d

ROOT = pathlib.Path(__file__).resolve().parent.parent
ENC_DIR = ROOT / "enclosure"
STL_DIR = ENC_DIR / "stl"
ARTIFACT_PREVIEW = pathlib.Path(
    r"C:\Users\CASPER\.gemini\antigravity\brain\a86a342a-50c2-4054-8732-48970d2ddd4d\enclosure_preview.png"
)

FONT_BOLD = r"C:\Windows\Fonts\segoeuib.ttf"
FONT_REG = r"C:\Windows\Fonts\segoeui.ttf"

# ---------------------------------------------------------------------------
# PARAMETRIC DIMENSIONS (mm) — 77x42mm PCB (68x37mm holes) + 15 deg wedge
# ---------------------------------------------------------------------------
OUTER_W = 112.0
OUTER_H = 66.0
CORNER_R = 6.0
WALL = 2.6
FRONT_WALL = 2.4
CHAMFER = 1.6
SHELL_DEPTH = 44.0
TILT_DEG = 15.0
SHEAR_Y = math.tan(math.radians(TILT_DEG))  # ~0.26795

# TFT active opening & 68x37mm bosses shifted -3.5mm so window stays centered
WIN_W = 50.8
WIN_H = 38.4
WIN_OFF_X = 0.0
TFT_HOLE_DX = 68.0
TFT_HOLE_DY = 37.0
TFT_HOLE_CX = -3.5
TFT_BOSS_H = 2.8

# Back cover & rear M3 bosses (outside 77x42 PCB & deep behind DuPont pins)
COVER_THICK = 2.6
LIP_H = 1.8
FIT_CLEAR = 0.35
REAR_HOLE_DX = 99.0
REAR_HOLE_DY = 53.0


def rounded_rect_cs(w: float, h: float, r: float, segs: int = 36) -> m3d.CrossSection:
    r = min(r, w * 0.49, h * 0.49)
    if r <= 0.05:
        return m3d.CrossSection.square([w, h], center=True)
    core = m3d.CrossSection.square([w - 2 * r, h - 2 * r], center=True)
    return core.offset(r, m3d.JoinType.Round, circular_segments=segs)


def chamfered_extrude_shell(w: float, h: float, r: float, depth: float, cham: float) -> m3d.Manifold:
    """Create a rounded prism from Z=0..depth with a 45-deg front chamfer at Z=0..cham."""
    body = rounded_rect_cs(w, h, r).extrude(depth - cham).translate([0, 0, cham])
    c_bot = rounded_rect_cs(w - 2 * cham, h - 2 * cham, max(0.5, r - cham)).extrude(0.05)
    c_top = rounded_rect_cs(w, h, r).extrude(0.05).translate([0, 0, cham])
    front_cham = (c_bot + c_top).hull()
    return body + front_cham


def shear_y_manifold(m: m3d.Manifold, ky: float) -> m3d.Manifold:
    """Shear Y along +Z: y' = y + ky * z."""
    mat = [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, ky,  0.0],
        [0.0, 0.0, 1.0, 0.0],
    ]
    return m.transform(mat)


def build_front_shell() -> m3d.Manifold:
    # 1) Outer shell & inner cavity (built straight, then sheared 15 deg!)
    outer = chamfered_extrude_shell(OUTER_W, OUTER_H, CORNER_R, SHELL_DEPTH, CHAMFER)

    # Subtle industrial accent groove around side walls at Z=15..16.2
    groove_out = rounded_rect_cs(OUTER_W + 2.0, OUTER_H + 2.0, CORNER_R).extrude(1.0).translate([0, 0, 15.0])
    groove_in = rounded_rect_cs(OUTER_W - 1.1, OUTER_H - 1.1, CORNER_R - 0.5).extrude(1.2).translate([0, 0, 14.9])
    outer = outer - (groove_out - groove_in)

    # Hollow interior from Z = FRONT_WALL to SHELL_DEPTH + 1
    in_w = OUTER_W - 2 * WALL
    in_h = OUTER_H - 2 * WALL
    in_r = max(1.0, CORNER_R - WALL)
    cavity = rounded_rect_cs(in_w, in_h, in_r).extrude(SHELL_DEPTH - FRONT_WALL + 1.0).translate([0, 0, FRONT_WALL])

    # Rear cover step recess (Z = SHELL_DEPTH - COVER_THICK .. SHELL_DEPTH + 1)
    step_w = OUTER_W - 2 * 1.4
    step_h = OUTER_H - 2 * 1.4
    step_cut = rounded_rect_cs(step_w, step_h, max(1.0, CORNER_R - 1.4)).extrude(COVER_THICK + 1.0).translate(
        [0, 0, SHELL_DEPTH - COVER_THICK]
    )

    # 45-degree chamfered TFT viewing window (Z = 0 .. FRONT_WALL + 0.5)
    win_in = rounded_rect_cs(WIN_W, WIN_H, 1.2).extrude(FRONT_WALL + 1.0).translate([WIN_OFF_X, 0, -0.3])
    w0 = rounded_rect_cs(WIN_W + 2 * FRONT_WALL, WIN_H + 2 * FRONT_WALL, 2.2).extrude(0.05).translate([WIN_OFF_X, 0, -0.05])
    w1 = rounded_rect_cs(WIN_W, WIN_H, 1.2).extrude(0.05).translate([WIN_OFF_X, 0, FRONT_WALL])
    win_bevel = (w0 + w1).hull()

    shell = outer - cavity - step_cut - win_in - win_bevel

    # Shear the main body by 15 degrees so bottom rests flat on desk!
    outer_sheared = shear_y_manifold(outer, SHEAR_Y)
    shell = shear_y_manifold(shell, SHEAR_Y)

    # 2) 4 internal TFT M3 standoff bosses (perpendicular to front face at Z=FRONT_WALL..FRONT_WALL+TFT_BOSS_H)
    boss_z0 = FRONT_WALL - 0.3
    boss_h = TFT_BOSS_H + 0.3
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            bx = TFT_HOLE_CX + sx * (TFT_HOLE_DX * 0.5)
            by = sy * (TFT_HOLE_DY * 0.5)
            boss = m3d.Manifold.cylinder(boss_h, 3.2, 3.2, 24).translate([bx, by, boss_z0])
            pilot = m3d.Manifold.cylinder(TFT_BOSS_H + 1.2, 1.35, 1.35, 20).translate([bx, by, 1.3])
            shell = shell + boss - pilot

    # 3) 4 rear corner M3 cover-mounting bosses at rear cover plane (clipped to outer_sheared)
    rear_z_top = SHELL_DEPTH - COVER_THICK
    rear_z_bot = 31.0
    rear_shift = rear_z_top * SHEAR_Y
    boss_add = m3d.Manifold()
    boss_sub = m3d.Manifold()
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            rx = sx * (REAR_HOLE_DX * 0.5)
            ry = sy * (REAR_HOLE_DY * 0.5) + rear_shift
            col_top = m3d.Manifold.cylinder(rear_z_top - rear_z_bot, 3.3, 3.3, 24).translate([rx, ry, rear_z_bot])
            cone_sup = m3d.Manifold.cylinder(5.5, 0.8, 3.3, 24).translate([rx, ry, rear_z_bot - 5.5])
            wall_x = sx * (OUTER_W * 0.5 - WALL * 0.6)
            rib = m3d.Manifold.cube([abs(wall_x - rx) + 2.0, 3.6, rear_z_top - rear_z_bot], center=True).translate(
                [(rx + wall_x) * 0.5, ry, (rear_z_top + rear_z_bot) * 0.5]
            )
            hole = m3d.Manifold.cylinder(11.0, 1.35, 1.35, 20).translate([rx, ry, rear_z_top - 9.5])
            boss_add = boss_add + col_top + cone_sup + rib
            boss_sub = boss_sub + hole
    shell = shell + (boss_add ^ outer_sheared) - boss_sub

    # 4) Rear bottom USB cable exit arch (cleared on rear step edge)
    usb_z = SHELL_DEPTH - COVER_THICK
    usb_y = -(OUTER_H * 0.5) + usb_z * SHEAR_Y
    usb_cut = rounded_rect_cs(15.0, 12.0, 3.5).extrude(10.0).translate([0, usb_y + 1.5, usb_z - 5.0])
    shell = shell - usb_cut

    return shell


def build_back_cover() -> m3d.Manifold:
    # Built flat at Z=0..COVER_THICK (100% support-free!)
    cov_w = OUTER_W - 2 * 1.4 - 2 * FIT_CLEAR
    cov_h = OUTER_H - 2 * 1.4 - 2 * FIT_CLEAR
    cov_r = max(1.0, CORNER_R - 1.4 - FIT_CLEAR)

    plate = rounded_rect_cs(cov_w, cov_h, cov_r).extrude(COVER_THICK)

    # Inner alignment lip
    lip_w = OUTER_W - 2 * WALL - 2 * FIT_CLEAR
    lip_h = OUTER_H - 2 * WALL - 2 * FIT_CLEAR
    lip_r = max(1.0, CORNER_R - WALL - FIT_CLEAR)
    lip_out = rounded_rect_cs(lip_w, lip_h, lip_r).extrude(LIP_H).translate([0, 0, COVER_THICK])
    lip_in = rounded_rect_cs(lip_w - 4.0, lip_h - 4.0, max(0.5, lip_r - 2.0)).extrude(LIP_H + 0.6).translate(
        [0, 0, COVER_THICK - 0.3]
    )
    lip = lip_out - lip_in

    # Cut lip corners where rear bosses sit
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            rx = sx * (REAR_HOLE_DX * 0.5)
            ry = sy * (REAR_HOLE_DY * 0.5)
            lip = lip - m3d.Manifold.cylinder(LIP_H + 1.0, 4.4, 4.4, 24).translate([rx, ry, COVER_THICK - 0.2])

    cover = plate + lip

    # 4 countersunk M3 screw holes
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            rx = sx * (REAR_HOLE_DX * 0.5)
            ry = sy * (REAR_HOLE_DY * 0.5)
            thru = m3d.Manifold.cylinder(COVER_THICK + 1.0, 1.7, 1.7, 20).translate([rx, ry, -0.5])
            csink = m3d.Manifold.cylinder(1.6, 3.2, 1.7, 24).translate([rx, ry, -0.05])
            cover = cover - thru - csink

    # 7 upper ventilation slots (2.4mm wide, rounded)
    for i in range(7):
        vx = -21.0 + i * 7.0
        slot = rounded_rect_cs(2.4, 15.0, 1.1).extrude(COVER_THICK + 1.0).translate([vx, 12.0, -0.5])
        cover = cover - slot

    # USB-C / Micro-USB cable exit notch at bottom center
    usb_notch = rounded_rect_cs(14.0, 10.0, 3.0).extrude(COVER_THICK + LIP_H + 1.0).translate(
        [0, -cov_h * 0.5 + 2.5, -0.5]
    )
    cover = cover - usb_notch

    # Internal ESP32 DevKit (28.6mm) & C3 Mini (18.6mm) cradle rails
    for sx in (-1.0, 1.0):
        rail_dev = m3d.Manifold.cube([2.2, 34.0, 4.2], center=True).translate(
            [sx * 15.6, -4.0, COVER_THICK + 2.1]
        )
        rail_c3 = m3d.Manifold.cube([1.8, 22.0, 3.2], center=True).translate(
            [sx * 10.4, -7.0, COVER_THICK + 1.6]
        )
        cover = cover + rail_dev + rail_c3

    return cover


def export_binary_stl(m: m3d.Manifold, path: pathlib.Path) -> tuple[int, float, tuple[np.ndarray, np.ndarray]]:
    mesh = m.to_mesh()
    verts = np.asarray(mesh.vert_properties, dtype=np.float32)[:, :3]
    tris = np.asarray(mesh.tri_verts, dtype=np.int32)
    path.parent.mkdir(parents=True, exist_ok=True)

    header = b"ESP32 DeskHub 3D Printable Enclosure - Manifold3D".ljust(80, b"\x00")
    with open(path, "wb") as f:
        f.write(header)
        f.write(struct.pack("<I", len(tris)))
        v0 = verts[tris[:, 0]]
        v1 = verts[tris[:, 1]]
        v2 = verts[tris[:, 2]]
        norms = np.cross(v1 - v0, v2 - v0)
        lens = np.linalg.norm(norms, axis=1, keepdims=True)
        lens = np.where(lens < 1e-9, 1.0, lens)
        norms = (norms / lens).astype(np.float32)

        buf = bytearray()
        for i in range(len(tris)):
            buf += struct.pack(
                "<12fH",
                float(norms[i, 0]), float(norms[i, 1]), float(norms[i, 2]),
                float(v0[i, 0]), float(v0[i, 1]), float(v0[i, 2]),
                float(v1[i, 0]), float(v1[i, 1]), float(v1[i, 2]),
                float(v2[i, 0]), float(v2[i, 1]), float(v2[i, 2]),
                0,
            )
        f.write(buf)

    vol_cm3 = m.volume() / 1000.0
    bmin = verts.min(axis=0)
    bmax = verts.max(axis=0)
    return len(tris), vol_cm3, (bmin, bmax)


# ---------------------------------------------------------------------------
# FAST PURE-NUMPY Z-BUFFERED SHADED 3D RENDERER FOR 4-VIEW PREVIEW PNG
# ---------------------------------------------------------------------------
def look_at_matrix(eye: np.ndarray, target: np.ndarray, up: np.ndarray = np.array([0.0, 0.0, 1.0])) -> np.ndarray:
    fwd = target - eye
    fwd /= np.linalg.norm(fwd)
    right = np.cross(fwd, up)
    right /= np.linalg.norm(right)
    true_up = np.cross(right, fwd)
    mat = np.eye(4, dtype=np.float32)
    mat[0, :3] = right
    mat[1, :3] = true_up
    mat[2, :3] = -fwd
    mat[:3, 3] = -mat[:3, :3] @ eye
    return mat


def rasterize_meshes(
    parts: list[tuple[m3d.Manifold, tuple[int, int, int]]],
    width: int,
    height: int,
    eye: np.ndarray,
    target: np.ndarray,
    bg_col: tuple[int, int, int] = (14, 17, 23),
    draw_bed_grid: bool = False,
) -> Image.Image:
    view = look_at_matrix(eye, target)
    img_buf = np.full((height, width, 3), bg_col, dtype=np.uint8)
    zbuf = np.full((height, width), -1e9, dtype=np.float32)

    # First project all meshes to compute auto-fit scale
    all_cam = []
    for m, col in parts:
        mesh = m.to_mesh()
        v = np.asarray(mesh.vert_properties, dtype=np.float32)[:, :3]
        t = np.asarray(mesh.tri_verts, dtype=np.int32)
        ones = np.ones((len(v), 1), dtype=np.float32)
        vh = np.hstack([v, ones])
        vc = (view @ vh.T).T[:, :3]
        all_cam.append((vc, t, col))

    pts = np.vstack([vc for vc, _, _ in all_cam])
    xmin, ymin = pts[:, 0].min(), pts[:, 1].min()
    xmax, ymax = pts[:, 0].max(), pts[:, 1].max()
    span = max(xmax - xmin, ymax - ymin, 1.0)
    scale = min(width, height) * 0.78 / span
    cx_p = (xmin + xmax) * 0.5
    cy_p = (ymin + ymax) * 0.5

    # Optional build-plate grid on Z=0
    if draw_bed_grid:
        bed_lines = []
        for g in range(-110, 111, 22):
            bed_lines.append(([g, -110.0, 0.0], [g, 110.0, 0.0]))
            bed_lines.append(([-110.0, g, 0.0], [110.0, g, 0.0]))
        im_tmp = Image.fromarray(img_buf)
        dr_tmp = ImageDraw.Draw(im_tmp)
        for p0, p1 in bed_lines:
            p0c = (view @ np.array([p0[0], p0[1], p0[2], 1.0], dtype=np.float32))[:3]
            p1c = (view @ np.array([p1[0], p1[1], p1[2], 1.0], dtype=np.float32))[:3]
            x0, y0 = width * 0.5 + (p0c[0] - cx_p) * scale, height * 0.5 - (p0c[1] - cy_p) * scale
            x1, y1 = width * 0.5 + (p1c[0] - cx_p) * scale, height * 0.5 - (p1c[1] - cy_p) * scale
            dr_tmp.line([x0, y0, x1, y1], fill=(32, 42, 58), width=1)
        img_buf = np.array(im_tmp)

    l1 = np.array([0.45, 0.55, 0.70], dtype=np.float32)
    l1 /= np.linalg.norm(l1)
    l2 = np.array([-0.60, -0.25, 0.45], dtype=np.float32)
    l2 /= np.linalg.norm(l2)

    for vc, tris, base_col in all_cam:
        sx = width * 0.5 + (vc[:, 0] - cx_p) * scale
        sy = height * 0.5 - (vc[:, 1] - cy_p) * scale
        sz = vc[:, 2]

        v0 = vc[tris[:, 0]]
        v1 = vc[tris[:, 1]]
        v2 = vc[tris[:, 2]]
        fn = np.cross(v1 - v0, v2 - v0)
        fl = np.linalg.norm(fn, axis=1, keepdims=True)
        fl = np.where(fl < 1e-8, 1.0, fl)
        fn /= fl

        # Backface cull in camera space (camera looks toward -Z, visible faces have fn_z > 0)
        vis = fn[:, 2] > -0.02
        vis_idx = np.nonzero(vis)[0]
        if len(vis_idx) == 0:
            continue

        # Sort back-to-front or use per-pixel zbuf
        t_vis = tris[vis_idx]
        n_vis = fn[vis_idx]
        d1 = np.clip(n_vis @ l1, 0.0, 1.0)
        d2 = np.clip(n_vis @ l2, 0.0, 1.0)
        # Subtle specular highlight on key light
        spec = np.power(np.clip(n_vis[:, 2] * 0.7 + d1 * 0.5, 0.0, 1.0), 16) * 0.22
        lum = 0.24 + 0.62 * d1 + 0.18 * d2 + spec

        bcol = np.array(base_col, dtype=np.float32)
        tri_cols = np.clip(bcol[None, :] * lum[:, None] + spec[:, None] * 60.0, 0, 255).astype(np.uint8)

        x0 = sx[t_vis[:, 0]]; y0 = sy[t_vis[:, 0]]; z0 = sz[t_vis[:, 0]]
        x1 = sx[t_vis[:, 1]]; y1 = sy[t_vis[:, 1]]; z1 = sz[t_vis[:, 1]]
        x2 = sx[t_vis[:, 2]]; y2 = sy[t_vis[:, 2]]; z2 = sz[t_vis[:, 2]]

        bx0 = np.maximum(0, np.floor(np.minimum(np.minimum(x0, x1), x2)).astype(np.int32))
        bx1 = np.minimum(width - 1, np.ceil(np.maximum(np.maximum(x0, x1), x2)).astype(np.int32))
        by0 = np.maximum(0, np.floor(np.minimum(np.minimum(y0, y1), y2)).astype(np.int32))
        by1 = np.minimum(height - 1, np.ceil(np.maximum(np.maximum(y0, y1), y2)).astype(np.int32))

        for k in range(len(t_vis)):
            minx, maxx, miny, maxy = bx0[k], bx1[k], by0[k], by1[k]
            if minx > maxx or miny > maxy:
                continue
            area = (x1[k] - x0[k]) * (y2[k] - y0[k]) - (y1[k] - y0[k]) * (x2[k] - x0[k])
            if abs(area) < 1e-4:
                continue
            inv_a = 1.0 / area
            xs = np.arange(minx, maxx + 1, dtype=np.float32)[None, :] + 0.5
            ys = np.arange(miny, maxy + 1, dtype=np.float32)[:, None] + 0.5

            w0 = ((x1[k] - xs) * (y2[k] - ys) - (y1[k] - ys) * (x2[k] - xs)) * inv_a
            w1 = ((x2[k] - xs) * (y0[k] - ys) - (y2[k] - ys) * (x0[k] - xs)) * inv_a
            w2 = 1.0 - w0 - w1

            inside = (w0 >= -1e-4) & (w1 >= -1e-4) & (w2 >= -1e-4)
            if not np.any(inside):
                continue
            zz = w0 * z0[k] + w1 * z1[k] + w2 * z2[k]
            sub_z = zbuf[miny : maxy + 1, minx : maxx + 1]
            mask = inside & (zz > sub_z)
            if np.any(mask):
                sub_z[mask] = zz[mask]
                img_buf[miny : maxy + 1, minx : maxx + 1][mask] = tri_cols[k]

    return Image.fromarray(img_buf)


def build_showcase_png(front: m3d.Manifold, back: m3d.Manifold, assembled: m3d.Manifold) -> None:
    # Simulated glowing TFT screen slab inside front shell for front 3/4 view
    tft_plate = m3d.Manifold.cube([WIN_W - 0.6, WIN_H - 0.6, 1.2], center=True).translate([WIN_OFF_X, 0, FRONT_WALL + 0.8])
    tft_upright = tft_plate.rotate([90.0 + TILT_DEG, 0, 0]).rotate([0, 0, 180.0])
    back_mated = back.rotate([0, 180.0, 0]).translate([0, (SHELL_DEPTH - COVER_THICK) * SHEAR_Y, SHELL_DEPTH])
    ab = (front + back_mated).rotate([90.0 + TILT_DEG, 0, 0]).rotate([0, 0, 180.0]).bounding_box()
    z_lift = -ab[2]
    tft_upright = tft_upright.translate([0, 0, z_lift])

    # Exploded view (front shell upright-tilted & back cover separated behind it)
    front_up = front.rotate([90.0 + TILT_DEG, 0, 0]).rotate([0, 0, 180.0]).translate([0, 0, z_lift])
    back_exploded = back_mated.rotate([90.0 + TILT_DEG, 0, 0]).rotate([0, 0, 180.0]).translate([0, 38.0, z_lift])

    # Build-plate side-by-side layout at Z=0
    plate_front = front.translate([-66.0, -5.0, 0.0])
    plate_back = back.translate([66.0, 0.0, 0.0])

    pw, ph = 660, 430
    p1 = rasterize_meshes(
        [(assembled, (124, 134, 152)), (tft_upright, (56, 152, 250))],
        pw, ph, np.array([115.0, -142.0, 78.0]), np.array([0.0, -8.0, 28.0]),
    )
    p2 = rasterize_meshes(
        [(assembled, (124, 134, 152))],
        pw, ph, np.array([-118.0, 142.0, 82.0]), np.array([0.0, -12.0, 28.0]),
    )
    p3 = rasterize_meshes(
        [(front_up, (124, 134, 152)), (back_exploded, (224, 158, 58))],
        pw, ph, np.array([150.0, 96.0, 96.0]), np.array([0.0, 8.0, 28.0]),
    )
    p4 = rasterize_meshes(
        [(plate_front, (124, 134, 152)), (plate_back, (224, 158, 58))],
        pw, ph, np.array([0.0, -175.0, 155.0]), np.array([0.0, 0.0, 14.0]),
        draw_bed_grid=True,
    )

    sheet = Image.new("RGB", (1400, 1000), (11, 14, 20))
    d = ImageDraw.Draw(sheet)
    f_t = ImageFont.truetype(FONT_BOLD, 28)
    f_c = ImageFont.truetype(FONT_BOLD, 19)
    f_s = ImageFont.truetype(FONT_REG, 15)

    d.text((700, 32), "ESP32 DeskHub • 3D Baskıya Uygun 15° Masaüstü Kutu (Desteksiz / Support-Free)",
           font=f_t, fill=(245, 247, 250), anchor="mm")

    panels = [
        ("1. Ön 3/4 Görünüm (15° Masaüstü Eğimi)", p1),
        ("2. Arka 3/4 Görünüm (Havalandırma & USB Çıkışı)", p2),
        ("3. Patlatılmış Montaj (TFT & ESP32 Yuvaları)", p3),
        ("4. 3D Yazıcı Tabla Yerleşimi (Sıfır Destek)", p4),
    ]
    for idx, (cap, pim) in enumerate(panels):
        col, row = idx % 2, idx // 2
        x = 26 + col * (pw + 28)
        y = 70 + row * (ph + 46)
        d.text((x + pw // 2, y + 12), cap, font=f_c, fill=(56, 147, 255), anchor="mm")
        d.rounded_rectangle([x - 2, y + 26, x + pw + 2, y + 30 + ph], radius=8, outline=(48, 56, 72), width=2)
        sheet.paste(pim, (x, y + 28))

    d.text(
        (700, 982),
        "Dış Boyut: 112×66 mm  •  TFT Delik: 68×37 mm (77×42 PCB)  •  Pin Boşluğu: ~11.5 mm  •  PLA/PETG 0.20 mm Desteksiz",
        font=f_s, fill=(155, 165, 180), anchor="mm",
    )

    out_enc = ENC_DIR / "enclosure_preview.png"
    sheet.save(out_enc)
    shutil.copy2(out_enc, ARTIFACT_PREVIEW)
    print(f"Saved 3D showcase render: {out_enc}")


def main() -> None:
    front = build_front_shell()
    back = build_back_cover()

    # Check watertight manifold status
    assert front.status() == m3d.Error.NoError, f"Front error: {front.status()}"
    assert back.status() == m3d.Error.NoError, f"Back error: {back.status()}"

    # Mated upright 15-deg desk assembly (front faces -Y, tilted 15 deg upward, flat base on Z=0)
    back_mated = back.rotate([0, 180.0, 0]).translate([0, (SHELL_DEPTH - COVER_THICK) * SHEAR_Y, SHELL_DEPTH])
    assembled = (front + back_mated).rotate([90.0 + TILT_DEG, 0, 0]).rotate([0, 0, 180.0])
    ab = assembled.bounding_box()
    assembled = assembled.translate([0, 0, -ab[2]])

    for name, m in [
        ("deskhub_front_shell.stl", front),
        ("deskhub_back_cover.stl", back),
        ("deskhub_assembled_preview.stl", assembled),
    ]:
        tris, vol_cm3, (bmin, bmax) = export_binary_stl(m, STL_DIR / name)
        dims = bmax - bmin
        pla_g = vol_cm3 * 1.24  # solid PLA density ~1.24 g/cm3 (actual ~50% less with 18% infill)
        print(
            f"{name}: {tris} tris, {vol_cm3:.2f} cm3 (~{pla_g * 0.65:.1f} g @18% infill), "
            f"bbox={dims[0]:.1f}x{dims[1]:.1f}x{dims[2]:.1f} mm"
        )

    build_showcase_png(front, back, assembled)


if __name__ == "__main__":
    main()
