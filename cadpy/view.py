"""Entry point for the live viewer: `./viewer.sh`. Edit TILT/PAN and save to redraw.

The model is shown as two groups, "parts" (translucent) and "fasteners" (red, opaque), so either group can be switched off in the viewer tree.
"""
from build123d import Color, Compound
from ocp_vscode import show

from palette import style
from pantilt import assemble

TILT = 90   # 0 = platform horizontal, 90 = vertical facing +X; free range about -2..100 with the holder and phone
PAN = 0
PART_ALPHA = 0.35   # every part is see-through, the fasteners stay opaque

shapes = assemble(tilt_deg=TILT, pan_deg=PAN, cable=True, hw=True)
parts, fasteners = [], []
for name, shape in shapes.items():
    hexcol, _ = style(name)
    r, g, b = (int(hexcol[i:i + 2], 16) / 255 for i in (1, 3, 5))
    shape.label = name
    if name.startswith("hw_"):
        shape.color = Color(r, g, b, 1.0)
        fasteners.append(shape)
    else:
        shape.color = Color(r, g, b, PART_ALPHA)
        parts.append(shape)

show(Compound(children=parts, label="parts"), Compound(children=fasteners, label="fasteners"),
     names=["parts", "fasteners"], reset_camera=None)
