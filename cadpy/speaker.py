"""FD92-1 speaker envelope (owner-measured): round flange, magnet behind.

Frame: axis Z, front face of the flange at z=0, body extends to z=-TOTAL_H (sound goes +Z).
No mounting holes yet: the flange hole pattern was not measured. Units: mm.
"""
from build123d import *

FLANGE_D = 92.0
FLANGE_T = 2.5          # assumed
MAGNET_D = 50.0
MAGNET_H = 22.0
TOTAL_H = 42.0          # front of flange to back of magnet
CONE_D = 75.0           # diaphragm diameter (owner)
CONE_DEPTH = 9.0        # assumed depth of the cone recess
HOLE_D, HOLE_PCD, HOLE_N = 4.0, 82.0, 4   # 4 flange holes D4 (owner); bolt circle diameter is ASSUMED, measure it
HOLE_ANGLE0 = 45.0
BASKET_R = 36.0         # basket radius right under the flange; it narrows to the magnet radius


def hole_xy():
    import math
    return [(HOLE_PCD / 2 * math.cos(math.radians(HOLE_ANGLE0 + 360 * i / HOLE_N)),
             HOLE_PCD / 2 * math.sin(math.radians(HOLE_ANGLE0 + 360 * i / HOLE_N))) for i in range(HOLE_N)]


def speaker():
    r0, r1, r2 = FLANGE_D / 2, MAGNET_D / 2, CONE_D / 2
    rb = BASKET_R                              # basket under the ring: stays inside the hole circle
    pts = [(0, -TOTAL_H), (r1, -TOTAL_H), (r1, -(TOTAL_H - MAGNET_H)), (rb, -FLANGE_T),   # magnet, basket
           (r0, -FLANGE_T), (r0, 0), (r2, 0), (r2, -2.0), (0, -CONE_DEPTH)]     # flat ring r2..r0, then diaphragm
    f = Face(Wire.make_polygon([Vector(x, 0, z) for x, z in pts], close=True))
    sp = Solid.revolve(f, 360, Axis.Z)
    for x, y in hole_xy():
        sp -= Pos(x, y, -FLANGE_T - 2) * Cylinder(HOLE_D / 2, FLANGE_T + 4, align=(Align.CENTER, Align.CENTER, Align.MIN))
    return sp
