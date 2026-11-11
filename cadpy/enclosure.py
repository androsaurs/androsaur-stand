"""Draft two-piece electronics enclosure split in half horizontally.

lower(): flat floor + walls up to the seam, corner posts with through holes,
         screws go in from the BOTTOM (counterbored in the floor).
upper(): ceiling + walls down to the seam, tongue ring that enters the lower walls,
         corner posts with M3 pilots at the seam, stand mounts on the ceiling.

Built centred on X/Y, floor bottom at z=0, top at BOX_H. Seam at BOX_H/2.
Print: lower floor-down; upper ceiling-down (flip it). No supports needed.
Units: mm. Every number here is a draft default to experiment with.
"""
from build123d import *

BOX_L = BOX_W = 125.0
BOX_H = 70.0            # total height
SEAM_Z = BOX_H / 2      # split plane
WALL = 3.0
FLOOR_T = 4.0
TOP_T = 3.0             # roof skin; local stand bosses provide the screw engagement
STAND_BOSS_D = 9.0      # same diameter as the enclosure corner posts
STAND_WASHER_D, STAND_WASHER_T = 18.0, 3.0
STAND_WASHER_HOLE_D = 3.4
STAND_WASHER_RELIEF_D, STAND_WASHER_RELIEF_DEPTH = STAND_BOSS_D + 0.8, 0.5
STAND_SCREW_L = 8.0    # 3 mm washer + 5 mm engagement; 1 mm clear of the blind pilot bottom
CORNER_R = 6.0
FIT = 0.3               # clearance between the tongue and the lower walls
POST_D = 9.0
PILOT_D, PILOT_DEPTH = 2.6, 10.0      # M3 self-tapping pilot in the upper posts
HOLE_D, CBORE_D, CBORE_DEPTH = 3.4, 6.4, 2.5   # bottom screw holes (clearance + head recess)
JOIN_SCREW_L, JOIN_ENGAGE = 12.0, 8.0                      # half-joining screws are M3x12; the tip goes 8 mm into the upper post pilot
POST_CBORE_DEPTH = SEAM_Z + JOIN_ENGAGE - JOIN_SCREW_L     # tunnel up the lower post: the head seats at z = 31, so a short screw reaches
LIP_H, LIP_T = 4.0, 2.0               # tongue ring
LEDGE_H = 2.0                         # inward ledge at the seam that the tongue ring hangs from
LEG_D = 36.4
OPENING_D = LEG_D + 0.6  # ceiling hole the leg passes through (the base plate hangs under the ceiling)
STAND_PILOT_D, STAND_PILOT_DEPTH = 2.6, 6.0   # blind pilots from below for the stand's base-plate screws

import speaker as _spk
SPK_X, SPK_Y = 12.5, -12.5       # speaker tucked into the front/-Y corner of the box (flange 1 mm from both walls)
SPK_STANDOFF = 5.0               # speaker flange stands this high above the floor (sound path to the grille)
SPK_POST_D, SPK_PILOT_D, SPK_PILOT_DEPTH = 9.0, 3.4, 6.0   # posts under the flange holes, M4 self-tapping pilots
SPK_RING_T = 3.0                                            # ring wall thickness
SPK_RING_R_IN = _spk.HOLE_PCD / 2 - SPK_RING_T / 2          # centre line of the ring on the bolt circle of the speaker
SPK_RING_R_OUT = _spk.HOLE_PCD / 2 + SPK_RING_T / 2         # ring between the floor and the flange: the sound can only leave through the floor grille
GRILLE_R, SLOT_W, SLOT_PITCH = 34.0, 3.0, 6.0               # floor grille under the speaker
FOOT_D, FOOT_H = 14.0, 8.0       # separate feet, screwed to the floor from inside with M3
FOOT_POS = tuple((sx * 47.0, sy * 47.0) for sx in (-1, 1) for sy in (-1, 1))   # pad edges at +-54: 8.5 mm in from the box edge, clear of the corner-post screw recesses
FOOT_HOLE_D, FOOT_PILOT_D, FOOT_PILOT_DEPTH = 3.4, 2.6, 6.0

