"""Test piece for the base: only its top part with the cavity vault, cut from the real base() geometry.

    .venv/bin/python export_bridge_test.py [--cut-z 82]   ->  ../exports/test/base_top.stl

The piece is the base above Z = cut-z (default 82: just above the "motors" window, which ends at 81.75): 2 mm of the cylindrical cavity,
then from CONE_Z0 = 84 mm the conical vault narrowing to the 8.4 mm cable hole at Z = 97, then the seat and the pin holes.
It is printed upright with the cut face on the bed, same as the base. The vault replaces the former flat 30.4 mm bridge: each layer
overhangs the one below by a small step (about 40 degrees from vertical). Print it without supports to find out whether the base
can be printed without them.
"""
import sys
from pathlib import Path

from build123d import Box, Pos, export_stl

from pantilt import BASE_L, BASE_W, CAVITY_TOP, CONE_Z0, RISER, base

cut_z = float(sys.argv[sys.argv.index("--cut-z") + 1]) if "--cut-z" in sys.argv else 82.0
out = Path(__file__).resolve().parent.parent / "exports" / "test"
out.mkdir(parents=True, exist_ok=True)

keep = Pos(BASE_L / 2, BASE_W / 2, (cut_z + RISER) / 2) * Box(BASE_L + 2, BASE_W + 2, RISER - cut_z)
top = base() & keep
top = Pos(-top.bounding_box().min.X, -top.bounding_box().min.Y, -top.bounding_box().min.Z) * top
export_stl(top, str(out / "base_top.stl"), tolerance=0.01, angular_tolerance=0.1)
bb = top.bounding_box()
print(f"base_top.stl: {bb.size.X:.1f} x {bb.size.Y:.1f} x {bb.size.Z:.1f} mm, volume {top.volume / 1000:.2f} cm3, "
      f"cylinder {CONE_Z0 - cut_z:.1f} mm, then the vault (cone) {CAVITY_TOP - CONE_Z0:.1f} mm high")
