"""Two-piece phone holder (spine "Body" + U-clip), rebuilt as parametric BRep. The slot is sized from PHONE_* below (the original
clip was for an 82 mm wide phone; the owner's phone is 76.3 mm).

Dimensions were read off the owner-supplied STL files in
references/phone-holder-two-piece (author and licence unknown). Placement of the
two parts relative to each other and to the phone was taken from the owner's
FreeCAD assembly of the two pieces (removed from the tree, see git commit 9d254f3).

Assembly frame: Z up along the phone height from the lower stop, X across the
phone, Y forward (towards the phone). Units: mm.
"""
from build123d import *

# ---- spine (Body), built in its own frame: x long, y depth, z width -------
BODY_L = 105.6          # length along the phone height
BODY_W = 30.0           # width across the phone
PLATE_Y0, PLATE_Y1 = 8.5, 15.5      # plate (7 mm) between the rear ribs and the phone slot

# ---- phone gauge: Xiaomi Redmi 12 (23053RN02Y), no case ---------------------
# Height and width: GSMArena 168.6 x 76.3 mm (owner measured the width 76.3). Thickness 9.0 is the owner's measurement
# (the spec says 8.2): the slot follows the measured value, so it is safe for both.
PHONE_H, PHONE_W, PHONE_T = 168.6, 76.3, 9.0
FIT_SIDE = 0.5          # play on each side across the phone (FDM prints slots 0.2-0.3 mm narrower: a slide-in fit)
FIT_THICK = 0.8         # total play in the thickness (0.4 on each side of the phone)
SLOT_T = PHONE_T + FIT_THICK                    # clear gap between the plate and the lips
SLOT_SHIFT = SLOT_T - 9.0                       # the original clip had a 9.0 mm gap: the lip geometry is moved by this much
LIP_Y0, LIP_Y1 = PLATE_Y1 + SLOT_T, PLATE_Y1 + SLOT_T + 4.0         # lip closing the slot over the phone's lower edge
SLOT_END_X = 7.0                    # end wall: the phone's lower edge rests here
LIP_X1 = 15.0
WINDOW_X0, WINDOW_PITCH, WINDOW_W, WINDOW_N = 9.04, 10.0, 5.0, 6
WINDOW_Z0, WINDOW_Z1 = 6.0, 24.0
GUIDE_FIN_W = 1.5
GUIDE_A_X0 = 65.0                   # first rear guide; its inner face is at 66.5
GUIDE_B_X1 = 78.35                  # second rear guide; its inner face is at 76.85
GUIDE_GAP = 10.35                   # clear gap between the guides (clip bar is 10)

# ---- clip (82mm) -----------------------------------------------------------
GRIP_T = 10.0
GRIP_W = 88.0
GRIP_H = 23.6 + SLOT_SHIFT
GRIP_INNER_W = PHONE_W + 2 * FIT_SIDE   # clear width between the side walls (was 82.6)
GRIP_WALL = (GRIP_W - GRIP_INNER_W) / 2 # side wall thickness
GRIP_PLACE = (-44.0, 3.5, 66.6771)  # (X, Y, Z) of its min corner in the assembly frame
BODY_PLACE_X = 15.0                 # Body is centred across: X in [-15, 15]

PHONE_BOTTOM_Z = 7.0
PHONE_BACK_Y = PLATE_Y1 + FIT_THICK / 2     # the phone sits in the middle of the slot's thickness


# ---- Type-C plug and pseudo cable (phone port at the middle of the lower edge) ----
PORT_W, PORT_T, PORT_DEPTH = 8.9, 3.1, 7.0          # port cut-out in the phone gauge
SHELL_W, SHELL_T, SHELL_L = 8.4, 2.6, 6.5            # plug metal shell, goes into the phone
OVERMOLD_W, OVERMOLD_T, OVERMOLD_L = 12.0, 6.5, 16.0  # plug housing, passes through the opening in the end wall
CABLE_D = 3.6
CABLE_STRAIGHT = 10.0   # straight part behind the plug before the bend
CABLE_BEND_R = 15.0     # about 4 x cable diameter
CABLE_TAIL = 30.0       # straight part after the bend, heading back towards the leg


def _prism_xy(pts, z0, z1):
    f = Face(Wire.make_polygon([Vector(x, y, 0) for x, y in pts], close=True))
    return Pos(0, 0, z0) * Solid.extrude(f, Vector(0, 0, z1 - z0))


def _prism_x(pts_yz, x0, x1):
    f = Face(Wire.make_polygon([Vector(0, y, z) for y, z in pts_yz], close=True))
    return Pos(x0, 0, 0) * Solid.extrude(f, Vector(x1 - x0, 0, 0))


