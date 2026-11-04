import sys, numpy as np
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from pantilt import assemble

COL = {"base": "#c9c3a8", "carrier": "#3a3a3f", "servo_pan": "#6f7f8f", "plate_horn": "#f2f2f2",
       "plate_shaft": "#e4e4e4", "platform": "#2d2d33", "holder": "#d9772b", "body": "#d9772b", "grip": "#e8b04a", "phone": "#4a7fb5", "plug": "#222222", "cable": "#111111", "servo_tilt": "#6f7f8f"}

def tris(parts):
    T, C = [], []
    for name, sh in parts.items():
        v, f = sh.tessellate(0.1, 0.3)
        v = np.array([(p.X, p.Y, p.Z) for p in v])
        for i, j, k in f:
            T.append(v[[i, j, k]]); C.append(COL[name])
    return np.array(T), C

def shade(T, C, light):
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
    n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-12
    k = 0.45 + 0.55 * np.abs(n @ light)
    rgb = np.array([matplotlib.colors.to_rgb(c) for c in C]) * k[:, None]
    return np.clip(rgb, 0, 1)

def render(tilt, views, out, pan=0):
    T, C = tris(assemble(tilt_deg=tilt, pan_deg=pan))
    cols = 2 if len(views) > 2 else len(views)
    rows = -(-len(views) // cols)
    fig = plt.figure(figsize=(7 * cols, 7 * rows), dpi=100)
    for idx, (title, elev, azim) in enumerate(views, 1):
        ax = fig.add_subplot(rows, cols, idx, projection="3d", proj_type="ortho")
        a = np.radians(azim); e = np.radians(elev)
        light = np.array([np.cos(e) * np.cos(a - .6), np.cos(e) * np.sin(a - .6), np.sin(e) + .4]); light /= np.linalg.norm(light)
        pc = Poly3DCollection(T, facecolors=shade(T, C, light), edgecolor=(0, 0, 0, .12), linewidths=.15)
        ax.add_collection3d(pc)
        mid = np.array([60, 21, 105.]); r = 110
        ax.set_xlim(mid[0] - r, mid[0] + r); ax.set_ylim(mid[1] - r, mid[1] + r); ax.set_zlim(mid[2] - r, mid[2] + r)
        ax.set_box_aspect((1, 1, 1)); ax.view_init(elev, azim); ax.set_axis_off(); ax.set_title(title)
    fig.suptitle(out.rsplit("/", 1)[-1] + f"   tilt={tilt}  pan={pan}", y=.93)
    fig.savefig(out, bbox_inches="tight"); plt.close(fig)

if __name__ == "__main__":
    V = [("iso", 24, -52), ("side (from -Y)", 0, -90), ("front (from +X)", 0, 0), ("top", 90, -90)]
    for t in (0, 45, 90):
        render(t, V, f"out/pantilt_tilt{t}.png")
    render(0, [("iso", 24, -52), ("side (from -Y)", 0, -90)], "out/pantilt_pan45.png", pan=45)