# ---- shelf inside the lower half: plate on 4 legs, hole for the speaker magnet ----
import esp32 as _esp
SHELF_T = 3.0
SHELF_HALF = 56.5                # inside the lid tongue ring (inner half-size 57.2)
SHELF_DROP = 1.5                               # how far the plate is lowered below the point where the magnet starts (limit: caps of the amp hanging under it meet the speaker flange)
_Z_FLANGE_TOP = FLOOR_T + SPK_STANDOFF + _spk.FLANGE_T
_Z_MAGNET = FLOOR_T + SPK_STANDOFF + _spk.TOTAL_H - _spk.MAGNET_H
SHELF_Z = _Z_MAGNET - SHELF_DROP         # plate underside
_r_basket = _spk.MAGNET_D / 2 + max(0.0, min(1.0, (_Z_MAGNET - SHELF_Z) / (_Z_MAGNET - _Z_FLANGE_TOP))) * (_spk.BASKET_R - _spk.MAGNET_D / 2)
SHELF_HOLE_D = 2 * (_r_basket + 1.5)     # hole clears the conical basket at the plate's underside by 1.5 mm
SHELF_LEGS = ((50.0, 34.0, 8.0), (-30.0, -45.0, 8.0), (-38.5, 0.0, 8.0))   # (x, y, diameter): inside the plate, clear of the speaker flange, the amplifier and the rear +Y corner (kept free for the power module)
SHELF_PILOT_D, SHELF_PILOT_DEPTH = 2.6, 8.0
import amp as _amp
# ESP32 lies ON TOP of the shelf (flat top), pins through slots; antenna end towards the -Y wall, USB end towards the middle
ESP_X = -36.0
ESP_Y_USB = -56.5 + _esp.TOTAL_L          # Y of the USB end (high Y); the antenna end at -56.5 stays inside the lid tongue ring (inner face at 57.2)
# amplifier hangs UNDER the shelf on posts (components down), rear-left zone
TPA_X, TPA_Y0 = -14.25, 39.0      # module local x=0 end and the world Y of its centre line; rotated: 46.5 along X, 35 along Y
TPA_STANDOFF, TPA_POST_D, TPA_PILOT_D, TPA_PILOT_DEPTH = 3.0, 6.0, 2.6, 5.0
TPA_FACE_Z = SHELF_Z - TPA_STANDOFF       # module bare (solder) face

# DAC (PCM5102A) is hot-glued flat to the UNDERSIDE of the shelf in the rear +Y corner: long side along Y, components down
DAC_X, DAC_Y0 = -36.0, 14.0     # centre X, module local x=0 end
DAC_FACE_Z = SHELF_Z            # bare face of the board touches the shelf underside

# HW-674 buck module (60 x 38 x 26) on the shelf top, beyond the ESP32 along +X, on 3 mm printed spacers
PSU_SPACER_H, PSU_SPACER_D = 3.0, 7.0                 # separate printed bushings: the shelf top stays flat
PSU_POST_D, PSU_POST_H, PSU_PILOT_D = 5.5, 2.5, 2.6    # posts hang under the shelf at the module holes (screws from the top, M3x8)
PSU_X0, PSU_Y0 = -21.0, 18.0    # min corner; X -21..39 starts 1 mm beyond the far (+X) face of the ESP32 (x = -22); Y 18..56 stops 0.5 mm short of the shelf edge (the lid tongue ring is at 57.2)
PSU_Z0 = SHELF_Z + SHELF_T


def psu_holes():
    """(x, y) of the module's four mounting holes in the enclosure frame."""
    return [(PSU_X0 + hx, PSU_Y0 + hy) for hx, hy in _amp.PSU_HOLES]


def tpa_holes():
    """(x, y) of the amplifier's mounting holes in the enclosure frame (module hung flipped and rotated: local x -> +X, local y -> -Y)."""
    return [(TPA_X + lx, TPA_Y0 - ly) for lx, ly in _amp.TPA_HOLES]
ESP_SLOT_W = 1.8                 # header pins pass through; the 2.54 wide header bodies rest on the shelf

