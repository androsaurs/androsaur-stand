"""Envelopes of the two audio modules (ESTIMATES from product photos; only the overall board sizes are given).

tpa3118(): mono class-D amplifier board 46.5 x 35, four corner holes, two inductors, four electrolytic caps.
           Frame: X along the 46.5 side, Y across (centred), Z up from the PCB bottom.
pcm5102(): I2S DAC board 32 x 17 with the 3.5 mm jack at the +X end. No mounting holes. Same frame style.
Units: mm.
"""
from build123d import *

PCB_T = 1.6
# ---- TPA3118 ----
TPA_L, TPA_W = 46.5, 35.0
TPA_HOLE_D, TPA_HOLE_INSET = 3.2, 3.5
TPA_HOLES = tuple((x, y) for x in (TPA_HOLE_INSET, TPA_L - TPA_HOLE_INSET)
                  for y in (-(TPA_W / 2 - TPA_HOLE_INSET), TPA_W / 2 - TPA_HOLE_INSET))
TPA_CAP_D, TPA_CAP_H = 8.0, 10.5
TPA_CAPS = ((5.0, -8.5), (5.0, 5.5), (33.5, -4.5), (31.5, 7.5))        # centres read off the photo (rough)
TPA_IND = ((12.0, -7.0), (30.0, -7.0))                                    # two 11 x 10 x 6.5 inductors
TPA_IND_SIZE = (10.0, 11.0, 6.5)
TPA_IC_SIZE = (7.0, 12.0, 1.8)
TPA_IC_POS = (19.0, 3.5)


def _box(x0, x1, y0, y1, z0, z1):
    return Pos((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2) * Box(x1 - x0, y1 - y0, z1 - z0)


def tpa3118():
    b = _box(0, TPA_L, -TPA_W / 2, TPA_W / 2, 0, PCB_T)
    for x, y in TPA_HOLES:
        b -= Pos(x, y, PCB_T / 2) * Cylinder(TPA_HOLE_D / 2, PCB_T + 2)
    for x, y in TPA_CAPS:
        b += Pos(x, y, PCB_T) * Cylinder(TPA_CAP_D / 2, TPA_CAP_H, align=(Align.CENTER, Align.CENTER, Align.MIN))
    for x, y in TPA_IND:
        w, d, h = TPA_IND_SIZE
        b += _box(x - w / 2, x + w / 2, y - d / 2, y + d / 2, PCB_T, PCB_T + h)
    w, d, h = TPA_IC_SIZE
    x, y = TPA_IC_POS
    b += _box(x - w / 2, x + w / 2, y - d / 2, y + d / 2, PCB_T, PCB_T + h)
    return b


# ---- PCM5102A ----
PCM_L, PCM_W = 32.0, 17.0
JACK = (12.0, 7.0, 5.5)          # along X, across Y, height; hangs 1.5 past the +X end


def pcm5102():
    b = _box(0, PCM_L, -PCM_W / 2, PCM_W / 2, 0, PCB_T)
    jl, jw, jh = JACK
    b += _box(PCM_L - jl + 1.5, PCM_L + 1.5, PCM_W / 2 - jw, PCM_W / 2, PCB_T, PCB_T + jh)
    b += _box(10.0, 20.0, -3.5, 3.5, PCB_T, PCB_T + 1.5)             # the DAC chip
    return b


# ---- XL4005 (HW-674) buck module, solid placeholder block (owner photo: 60 x 38 x 26 incl. heatsinks) ----
PSU_L, PSU_W, PSU_H = 60.0, 38.0, 26.0


def psu_block():
    """Plain cuboid, local X=60, Y=38 (the PCB plane), Z=26 (height of the components)."""
    return Pos(PSU_L / 2, PSU_W / 2, PSU_H / 2) * Box(PSU_L, PSU_W, PSU_H)
