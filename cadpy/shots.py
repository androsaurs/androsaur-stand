"""Render the documentation pictures into ../docs/img (matplotlib, no GPU needed).

    .venv/bin/python shots.py

assembly.png     whole assembly, two views
rear.png         rear wall: DC jack and vents, plus the opposite (front) side
inside.png       lower half with the stand and the lid removed: shelf, speaker, ESP32, power module (top and iso)
under-shelf.png  what hangs below the shelf (shelf hidden): amplifier, DAC, speaker, floor grille
mechanism-tilt*.png  pan-tilt stand with the phone at tilt 0, 45 and 90 degrees
"""
import sys
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

from palette import STYLE
from pantilt import assemble

OUT = Path(__file__).resolve().parent.parent / "docs" / "img"
OUT.mkdir(parents=True, exist_ok=True)
STAND = ["base", "carrier", "servo_pan", "plate_horn", "plate_shaft", "platform", "body", "grip", "phone",
         "plug", "cable", "servo_tilt"]
CENTRE = np.array([42.5, 42.5, 60.0])     # box centre in world coordinates (box floor underside is at z = -66)


def triangles(parts, alpha):
    T, C, A = [], [], []
    for name, sh in parts.items():
        v, f = sh.tessellate(0.15, 0.4)
        v = np.array([(p.X, p.Y, p.Z) for p in v])
        col = matplotlib.colors.to_rgb(STYLE.get(name, ("#999999", 1.0))[0])
        a = alpha.get(name, 1.0)
        for i, j, k in f:
            T.append(v[[i, j, k]]); C.append(col); A.append(a)
    return np.array(T), np.array(C), np.array(A)


def draw(parts, views, out, mid, radius, alpha=None, title=""):
    T, C, A = triangles(parts, alpha or {})
    cols = 2 if len(views) > 1 else 1
    rows = -(-len(views) // cols)
    fig = plt.figure(figsize=(7 * cols, 7 * rows), dpi=100)
    for idx, (caption, elev, azim) in enumerate(views, 1):
        ax = fig.add_subplot(rows, cols, idx, projection="3d", proj_type="ortho")
        a = np.radians(azim); e = np.radians(elev)
        light = np.array([np.cos(e) * np.cos(a - .6), np.cos(e) * np.sin(a - .6), np.sin(e) + .4]); light /= np.linalg.norm(light)
        n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-12
        k = 0.45 + 0.55 * np.abs(n @ light)
        rgba = np.concatenate([np.clip(C * k[:, None], 0, 1), A[:, None]], axis=1)
        ax.add_collection3d(Poly3DCollection(T, facecolors=rgba, edgecolor=(0, 0, 0, .10), linewidths=.15))
        rx, ry, rz = (radius, radius, radius) if np.isscalar(radius) else radius
        ax.set_xlim(mid[0] - rx, mid[0] + rx); ax.set_ylim(mid[1] - ry, mid[1] + ry)
        ax.set_zlim(mid[2] - rz, mid[2] + rz)
        ax.set_box_aspect((rx, ry, rz)); ax.view_init(elev, azim); ax.set_axis_off(); ax.set_title(caption)
    if title:
        fig.suptitle(title, y=.93)
    fig.savefig(out, bbox_inches="tight"); plt.close(fig)
    print(out.name, out.stat().st_size // 1024, "KB")


def pick(parts, names):
    return {k: v for k, v in parts.items() if k in names}


if __name__ == "__main__":
    p = assemble(tilt_deg=90, pan_deg=0, cable=True)
    # directions: +X = towards the phone, the rear wall is at -X. azim = -50 looks from the front-left (+X, -Y)
    draw(p, [("front-left (phone side)", 20, -50), ("rear-right (vents and DC jack)", 20, 130)], OUT / "assembly.png",
         np.array([42.5, 42.5, 62.0]), (80, 80, 130), title="Androsaur stand with the enclosure")
    draw(pick(p, ["box_lower", "box_upper", "foot0", "foot1", "foot2", "foot3"]),
         [("rear wall from outside (-X)", 0, 180), ("front, from +X", 0, 0)], OUT / "rear.png",
         np.array([42.5, 42.5, -31.0]), 62, title="Enclosure walls: DC jack and vents at the back")
    inner = pick(p, ["box_lower", "shelf", "speaker", "esp32", "psu", "dac", "amp", "foot0", "foot1", "foot2", "foot3"])
    draw(inner, [("top (lid and stand removed)", 90, -90), ("iso", 42, -35)], OUT / "inside.png",
         np.array([42.5, 42.5, -30.0]), 68, alpha={"box_lower": 0.3}, title="Inside the lower half")
    below = pick(p, ["box_lower", "speaker", "esp32", "amp", "dac", "foot0", "foot1", "foot2", "foot3"])
    draw(below, [("top, shelf hidden", 90, -90), ("iso, shelf hidden", 40, -35)], OUT / "under-shelf.png",
         np.array([42.5, 42.5, -40.0]), 68, alpha={"box_lower": 0.25}, title="Below the shelf: amplifier hangs from the shelf, DAC glued to it")
    for tilt in (0, 45, 90):
        q = pick(assemble(tilt_deg=tilt, pan_deg=0, cable=True, box=False), STAND)
        draw(q, [("iso", 24, -52), ("side (from -Y)", 0, -90)], OUT / f"mechanism-tilt{tilt}.png",
             np.array([60.0, 42.5, 80.0]), (95, 95, 115), alpha={"phone": 0.35}, title=f"tilt {tilt} deg")
