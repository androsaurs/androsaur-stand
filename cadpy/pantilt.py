"""Pan-tilt mechanism on two MG996R servos, rebuilt in build123d.

Geometry follows micromet's OpenSCAD sources (Thingiverse 1941574, CC BY-SA)
in references/pan-tilt-1941574/scad-rev; servo dimensions come from the
user-supplied MG996R STL. Units: mm. World: Z up, base corner at origin.
"""
from build123d import *

import clip_holder
import enclosure
import speaker as spk
import esp32
import amp

# ---- parameters -----------------------------------------------------------
RISER = 104.0       # top of the hollow leg above the sole bottom (SCAD original 12): lets the Type-C cable meet the charger window at the vertical pose
SOLE = 4.0
BASE_L, BASE_W = 85.0, 85.0   # sole footprint (SCAD original: 85 x 42); leg stays in the centre
LEG_R, CAVITY_R = 18.2, 15.2   # wall = 3.0 (was 6.2 with CAVITY_R 12)
CAVITY_TOP = RISER - 7.0   # = bottom of the carrier cup (RISER + ROOF_GAP - 7.5), so the cavity ends where the cup starts
SEAT_DEPTH = 4.0
WINDOWS = (         # (name, width, height, fraction of visible leg height from the bottom)
    ("charger", 12.0, 18.0, 0.25),
    ("motors", 10.0, 16.0, 0.75),
)
WINDOW_R = 2.0
ROOF_GAP = 0.5      # clearance between riser top and carrier roof
TILT_GAP_A = 0.5    # servo boss to horn-side plate
PIVOT_DX = 37.5     # tilt axis offset from carrier centre (from the SCAD plates)
BOLT_DZ = 43.5      # tilt axis height above the plate bolt line (from the SCAD plates)

HOLDER = "clip"    # "clip" = Body + 82mm clip, "tray" = one-piece tray
PLATFORM_X0, PLATFORM_X1 = -33.0, 30.0   # platform extent along its length axis
PLATFORM_Y1 = 27.5                          # pillow-block side edge, position unchanged
# widen on the other side so the plate is centred on the base: the mechanism was designed around a 42 mm sole (centre 21),
# and the platform frame origin sits at 25.5 + TILT_GAP_A along that axis
PLATFORM_Y0 = 2 * (21.0 - (25.5 + TILT_GAP_A)) - PLATFORM_Y1
HOLDER_HOLES = ((-18.0, -15.0), (-18.0, 5.0), (15.0, -15.0), (15.0, 5.0))   # (x_p, y_p) in the platform frame: between the servo blocks (|x_p| 22..19 gap), nothing built above the plate there
MOUNT_PAD = 2.0      # extra platform thickness on the holder side, over the whole plate (3 -> 5 mm), keeps it printable without supports
HOLDER_SLIDE = -8.0  # clip holder shifted along the phone height (negative = down) so the guides and clip hang off the platform edge
MIN3 = (Align.MIN, Align.MIN, Align.MIN)


