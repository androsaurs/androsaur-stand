"""Export the whole assembly as a coloured, named STEP file that FreeCAD (and any CAD) opens.

    .venv/bin/python export.py [tilt] [pan] [--glass]   ->  ../exports/androsaur-stand-assembly.step

Each part keeps its name and colour. The pose is the viewer's default (tilt 90, pan 0) unless given.
All parts are opaque by default; with --glass the viewer's see-through parts (phone, both box halves,
power module, shelf) keep their transparency, which FreeCAD then shows.
"""
import sys
from pathlib import Path

from build123d import Color, Compound, export_step

from palette import STYLE
from pantilt import assemble

glass = "--glass" in sys.argv
args = [a for a in sys.argv[1:] if not a.startswith("--")]
tilt = float(args[0]) if len(args) > 0 else 90.0
pan = float(args[1]) if len(args) > 1 else 0.0
out = Path(__file__).resolve().parent.parent / "exports" / "androsaur-stand-assembly.step"
out.parent.mkdir(exist_ok=True)

children = []
for name, shape in assemble(tilt_deg=tilt, pan_deg=pan, cable=True).items():
    hexcol, alpha = STYLE.get(name, ("#999999", 1.0))
    r, g, b = (int(hexcol[i:i + 2], 16) / 255 for i in (1, 3, 5))
    shape.label = name
    shape.color = Color(r, g, b, alpha if glass else 1.0)
    children.append(shape)

asm = Compound(children=children, label="androsaur-stand")
export_step(asm, str(out))
print(out, round(out.stat().st_size / 1e6, 2), "MB,", len(children), "parts")