def _box(x0, x1, y0, y1, z0, z1):
    return Pos((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2) * Box(x1 - x0, y1 - y0, z1 - z0)


MOUNT_HOLE_D, MOUNT_CBORE_D, MOUNT_CBORE_DEPTH = 3.4, 6.4, 3.0   # M3 clearance, head recess on the phone side


def _hole_y(r, y0, y1, x, z):
    return Pos(x, (y0 + y1) / 2, z) * Rot(X=90) * Cylinder(r, y1 - y0)


def body_local(length=BODY_L, holes=()):
    """holes: (x, z) positions in the body frame (x along the length, z across)."""
    w = BODY_W
    # plate + end wall + lip as one profile extruded over the full width
    b = _prism_xy([
        (4.0, PLATE_Y0), (length, PLATE_Y0), (length, PLATE_Y1), (SLOT_END_X, PLATE_Y1),
        (SLOT_END_X, LIP_Y0), (LIP_X1, LIP_Y0), (LIP_X1, LIP_Y1), (4.12, LIP_Y1),
        (0.0, LIP_Y1 - 4.12), (0.0, 12.5),
    ], 0, w)
    # six windows in the plate
    for i in range(WINDOW_N):
        x0 = WINDOW_X0 + i * WINDOW_PITCH
        b -= _box(x0, x0 + WINDOW_W, PLATE_Y0 - 1, PLATE_Y1 + 1, WINDOW_Z0, WINDOW_Z1)
    # opening in the end wall (octagon, 45 degree corners)
    b -= _prism_x([(15.5, 9.5), (17.0, 8.0), (LIP_Y0 - 2.0, 8.0), (LIP_Y0, 10.0),
                   (LIP_Y0, 20.0), (LIP_Y0 - 2.0, 22.0), (17.0, 22.0), (15.5, 20.5)], -1, SLOT_END_X)
    # back is flat except for two snap guides with barbs that hold the clip bar
    gap_a = GUIDE_A_X0 + GUIDE_FIN_W
    b += _prism_xy([(GUIDE_A_X0, PLATE_Y0), (GUIDE_A_X0, 2.0), (66.76, 2.0), (67.61, 3.2),
                    (gap_a, 3.2), (gap_a, PLATE_Y0)], 0, w)
    gap_b = gap_a + GUIDE_GAP
    b += _prism_xy([(GUIDE_B_X1, PLATE_Y0), (GUIDE_B_X1, 2.0), (76.6, 2.0), (75.74, 3.2),
                    (gap_b, 3.2), (gap_b, PLATE_Y0)], 0, w)
    for x, z in holes:
        b -= _hole_y(MOUNT_HOLE_D / 2, PLATE_Y0 - 1, PLATE_Y1 + 1, x, z)
        b -= _hole_y(MOUNT_CBORE_D / 2, PLATE_Y1 - MOUNT_CBORE_DEPTH, PLATE_Y1 + 1, x, z)
    return b


def body(length=BODY_L, holes=()):
    """Body in the assembly frame (X across, Y forward, Z along the phone)."""
    return Pos(BODY_PLACE_X, 0, 0) * Rot(Y=-90) * body_local(length, holes)


def grip():
    """U-clip in the assembly frame."""
    d, x_in = SLOT_SHIFT, GRIP_WALL
    half = [(0, 6.76), (0, 21.6 + d), (2.0, 23.6 + d), (18.5, 23.6 + d), (18.5, 21.1 + d), (x_in, 21.1 + d),
            (x_in, 9.05), (18.53, 5.0), (22.08, 5.0), (28.28, 11.2), (28.28, 5.0)]
    bottom = [(GRIP_W - 1.94, 4.26), (GRIP_W - 18.53, 0.0), (18.53, 0.0), (1.94, 4.26)]
    pts = half + [(GRIP_W - x, y) for x, y in reversed(half)] + bottom
    g = _prism_xy(pts, 0, GRIP_T)
    return Pos(*GRIP_PLACE) * g


PORT_Y = PHONE_BACK_Y + PHONE_T / 2


def phone():
    p = _box(-PHONE_W / 2, PHONE_W / 2, PHONE_BACK_Y, PHONE_BACK_Y + PHONE_T,
             PHONE_BOTTOM_Z, PHONE_BOTTOM_Z + PHONE_H)
    p -= _box(-PORT_W / 2, PORT_W / 2, PORT_Y - PORT_T / 2, PORT_Y + PORT_T / 2,
              PHONE_BOTTOM_Z - 1, PHONE_BOTTOM_Z + PORT_DEPTH)
    return p


def plug():
    """Shell inside the port, housing below the phone's lower edge."""
    z0 = PHONE_BOTTOM_Z
    s = _box(-SHELL_W / 2, SHELL_W / 2, PORT_Y - SHELL_T / 2, PORT_Y + SHELL_T / 2, z0, z0 + SHELL_L)
    s += _box(-OVERMOLD_W / 2, OVERMOLD_W / 2, PORT_Y - OVERMOLD_T / 2, PORT_Y + OVERMOLD_T / 2, z0 - OVERMOLD_L, z0)
    return s


def cable():
    """Rigid stand-in for the cable: straight, 90 degree bend towards -Y, straight tail."""
    r = CABLE_D / 2
    zs = PHONE_BOTTOM_Z - OVERMOLD_L                    # plug end
    zb = zs - CABLE_STRAIGHT                            # start of the bend
    c = Pos(0, PORT_Y, zb) * Pos(0, 0, CABLE_STRAIGHT / 2) * Cylinder(r, CABLE_STRAIGHT)
    prof = Face(Wire([Edge.make_circle(r, Plane(origin=(0, PORT_Y, zb), z_dir=(0, 0, 1)))]))
    c += Solid.revolve(prof, -90, Axis((0, PORT_Y - CABLE_BEND_R, zb), (1, 0, 0)))
    c += Pos(0, PORT_Y - CABLE_BEND_R - CABLE_TAIL / 2, zb - CABLE_BEND_R) * Rot(X=90) * Cylinder(r, CABLE_TAIL)
    return c
