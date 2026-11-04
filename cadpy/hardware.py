"""Simplified fasteners: socket-head screws (ISO 7380 style button head), M3 nut and washer. No threads (plain shank).

Screw frame: z = 0 is the underside of the head, +z towards the tip, the head sits at z < 0.
Units: mm. Dimensions are the nominal ones from the standard tables.
"""
import math

from build123d import *

# size: (shank d, head d, head h, hex socket across flats, socket depth)
SPEC = {"M2.5": (2.5, 5.0, 1.5, 1.5, 0.8), "M3": (3.0, 5.7, 1.65, 2.0, 1.0), "M4": (4.0, 7.6, 2.2, 2.5, 1.3)}


def screw(size, length):
    d, hd, hh, af, sd = SPEC[size]
    s = Pos(0, 0, length / 2) * Cylinder(d / 2, length)
    s = chamfer(s.edges().filter_by_position(Axis.Z, length - 0.01, length + 0.01), 0.4)
    head = Pos(0, 0, -hh / 2) * Cylinder(hd / 2, hh)
    pts = [Vector(af / math.sqrt(3) * math.cos(math.radians(60 * i + 30)), af / math.sqrt(3) * math.sin(math.radians(60 * i + 30)), 0) for i in range(6)]
    socket = Pos(0, 0, -hh) * Solid.extrude(Face(Wire.make_polygon(pts, close=True)), Vector(0, 0, sd))
    return s + (head - socket)


def washer_m3():
    """3.2 x 7 x 0.5, lies on z in [0, 0.5]."""
    return Pos(0, 0, 0.25) * (Cylinder(3.5, 0.5) - Cylinder(1.6, 0.5))


def nut_m3():
    """Hex 5.5 across flats, 2.4 thick, z in [0, 2.4]."""
    r = 5.5 / math.sqrt(3)
    pts = [Vector(r * math.cos(math.radians(60 * i)), r * math.sin(math.radians(60 * i)), 0) for i in range(6)]
    return Solid.extrude(Face(Wire.make_polygon(pts, close=True)), Vector(0, 0, 2.4)) - Cylinder(1.5, 2.4, align=(Align.CENTER, Align.CENTER, Align.MIN))


def at(x, y, z, direction=1):
    """Location with the origin (x, y, z) and the +z axis pointing up (direction=1) or down (-1)."""
    return Location(Plane(origin=(x, y, z), z_dir=(0, 0, direction)))
