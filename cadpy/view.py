"""Entry point for the live viewer: `./viewer.sh`. Edit TILT/PAN and save to redraw.

The tree in the viewer has the groups stand / holder / enclosure / electronics / fasteners (fasteners split per joint),
so any group can be switched off. Colours, opacity and grouping are defined in palette.py.
"""
from ocp_vscode import show

from pantilt import assemble
from scene import build

TILT = 90   # 0 = platform horizontal, 90 = vertical facing +X; free range about -2..100 with the holder and phone
PAN = 0

tops = build(assemble(tilt_deg=TILT, pan_deg=PAN, cable=True, hw=True))
show(*tops, names=[t.label for t in tops], reset_camera=None)
