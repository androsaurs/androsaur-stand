import numpy as np, itertools
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from build123d import Pos, Box, Compound
from pantilt import base, assemble
import render

def vol(r):
    if r is None: return 0.0
    return r.volume if hasattr(r, "volume") else sum(i.volume for i in r)

for pan in (45, 90, 180):
    p = assemble(tilt_deg=60, pan_deg=pan)
    bad = [(a, b, round(vol(x.intersect(y)), 1)) for (a, x), (b, y) in itertools.combinations(p.items(), 2) if vol(x.intersect(y)) > 0.01]
    print("pan", pan, bad or "no intersections")

b = base()
cut = b.intersect(Pos(50, 10.5, 40) * Box(120, 21, 100))
if not hasattr(cut, "tessellate"): cut = Compound(children=list(cut))
fig = plt.figure(figsize=(14, 7), dpi=100)
for i, (sh, title, el, az) in enumerate([(b, "leg from +X (phone side), two cable windows", 8, 0), (cut, "section through the axis (cut face, seen from +Y)", 8, 90)], 1):
    ax = fig.add_subplot(1, 2, i, projection="3d", proj_type="ortho")
    v, f = sh.tessellate(0.05, 0.2); v = np.array([(q.X, q.Y, q.Z) for q in v]); T = v[np.array(f)]
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-12
    L = np.array([.4, -.5, .8]); L /= np.linalg.norm(L); k = .4 + .6 * np.abs(n @ L)
    ax.add_collection3d(Poly3DCollection(T, facecolors=np.outer(k, [.8, .76, .62]), edgecolor=(0, 0, 0, .1), linewidths=.1))
    ax.set_xlim(-5, 90); ax.set_ylim(-5, 90); ax.set_zlim(-5, 90); ax.set_box_aspect((1, 1, 1)); ax.view_init(el, az); ax.set_axis_off(); ax.set_title(title)
fig.savefig("out/leg.png", bbox_inches="tight")
print("window z-centres:", [round(4 + 59 * f, 2) for *_, f in __import__("pantilt").WINDOWS])