# ---- DC inlet (barrel jack 5.5 x 2.1, panel mount) and vent slots in the rear (-X) walls ----
JACK_HOLE_D = 11.0              # typical panel-mount DC jack body (threaded barrel)
JACK_Y, JACK_Z = 0.0, 13.0      # centred on the rear wall; vents are symmetric around it
VENT_W, VENT_PITCH = 3.0, 7.0
VENT_Y_MAX = 44.0               # slots span Y -42..42 (clear of the corner posts)
LOWER_VENT_Z = (7.0, 25.0)       # entirely below the shelf (its underside is at z 27.5), above the floor (top at z 4)
UPPER_VENT_Z = (40.0, 56.0)     # above the tongue ring, below the stand plate
JACK_CLEAR_Y = 8.0              # no slots within +-8 of the jack axis


# slots in the shelf at the FRONT (+X) edge, opposite the rear vents: air enters low at the back, passes under the shelf
# (amplifier), rises through these slots and leaves through the upper rear vents (over the power module)
SHELF_VENT_X = (46.5, 50.5, 54.0)
SHELF_VENT_W, SHELF_VENT_L = 2.5, 10.0
SHELF_VENT_Y = (-19.5, -6.5, 6.5, 19.5)   # centred on Y = 0; clear of the leg at (50, 34)


