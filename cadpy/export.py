"""Export the whole assembly as a coloured, named STEP file that FreeCAD (and any CAD) opens.

    .venv/bin/python export.py [tilt] [pan] [--glass] [--hw] [--out file.step]   ->  ../exports/androsaur-stand-assembly.step

Each part keeps its name and colour. The pose is the viewer's default (tilt 90, pan 0) unless given.
The parts are grouped like in the viewer (stand / holder / enclosure / electronics, plus fasteners with --hw).
All parts are opaque by default; with --glass the see-through parts (phone, both box halves, shelf) keep their transparency.
"""
import sys
from pathlib import Path

from build123d import Compound, export_step

from pantilt import assemble
from scene import build

glass = "--glass" in sys.argv
with_hw = "--hw" in sys.argv
args = list(sys.argv[1:])
out = Path(__file__).resolve().parent.parent / "exports" / "androsaur-stand-assembly.step"
if "--out" in args:
    i = args.index("--out")
    out = Path(args[i + 1])
    del args[i:i + 2]
args = [a for a in args if not a.startswith("--")]
tilt = float(args[0]) if len(args) > 0 else 90.0
pan = float(args[1]) if len(args) > 1 else 0.0
out.parent.mkdir(exist_ok=True, parents=True)

tops = build(assemble(tilt_deg=tilt, pan_deg=pan, cable=True, hw=with_hw), glass=glass)
export_step(Compound(children=tops, label="androsaur-stand"), str(out))
print(out, round(out.stat().st_size / 1e6, 2), "MB, groups:", [t.label for t in tops])
