import itertools, sys
from pantilt import assemble

def vol(a, b):
    try:
        r = a.intersect(b)
        if r is None: return 0.0
        if hasattr(r, 'volume'): return r.volume
        return sum(x.volume for x in r)
    except Exception as e:
        return float("nan")

for tilt in [float(a) for a in sys.argv[1:]] or [0, 30, 60]:
    parts = assemble(tilt_deg=tilt)
    bad = []
    for (n1, a), (n2, b) in itertools.combinations(parts.items(), 2):
        v = vol(a, b)
        if v > 0.01 or v != v:
            bad.append((n1, n2, round(v, 2)))
    print(f"tilt {tilt:>5}: ", bad or "no intersections")
