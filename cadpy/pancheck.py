import itertools
from pantilt import assemble
def vol(r):
    if r is None: return 0.0
    return r.volume if hasattr(r, "volume") else sum(i.volume for i in r)
for tilt in (0, 45, 90):
    for pan in (45, 90, 135, 180):
        p = assemble(tilt_deg=tilt, pan_deg=pan)
        bad = [(a, b, round(vol(x.intersect(y)), 1)) for (a, x), (b, y) in itertools.combinations(p.items(), 2) if vol(x.intersect(y)) > 0.01]
        print(f"tilt {tilt:>3} pan {pan:>3}:", bad or "no intersections")
