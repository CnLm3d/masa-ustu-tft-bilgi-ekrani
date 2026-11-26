// ESP32 DeskHub v2.0 — Parametric 15-deg Support-Free Enclosure (OpenSCAD)
// Compatible with TJCTM24024-SPI (2.4" ILI9341) + ESP32 DevKit / ESP32-C3 Mini

$fn = 48;

outer_w = 112.0;
outer_h = 66.0;
corner_r = 6.0;
wall = 2.6;
front_wall = 2.4;
chamfer = 1.6;
shell_depth = 44.0;
tilt_deg = 15.0;
shear_y = tan(tilt_deg);

win_w = 50.8;
win_h = 38.4;
win_off_x = 0.0;

tft_hole_dx = 68.0;
tft_hole_dy = 37.0;
tft_hole_cx = -3.5;
tft_boss_h = 2.8;

cover_thick = 2.6;
lip_h = 1.8;
fit_clear = 0.35;
rear_hole_dx = 99.0;
rear_hole_dy = 53.0;

module rrect(w, h, r) {
    offset(r = r) square([w - 2 * r, h - 2 * r], center = true);
}

module front_shell() {
    union() {
        difference() {
            multmatrix([[1,0,0,0],[0,1,shear_y,0],[0,0,1,0]]) {
                difference() {
                    union() {
                        hull() {
                            linear_extrude(0.05) rrect(outer_w - 2*chamfer, outer_h - 2*chamfer, corner_r - chamfer);
                            translate([0,0,chamfer]) linear_extrude(0.05) rrect(outer_w, outer_h, corner_r);
                        }
                        translate([0,0,chamfer]) linear_extrude(shell_depth - chamfer) rrect(outer_w, outer_h, corner_r);
                    }
                    // Inner cavity
                    translate([0,0,front_wall]) linear_extrude(shell_depth) rrect(outer_w - 2*wall, outer_h - 2*wall, corner_r - wall);
                    // Rear step
                    translate([0,0,shell_depth - cover_thick]) linear_extrude(cover_thick + 1) rrect(outer_w - 2.8, outer_h - 2.8, corner_r - 1.4);
                    // Viewing window + 45-deg bevel
                    translate([win_off_x, 0, -0.2]) linear_extrude(front_wall + 1) rrect(win_w, win_h, 1.2);
                    hull() {
                        translate([win_off_x, 0, -0.05]) linear_extrude(0.05) rrect(win_w + 2*front_wall, win_h + 2*front_wall, 2.2);
                        translate([win_off_x, 0, front_wall]) linear_extrude(0.05) rrect(win_w, win_h, 1.2);
                    }
                }
            }
            // USB exit arch at bottom rear
            translate([0, -outer_h/2 + (shell_depth - cover_thick)*shear_y + 1.5, shell_depth - cover_thick - 5])
                linear_extrude(10) rrect(15, 12, 3.5);
        }
        // TFT M3 standoffs (68x37mm centered at -3.5mm so active screen aligns with window)
        for (sx = [-1, 1], sy = [-1, 1]) {
            translate([tft_hole_cx + sx * tft_hole_dx/2, sy * tft_hole_dy/2, front_wall - 0.2])
                difference() {
                    cylinder(h = tft_boss_h + 0.2, r = 3.2);
                    translate([0, 0, -1.0]) cylinder(h = tft_boss_h + 2.0, r = 1.35);
                }
        }
    }
}

module back_cover() {
    cov_w = outer_w - 2.8 - 2*fit_clear;
    cov_h = outer_h - 2.8 - 2*fit_clear;
    cov_r = corner_r - 1.4 - fit_clear;
    difference() {
        union() {
            linear_extrude(cover_thick) rrect(cov_w, cov_h, cov_r);
            translate([0, 0, cover_thick]) linear_extrude(lip_h)
                difference() {
                    rrect(outer_w - 2*wall - 2*fit_clear, outer_h - 2*wall - 2*fit_clear, corner_r - wall - fit_clear);
                    rrect(outer_w - 2*wall - 2*fit_clear - 4, outer_h - 2*wall - 2*fit_clear - 4, 1.0);
                }
            for (sx = [-1, 1]) {
                translate([sx * 15.6, -4, cover_thick + 2.1]) cube([2.2, 34, 4.2], center = true);
                translate([sx * 10.4, -7, cover_thick + 1.6]) cube([1.8, 22, 3.2], center = true);
            }
        }
        for (sx = [-1, 1], sy = [-1, 1]) {
            translate([sx * rear_hole_dx/2, sy * rear_hole_dy/2, -0.5]) cylinder(h = cover_thick + 1, r = 1.7);
            translate([sx * rear_hole_dx/2, sy * rear_hole_dy/2, -0.05]) cylinder(h = 1.6, r1 = 3.2, r2 = 1.7);
        }
        for (i = [0:6]) {
            translate([-21 + i*7, 12, -0.5]) linear_extrude(cover_thick + 1) rrect(2.4, 15, 1.1);
        }
        translate([0, -cov_h/2 + 2.5, -0.5]) linear_extrude(cover_thick + lip_h + 1) rrect(14, 10, 3.0);
    }
}

// Print layout (both flat at Z=0)
translate([-66, 0, 0]) front_shell();
translate([66, 0, 0]) back_cover();