def _vent_ys():
    n = int(VENT_Y_MAX // VENT_PITCH)
    return [k * VENT_PITCH for k in range(-n, n + 1)]


def _vents(z0, z1, skip_jack):
    cut = None
    for y in _vent_ys():
        if skip_jack and abs(y - JACK_Y) < JACK_CLEAR_Y:
            continue
        b = Pos(-BOX_L / 2 + WALL / 2, y, (z0 + z1) / 2) * Box(WALL + 2, VENT_W, z1 - z0)
        cut = b if cut is None else cut + b
    return cut


POST_C = BOX_L / 2 - WALL - POST_D / 2
POSTS = tuple((sx * POST_C, sy * POST_C) for sx in (-1, 1) for sy in (-1, 1))


def _rounded(l, w, h, r, z0=0.0):
    b = Pos(0, 0, z0 + h / 2) * Box(l, w, h)
    return fillet(b.edges().filter_by(Axis.Z), r)


def _cyl(r, h, x, y, z0):
    return Pos(x, y, z0) * Cylinder(r, h, align=(Align.CENTER, Align.CENTER, Align.MIN))


def lower():
    t = _rounded(BOX_L, BOX_W, SEAM_Z, CORNER_R)
    t -= _rounded(BOX_L - 2 * WALL, BOX_W - 2 * WALL, SEAM_Z, CORNER_R - WALL, z0=FLOOR_T)
    for x, y in POSTS:
        t += _cyl(POST_D / 2, SEAM_Z - FLOOR_T, x, y, FLOOR_T)
        t -= _cyl(HOLE_D / 2, SEAM_Z + 2, x, y, -1)
        t -= _cyl(CBORE_D / 2, POST_CBORE_DEPTH + 1, x, y, -1)
    ring = _cyl(SPK_RING_R_OUT, SPK_STANDOFF + 1, SPK_X, SPK_Y, FLOOR_T - 1) - _cyl(SPK_RING_R_IN, SPK_STANDOFF + 3, SPK_X, SPK_Y, FLOOR_T - 2)
    t += ring                                        # added before the posts so their pilots stay open (tape or foam gasket on top)
    for hx, y in _spk.hole_xy():                     # standoffs for the speaker bolts
        x, y = hx + SPK_X, y + SPK_Y
        t += _cyl(SPK_POST_D / 2, SPK_STANDOFF + 1, x, y, FLOOR_T - 1)
        t -= _cyl(SPK_PILOT_D / 2, SPK_PILOT_DEPTH, x, y, FLOOR_T + SPK_STANDOFF - SPK_PILOT_DEPTH)
    disc = _cyl(GRILLE_R, FLOOR_T + 2, SPK_X, SPK_Y, -1)     # slot grille clipped to a circle
    k = 0
    while k * SLOT_PITCH + SLOT_W / 2 < GRILLE_R - 1:
        for sgn in ((1,) if k == 0 else (1, -1)):
            slot = Pos(SPK_X, SPK_Y + sgn * k * SLOT_PITCH, FLOOR_T / 2) * Box(2 * GRILLE_R, SLOT_W, FLOOR_T + 2)
            t -= slot.intersect(disc)
        k += 1
    for x, y in FOOT_POS:
        t -= _cyl(FOOT_HOLE_D / 2, FLOOR_T + 2, x, y, -1)
        t -= _cyl(CBORE_D / 2, CBORE_DEPTH + 1, x, y, FLOOR_T - CBORE_DEPTH)   # head recess on the inside: M3x6 reaches the foot
    t -= Pos(-BOX_L / 2 + WALL / 2, JACK_Y, JACK_Z) * Rot(Y=90) * Cylinder(JACK_HOLE_D / 2, WALL + 2)   # DC jack hole in the rear wall
    t -= _vents(*LOWER_VENT_Z, skip_jack=True)
    for x, y, _d in SHELF_LEGS:                      # screws up into the shelf legs
        t -= _cyl(HOLE_D / 2, FLOOR_T + 2, x, y, -1)
        t -= _cyl(CBORE_D / 2, CBORE_DEPTH + 1, x, y, -1)
    return t


def shelf():
    """Flat top. Print plate-down, legs/posts up. Legs stand on the floor, M3 screws from below."""
    s = _rounded(2 * SHELF_HALF, 2 * SHELF_HALF, SHELF_T, 5.0, z0=SHELF_Z)
    for x, y in POSTS:
        s -= _cyl(POST_D / 2 + 1.5, SHELF_T + 2, x, y, SHELF_Z - 1)
    s -= _cyl(SHELF_HOLE_D / 2, SHELF_T + 2, SPK_X, SPK_Y, SHELF_Z - 1)
    span = _esp.HEADER_N * _esp.HEADER_PITCH
    yc = ESP_Y_USB - _esp.PCB_L / 2
    for sx in (-1, 1):
        s -= Pos(ESP_X + sx * _esp.ROW_SPACING / 2, yc, SHELF_Z + SHELF_T / 2) * Box(ESP_SLOT_W, span + 1.5, SHELF_T + 2)
    for x, y in tpa_holes():                         # amplifier posts hang under the shelf (screws from below)
        s += _cyl(TPA_POST_D / 2, TPA_STANDOFF + 0.5, x, y, TPA_FACE_Z)
        s -= _cyl(TPA_PILOT_D / 2, TPA_PILOT_DEPTH + 1, x, y, TPA_FACE_Z - 1)
    for vx in SHELF_VENT_X:                          # airflow slots
        for vy in SHELF_VENT_Y:
            s -= Pos(vx, vy, SHELF_Z + SHELF_T / 2) * Box(SHELF_VENT_W, SHELF_VENT_L, SHELF_T + 2)
    for x, y in psu_holes():                         # power module: posts under the shelf, pilots open at the flat top
        s += _cyl(PSU_POST_D / 2, PSU_POST_H + 0.5, x, y, SHELF_Z - PSU_POST_H)
        s -= _cyl(PSU_PILOT_D / 2, SHELF_T + PSU_POST_H - 0.5 + 1, x, y, SHELF_Z - PSU_POST_H + 0.5)
    for x, y, d in SHELF_LEGS:
        s += _cyl(d / 2, SHELF_Z - FLOOR_T + 0.5, x, y, FLOOR_T)
        s -= _cyl(SHELF_PILOT_D / 2, SHELF_PILOT_DEPTH + 1, x, y, FLOOR_T - 1)
    return s



def psu_spacer():
    """Printed bushing under the power module, origin at its bottom, 3 mm tall."""
    return _cyl(PSU_SPACER_D / 2, PSU_SPACER_H, 0, 0, 0) - _cyl(1.7, PSU_SPACER_H + 2, 0, 0, -1)


def foot():
    """Separate foot, origin at its top face (z=0), body hangs down to -FOOT_H. Screwed from inside the box."""
    f = Pos(0, 0, -FOOT_H) * Cylinder(FOOT_D / 2, FOOT_H, align=(Align.CENTER, Align.CENTER, Align.MIN))
    f = chamfer(f.edges().filter_by_position(Axis.Z, -FOOT_H - 0.01, -FOOT_H + 0.01), 0.6)
    f -= _cyl(FOOT_PILOT_D / 2, FOOT_PILOT_DEPTH + 1, 0, 0, -FOOT_PILOT_DEPTH)
    return f


def stand_washer():
    """Printed 18 x 3 mm clamp washer; relieved face goes against the stand sole.

    The 9.8 x 0.5 mm relief clears the peg tip, so tightening clamps the sole
    instead of bottoming out on the peg. Print flat back down, relief up.
    """
    w = _cyl(STAND_WASHER_D / 2, STAND_WASHER_T, 0, 0, 0)
    w -= _cyl(STAND_WASHER_HOLE_D / 2, STAND_WASHER_T + 2, 0, 0, -1)
    w -= _cyl(STAND_WASHER_RELIEF_D / 2, STAND_WASHER_RELIEF_DEPTH + 1,
              0, 0, STAND_WASHER_T - STAND_WASHER_RELIEF_DEPTH)
    return w


def upper(stand_holes=(), stand_boss_h=4.0):
    """stand_holes: screw centres; stand_boss_h: stand sole thickness (passed by assemble)."""
    h = BOX_H - SEAM_Z
    u = _rounded(BOX_L, BOX_W, h, CORNER_R, z0=SEAM_Z)
    u -= _rounded(BOX_L - 2 * WALL, BOX_W - 2 * WALL, h - TOP_T + 1, CORNER_R - WALL, z0=SEAM_Z - 1)
    il, iw = BOX_L - 2 * WALL - 2 * FIT, BOX_W - 2 * WALL - 2 * FIT
    r_in = CORNER_R - WALL - FIT
    ring = _rounded(il, iw, LIP_H, r_in, z0=SEAM_Z - LIP_H)
    ring -= _rounded(il - 2 * LIP_T, iw - 2 * LIP_T, LIP_H, max(r_in - LIP_T, 0.5), z0=SEAM_Z - LIP_H)
    for x, y in POSTS:
        ring -= _cyl(POST_D / 2 + 0.5, LIP_H, x, y, SEAM_Z - LIP_H)
    # ledge under the walls that carries the tongue ring (without it the ring is a loose strip)
    ledge = _rounded(BOX_L - 2 * WALL, BOX_W - 2 * WALL, LEDGE_H, CORNER_R - WALL, z0=SEAM_Z)
    ledge -= _rounded(il - 2 * LIP_T, iw - 2 * LIP_T, LEDGE_H, max(r_in - LIP_T, 0.5), z0=SEAM_Z)
    u += ledge
    u += ring
    for x, y in POSTS:
        u += _cyl(POST_D / 2, h - TOP_T, x, y, SEAM_Z)
        # fill the pocket between the post and the corner: with the ledge below it that pocket was a sealed void
        cx_, cy_ = (abs(x) + 60.0) / 2 * (1 if x > 0 else -1), (abs(y) + 60.0) / 2 * (1 if y > 0 else -1)
        u += Pos(cx_, cy_, SEAM_Z + (h - TOP_T) / 2) * Box(60.0 - abs(x), 60.0 - abs(y), h - TOP_T)
        u -= _cyl(PILOT_D / 2, PILOT_DEPTH, x, y, SEAM_Z - 1)
    u -= _vents(*UPPER_VENT_Z, skip_jack=False)
    u -= _cyl(OPENING_D / 2, TOP_T + 2, 0, 0, BOX_H - TOP_T - 1)
    for x, y in stand_holes:
        boss_z = BOX_H - TOP_T - stand_boss_h
        u += _cyl(STAND_BOSS_D / 2, stand_boss_h + 0.25, x, y, boss_z)
        u -= _cyl(STAND_PILOT_D / 2, STAND_PILOT_DEPTH + 1, x, y, boss_z - 1)
    return u
