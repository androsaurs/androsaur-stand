"""Export the printed parts of the pan-tilt mechanism and the phone holder as STL, one file per part, ready for a slicer.

    .venv/bin/python export_stl.py [--out dir]   ->  ../exports/stl/<part>.stl

Every part is saved in its print orientation (the flat face on the bed, see ../docs/printing.md) and moved so that its lowest
point is at Z = 0 and its corner at X = Y = 0. The report shows, for each part, the bed contact, the overhang area (faces steeper
than 45 degrees that are not on the bed), and re-reads the written STL: is it closed (watertight) and does its volume match the CAD volume.
"""
import math
import sys
from pathlib import Path

from build123d import Pos, export_stl

from pantilt import print_parts

out = Path(__file__).resolve().parent.parent / "exports" / "stl"
if "--out" in sys.argv:
    out = Path(sys.argv[sys.argv.index("--out") + 1])
out.mkdir(parents=True, exist_ok=True)


def stl_check(path):
    """Reads the written binary STL back: is every edge shared by exactly two triangles (closed, watertight) and the signed volume."""
    import struct
    data = path.read_bytes()
    n = struct.unpack_from("<I", data, 80)[0]
    edges, vol = {}, 0.0
    weld = lambda v: tuple(round(c, 4) for c in v)
    for i in range(n):
        f = struct.unpack_from("<12fH", data, 84 + 50 * i)
        t = [weld(f[3:6]), weld(f[6:9]), weld(f[9:12])]
        for e in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            key = tuple(sorted(e))
            edges[key] = edges.get(key, 0) + 1
        a, b, c = f[3:6], f[6:9], f[9:12]
        vol += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6
    return all(count == 2 for count in edges.values()), vol


def report(shape):
    """Bed contact area and overhang area (faces steeper than 45 degrees that are not on the bed) from the CAD mesh."""
    zmin = shape.bounding_box().min.Z
    verts, tris = shape.tessellate(0.05, 0.3)
    bed = over = 0.0
    for a, b, c in tris:
        p, q, r = verts[a], verts[b], verts[c]
        u = (q.X - p.X, q.Y - p.Y, q.Z - p.Z)
        v = (r.X - p.X, r.Y - p.Y, r.Z - p.Z)
        n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        ln = math.sqrt(sum(x * x for x in n))
        if ln:
            nz = n[2] / ln
            if nz < -0.999 and (p.Z + q.Z + r.Z) / 3 - zmin < 0.05:
                bed += ln / 2
            elif nz < -0.7071:
                over += ln / 2
    return round(bed), round(over)


print(f"{'part':12s} {'size X×Y×Z, mm':>20s} {'volume':>10s} {'bed':>8s} {'overhang':>10s}  closed   STL volume")
for name, shape in print_parts().items():
    bb = shape.bounding_box()
    shape = Pos(-bb.min.X, -bb.min.Y, -bb.min.Z) * shape
    path = out / f"{name}.stl"
    export_stl(shape, str(path), tolerance=0.01, angular_tolerance=0.1)
    bed, over = report(shape)
    closed, stl_vol = stl_check(path)
    print(f"{name:12s} {bb.size.X:6.1f} × {bb.size.Y:5.1f} × {bb.size.Z:5.1f} {shape.volume / 1000:7.1f} cm3 {bed:5d} mm2 {over:6d} mm2  {'yes' if closed else 'NO':6s} {stl_vol / 1000:7.1f} cm3 ({path.stat().st_size // 1024} KB)")
print("->", out)