def bx(x0, x1, y0, y1, z0, z1):
    return Pos((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2) * Box(x1 - x0, y1 - y0, z1 - z0)


def cz(r, h, x, y, z0):
    return Pos(x, y, z0) * Cylinder(r, h, align=(Align.CENTER, Align.CENTER, Align.MIN))


def cyl_y(r, length, x, yc, z):
    return Pos(x, yc, z) * Rot(X=90) * Cylinder(r, length)


def prism_xz(pts, y0, y1):
    """Polygon given in (x, z) extruded along Y from y0 to y1."""
    f = Face(Wire.make_polygon([Vector(x, 0, z) for x, z in pts], close=True))
    return Pos(0, y0, 0) * Solid.extrude(f, Vector(0, y1 - y0, 0))


def prism_xy(pts, z0, z1):
    f = Face(Wire.make_polygon([Vector(x, y, 0) for x, y in pts], close=True))
    return Pos(0, 0, z0) * Solid.extrude(f, Vector(0, 0, z1 - z0))


# ---- servo MG996R (shaft +Z, body centred on XY, shaft at x=+10) ----------
def servo():
    s = bx(-20, 20, -9.5, 9.5, 0, 36)
    s += bx(-18, 18, -9.5, 9.5, 36, 37.5)
    s += bx(-27, 27, -9.5, 9.5, 26.5, 29.5)
    for sx in (-24.75, 24.75):
        for sy in (-5, 5):
            s -= cz(2.1, 4, sx, sy, 26)
    s += cz(6.5, 1.5, 10, 0, 37.5)
    s += cz(5.5, 1.0, 10, 0, 39)
    s += cz(2.8, 3.5, 10, 0, 40)
    return s


# ---- base: plate with riser ----------------------------------------------
def base():
    p = bx(0, BASE_L, 0, BASE_W, 0, SOLE)
    p = fillet(p.edges().filter_by(Axis.Z), 4)
    cx, cy = BASE_L / 2, BASE_W / 2
    p += cz(LEG_R, RISER, cx, cy, 0)
    p -= cz(CAVITY_R, CAVITY_TOP + 1, cx, cy, -1)   # open through the sole, as in base.FCStd
    p -= cz(4.2, RISER, cx, cy, CAVITY_TOP - 1)
    p -= cz(10.7, SEAT_DEPTH, cx, cy, RISER - SEAT_DEPTH)
    for dx, dy in ((-7.8, 0), (7.8, 0), (0, -7.8), (0, 7.8)):
        p -= cz(1, 9, cx + dx, cy + dy, RISER - 9)
    for x, y in ((6, 6), (6, BASE_W - 6), (BASE_L - 6, BASE_W - 6), (BASE_L - 6, 6)):
        p -= cz(2, SOLE + 2, x, y, -1)
        p -= cz(enclosure.CBORE_D / 2, enclosure.CBORE_DEPTH + 1, x, y, -1)   # head recess on the underside: M3x6 reaches the lid
    visible = CAVITY_TOP - SOLE
    for _, w, h, frac in WINDOWS:
        win = Box(25, w, h)
        win = fillet(win.edges().filter_by(Axis.X), WINDOW_R)
        p -= Pos(cx + 12.5, cy, SOLE + visible * frac) * win
    return p


# ---- pan carrier (PanMount7): frame as in the SCAD ------------------------
def carrier():
    p = bx(-30.5, 30.5, -21, 21, -3, 7)
    p += cz(21, 10, 10, 0, 7)
    p -= cz(18.5, 10, 10, 0, 9.5)
    p -= cz(7, 30, 10, 0, -15)
    p -= bx(-27.5, 27.5, -10.2, 10.2, -3, 0)
    p -= bx(-20.5, 20.5, -10.2, 10.2, -3, 8)
    p -= bx(-27.5, 27.5, -1.2, 1.2, 0, 4)
    for sx in (-24.75, 24.75):
        for sy in (-5, 5):
            p -= cz(1, 12, sx, sy, -6)
    for x in (-18, 18):
        p -= cyl_y(1.25, 52, x, 0, 2)
    return p


# ---- side plates ----------------------------------------------------------
def horn_pocket():
    arm = prism_xy([(-5.5, 0), (5.5, 0), (5.5, 2), (3, 19), (-3, 19), (-5.5, 2)], 1.5, 6.5)
    arm += cz(3, 5, 0, 19, 1.5)
    arm += cz(5.5, 5, 0, 0, 1.5)
    return Pos(37.5, 22.5, 0) * Rot(Z=90) * arm


def plate_horn_side():
    p = bx(-30, 30, -26, 30, -1.5, 1.5)
    p += cz(7.5, 6, -37.5, 22.5, -4.5)
    p += bx(-37.5, -11.5, 15, 30, -4.5, 1.5)
    p -= cz(10, 8, -5, -5, -4)
    for x in (-18, 18):
        p -= cz(1.5, 8, x, -21, -4)
    p -= Pos(20, 20, 0) * Rot(Z=45) * Box(30, 60, 10)
    p -= cz(4.25, 12, -37.5, 22.5, -6)
    for x in (-26.5, -20):
        p -= cz(1, 12, x, 22.5, -6)
    p = Rot(Y=180) * p
    p -= horn_pocket()
    return p


def plate_shaft_side():
    p = bx(-30, 30, -28, 28, -1.5, 1.5)
    p += cz(7.5, 3, -37.5, 20.5, -1.5)
    p += bx(-37.5, -22.5, 13, 28, -1.5, 1.5)
    p += cz(3, 5, -37.5, 20.5, 1.5)
    p -= cz(10, 8, -7, -5, -4)
    for x in (-18, 18):
        p -= cz(1.5, 8, x, -23, -4)
    p -= Pos(18.75, 18.75, 0) * Rot(Z=45) * Box(32.5, 65, 10)
    return p


# ---- tilt platform --------------------------------------------------------
def wsupport(x, y):
    return prism_xz([(0, 0), (x, 0), (x, x)], 0, y)


def platform():
    pad = MOUNT_PAD if HOLDER == "clip" else 0.0   # whole plate thickened on the holder side, back side unchanged
    p = bx(PLATFORM_X0, PLATFORM_X1, PLATFORM_Y0, PLATFORM_Y1, -1.5 - pad, 1.5)
    p += bx(19, 29, -12, 0, -0.5, 20.5)
    p += bx(-32, -22, -12, 0, 0, 11)
    p += Pos(19, 9.5, 1) * Rot(Z=-90) * wsupport(10, 10)
    p += Pos(-32, 9.5, 1) * Rot(Z=-90) * wsupport(10, 10)
    p += cyl_y(6, 6, -11.8, 24.5, 11)
    p += bx(-17.8, -5.8, 21.5, 27.5, -1, 11)
    p += Pos(-30, 21.5, 1) * wsupport(15, 6)
    p += Pos(6.5, 27.5, 1) * Rot(Z=-180) * wsupport(15, 6)
    for x in (23, -26):
        for z in (16.5, 6.5):
            p -= cyl_y(1.2, 55, x, -7.5, z)
    p -= cyl_y(3.2, 56, -11.8, 0, 11)
    for x, y in HOLDER_HOLES:
        p -= cz(clip_holder.MOUNT_HOLE_D / 2, 6 + pad, x, y, -3 - pad)
    return p


# ---- phone holder (platform frame; phone on the -Z side of the platform) ---
PHONE_H, PHONE_W, PHONE_T = 170.0, 80.0, 10.0
FIT, WALL, BACK = 0.3, 2.0, 3.0
AXIS_X = -11.8      # tilt axis passes through the middle of the phone height
FACE_Z = -1.5       # platform face the holder back plate rests on


def holder():
    length = PHONE_H + 2 * FIT + 2 * WALL
    width = PHONE_W + 2 * FIT + 2 * WALL
    x0, x1 = AXIS_X - length / 2, AXIS_X + length / 2
    zb = FACE_Z - BACK
    zr = zb - PHONE_T - 0.5
    p = bx(x0, x1, -width / 2, width / 2, zb, FACE_Z)
    p += bx(x0, x1, -width / 2, -width / 2 + WALL, zr, zr + PHONE_T + 0.6)
    p += bx(x0, x1, width / 2 - WALL, width / 2, zr, zr + PHONE_T + 0.6)
    p += bx(x0, x0 + WALL, -width / 2, width / 2, zr, zr + PHONE_T + 0.6)
    for x, y in ((23, 17), (23, -21), (-27, -21), (-27, 17)):
        p -= cz(1.7, 5, x, y, -5.5)
    return p


def phone():
    zb = FACE_Z - BACK
    x0 = AXIS_X - PHONE_H / 2   # lower edge rests on the bottom stop (stop 2 mm + 0.3 fit offsets cancel)
    return bx(x0, x0 + PHONE_H, -PHONE_W / 2, PHONE_W / 2, zb - PHONE_T, zb)


# ---- assembly -------------------------------------------------------------
def assemble(tilt_deg=0.0, pan_deg=0.0, cable=False, box=True, speaker=True):
    """Returns dict name -> positioned Shape. tilt_deg: 0 = platform horizontal (top face up), 90 = vertical (top face toward +X)."""
    zc = RISER + ROOF_GAP + 9.5          # carrier frame origin height (flipped)
    zp = zc - 2 + BOLT_DZ                # tilt axis height
    xp = 32.5 + PIVOT_DX
    y0p = 25.5 + TILT_GAP_A              # platform frame origin along Y

    to_carrier = Pos(32.5, 21, zc) * Rot(X=180)
    parts = {
        "base": base(),
        "carrier": to_carrier * carrier(),
        "servo_pan": to_carrier * Pos(0, 0, -29.5) * servo(),
        "plate_horn": Pos(32.5, -1.5, zp - 22.5) * Rot(X=90) * plate_horn_side(),
        "plate_shaft": Pos(32.5, 43.5, zp - 20.5) * Rot(Z=180) * Rot(X=90) * plate_shaft_side(),
    }
    to_platform = Pos(xp - 11.8, y0p, zp + 11) * Rot(Y=180)
    tilt = Pos(xp, 0, zp) * Rot(Y=tilt_deg) * Pos(-xp, 0, -zp)
    parts["platform"] = tilt * to_platform * platform()
    if HOLDER == "clip":
        # holder frame (X across, Y towards phone, Z along phone) -> platform frame; phone middle on the tilt axis
        PLATFORM_CY = (PLATFORM_Y0 + PLATFORM_Y1) / 2
        origin_x = AXIS_X - (clip_holder.PHONE_BOTTOM_Z + clip_holder.PHONE_H / 2) + HOLDER_SLIDE
        body_len = PLATFORM_X1 - origin_x    # spine runs on up to the platform's upper edge
        mount = Location(Plane(origin=(origin_x, PLATFORM_CY, FACE_Z - MOUNT_PAD + clip_holder.PLATE_Y0),   # flat back of the plate rests on the mounting pad
                               x_dir=(0, -1, 0), z_dir=(1, 0, 0)))
        on_platform = tilt * to_platform * mount
        body_holes = [(x - origin_x, y - PLATFORM_CY + clip_holder.BODY_W / 2) for x, y in HOLDER_HOLES]
        parts["body"] = on_platform * clip_holder.body(body_len, body_holes)
        parts["grip"] = on_platform * clip_holder.grip()
        parts["phone"] = on_platform * clip_holder.phone()
        parts["plug"] = on_platform * clip_holder.plug()
        if cable:
            parts["cable"] = on_platform * clip_holder.cable()
    else:
        parts["holder"] = tilt * to_platform * holder()
        parts["phone"] = tilt * to_platform * phone()
    parts["servo_tilt"] = tilt * to_platform * Pos(-1.5, 14.5, 11.5) * Rot(X=90) * Rot(Z=180) * servo()

    dy = BASE_W / 2 - 21                 # mechanism was designed around a 42 mm wide sole
    cx, cy = BASE_L / 2, BASE_W / 2
    pan = Pos(cx, cy, 0) * Rot(Z=pan_deg) * Pos(-cx, -cy, 0)
    for k in parts:
        if k != "base":
            parts[k] = pan * Pos(0, dy, 0) * parts[k]
    if box:   # enclosure: base plate hangs under the ceiling inside the box, the leg passes through the ceiling hole
        stand_holes = [(x - BASE_L / 2, y - BASE_W / 2) for x, y in ((6, 6), (6, BASE_W - 6), (BASE_L - 6, BASE_W - 6), (BASE_L - 6, 6))]
        to_box = Pos(cx, cy, SOLE + enclosure.TOP_T - enclosure.BOX_H)
        parts["box_lower"] = to_box * enclosure.lower()
        parts["box_upper"] = to_box * enclosure.upper(stand_holes)
        if speaker:   # FD92-1 face down on standoffs over the floor grille, centred under the leg
            parts["shelf"] = to_box * enclosure.shelf()
            # ESP32 lies on the flat shelf top (pins through slots, antenna to the -Y wall); the amplifier hangs under the shelf
            parts["esp32"] = to_box * Location(Plane(origin=(enclosure.ESP_X, enclosure.ESP_Y_USB, enclosure.SHELF_Z + enclosure.SHELF_T + esp32.HEADER_H),
                                                     x_dir=(0, -1, 0), z_dir=(0, 0, 1))) * esp32.board()
            parts["amp"] = to_box * Location(Plane(origin=(enclosure.TPA_X, enclosure.TPA_Y0, enclosure.TPA_FACE_Z),
                                                   x_dir=(1, 0, 0), z_dir=(0, 0, -1))) * amp.tpa3118()
            parts["dac"] = to_box * Location(Plane(origin=(enclosure.DAC_X, enclosure.DAC_Y0, enclosure.DAC_FACE_Z),
                                                   x_dir=(0, 1, 0), z_dir=(0, 0, -1))) * amp.pcm5102()
            # power module placeholder lies flat on the shelf top (the DAC is removed)
            parts["psu"] = to_box * Pos(enclosure.PSU_X0, enclosure.PSU_Y0, enclosure.PSU_Z0) * amp.psu_block()
            for i, (fx, fy) in enumerate(enclosure.FOOT_POS):
                parts[f"foot{i}"] = to_box * Pos(fx, fy, 0) * enclosure.foot()
            parts["speaker"] = to_box * Pos(enclosure.SPK_X, enclosure.SPK_Y, enclosure.FLOOR_T + enclosure.SPK_STANDOFF) * Rot(X=180) * spk.speaker()
    return parts


if __name__ == "__main__":
    for k, v in assemble().items():
        print(k, round(v.volume, 1), v.bounding_box().min, v.bounding_box().max)
