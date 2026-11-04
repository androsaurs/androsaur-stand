"""ESP32-S3 DevKitC-1 style dev board envelope (owner-measured 28 x 57, antenna reaches 64).

Frame: X along the length (USB end at 0, antenna end at +X), Y across (centred), Z up from the bottom
face of the PCB (components on +Z, header pins hang down to -PIN_BELOW). Units: mm.
Heights/pin lengths are estimates read off the photo; only 28 / 57 / 64 are measured.
"""
from build123d import *

PCB_L, PCB_W, PCB_T = 57.0, 28.0, 1.6
TOTAL_L = 64.0                    # including the antenna that sticks out past the PCB
ANT_W = 18.0                      # WROOM-1 module width (antenna PCB past the board edge)
SHIELD_L, SHIELD_W, SHIELD_H = 18.0, 18.0, 3.1
HEADER_PITCH, HEADER_N, ROW_SPACING = 2.54, 22, 25.4
HEADER_H = 2.5                    # yellow plastic under the PCB
PIN, PIN_BELOW, PIN_ABOVE = 0.64, 11.0, 2.5
USB_W, USB_L, USB_H, USB_OVER = 9.0, 7.3, 3.2, 0.8
USB_Y = 6.5                       # centre offset of each of the two USB-C ports


def _box(x0, x1, y0, y1, z0, z1):
    return Pos((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2) * Box(x1 - x0, y1 - y0, z1 - z0)


def board():
    b = _box(0, PCB_L, -PCB_W / 2, PCB_W / 2, 0, PCB_T)
    b += _box(PCB_L, TOTAL_L, -ANT_W / 2, ANT_W / 2, 0, PCB_T)                      # antenna past the edge
    b += _box(PCB_L - SHIELD_L - 0.5, PCB_L - 0.5, -SHIELD_W / 2, SHIELD_W / 2, PCB_T, PCB_T + SHIELD_H)
    for sy in (-1, 1):
        span = HEADER_N * HEADER_PITCH
        x0 = (PCB_L - span) / 2
        b += _box(x0, x0 + span, sy * ROW_SPACING / 2 - 1.27, sy * ROW_SPACING / 2 + 1.27, -HEADER_H, 0)
        for i in range(HEADER_N):
            x = x0 + HEADER_PITCH / 2 + i * HEADER_PITCH
            b += _box(x - PIN / 2, x + PIN / 2, sy * ROW_SPACING / 2 - PIN / 2, sy * ROW_SPACING / 2 + PIN / 2,
                      -PIN_BELOW, PCB_T + PIN_ABOVE)
        b += _box(-USB_OVER, USB_L - USB_OVER, sy * USB_Y - USB_W / 2, sy * USB_Y + USB_W / 2, PCB_T, PCB_T + USB_H)
    return b
