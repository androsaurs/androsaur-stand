"""Envelopes of the two audio modules (ESTIMATES: only the overall board sizes and the corner holes are real; components are laid out symmetrically).

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
TPA_CAPS = ((8.25, -7.0), (8.25, 7.0), (38.25, -7.0), (38.25, 7.0))   # four electrolytics, mirror-symmetric about both board axes
TPA_IND = ((23.25, -9.0), (23.25, 9.0))                                # two inductors above and below the chip
TPA_IND_SIZE = (10.0, 11.0, 6.5)                                     # along X, along Y, height
TPA_IC_SIZE = (12.0, 7.0, 1.8)                                      # along X, along Y, height
TPA_IC_POS = (23.25, 0.0)                                           # board centre


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


# ---- XL4005 (HW-674) buck module 60 x 38 x 26 (ESTIMATE from the seller photo; only 60 / 38 / 26 are given) ----
PSU_L, PSU_W, PSU_H = 60.0, 38.0, 26.0     # board length, board width, height of the tallest part above the board bottom
PSU_HOLE_D, PSU_HOLE_INSET = 3.2, 3.5     # four corner holes (positions estimated, measure on the real board)
PSU_HOLES = tuple((x, y) for x in (PSU_HOLE_INSET, PSU_L - PSU_HOLE_INSET) for y in (PSU_HOLE_INSET, PSU_W - PSU_HOLE_INSET))


def buck_hw674():
    """Local frame: X along the 60 side, Y along the 38 side, Z up from the bottom of the board. Components are rough boxes."""
    top = PCB_T
    b = _box(0, PSU_L, 0, PSU_W, 0, PCB_T)
    for x, y in PSU_HOLES:
        b -= Pos(x, y, PCB_T / 2) * Cylinder(PSU_HOLE_D / 2, PCB_T + 2)
    b += _box(8, 23, 25, 36, top, PSU_H)                 # heatsink 1 with a TO-220 on it, on the +Y long edge
    b += _box(24, 39, 25, 36, top, PSU_H)                # heatsink 2
    b += Pos(21, 12, top) * Cylinder(5, 16, align=(Align.CENTER, Align.CENTER, Align.MIN))   # electrolytic
    b += Pos(45, 28, top) * Cylinder(5, 16, align=(Align.CENTER, Align.CENTER, Align.MIN))   # electrolytic
    b += Pos(34, 12, top) * Cylinder(7.5, 12, align=(Align.CENTER, Align.CENTER, Align.MIN)) # toroid
    b += _box(1.5, 11.5, 7.5, 16.5, top, top + 10)       # IN terminal block (clear of the corner screw heads)
    b += _box(50.5, 59.5, 17.5, 27.5, top, top + 10)     # OUT terminal block
    b += _box(43, 51, 6, 14, top, top + 6)               # trimmer
    return b
