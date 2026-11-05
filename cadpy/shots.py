"""Render the documentation pictures into ../docs/img (numpy software renderer with a z-buffer, no GPU needed).

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

from palette import style
from pantilt import assemble

OUT = Path(__file__).resolve().parent.parent / "docs" / "img"
OUT.mkdir(parents=True, exist_ok=True)
STAND = ["base", "carrier", "servo_pan", "plate_horn", "plate_shaft", "platform", "body", "grip", "phone",
         "plug", "cable", "servo_tilt"]
CENTRE = np.array([42.5, 42.5, 60.0])     # box centre in world coordinates (box floor underside is at z = -66)


def triangles(parts, alpha):
    """Triangles of all parts: vertices (n,3,3), colours (n,3), opacity (n,), part id (n,)."""
    T, C, A, I = [], [], [], []
    for pid, (name, sh) in enumerate(parts.items()):
        v, f = sh.tessellate(0.15, 0.4)
        v = np.array([(p.X, p.Y, p.Z) for p in v])
        f = np.array(f)
        T.append(v[f])
        col = matplotlib.colors.to_rgb(style(name)[0])
        C.append(np.tile(col, (len(f), 1))); A.append(np.full(len(f), alpha.get(name, 1.0))); I.append(np.full(len(f), pid))
    return np.concatenate(T), np.concatenate(C), np.concatenate(A), np.concatenate(I)


def rasterize(T, C, A, I, elev, azim, mid, radius, size=800, ss=2):
    """Small software renderer: orthographic camera, z-buffer, flat shading, outlines, translucent parts blended last."""
    e, a = np.radians(elev), np.radians(azim)
    cam = np.array([np.cos(e) * np.cos(a), np.cos(e) * np.sin(a), np.sin(e)])
    right = np.array([-np.sin(a), np.cos(a), 0.0])
    up = np.array([-np.sin(e) * np.cos(a), -np.sin(e) * np.sin(a), np.cos(e)])
    n = size * ss
    scale = n / (2 * radius)
    P = T - np.asarray(mid)
    px = P @ right * scale + n / 2
    py = n / 2 - P @ up * scale
    pz = P @ cam                                   # larger = closer to the camera
    nrm = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True) + 1e-12
    light = cam + 0.5 * right + 0.8 * up
    light /= np.linalg.norm(light)
    shade = 0.40 + 0.60 * np.abs(nrm @ light)
    rgb = np.clip(C * shade[:, None], 0, 1)

    zbuf = np.full((n, n), -1e9)
    img = np.ones((n, n, 3))
    ids = np.full((n, n), -1, dtype=np.int32)

    def fill(k, blend=None):
        x = px[k]; y = py[k]; z = pz[k]
        x0, x1 = int(max(np.floor(x.min()), 0)), int(min(np.ceil(x.max()), n - 1))
        y0, y1 = int(max(np.floor(y.min()), 0)), int(min(np.ceil(y.max()), n - 1))
        if x1 < x0 or y1 < y0:
            return
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        d = (y[1] - y[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (y[0] - y[2])
        if abs(d) < 1e-12:
            return
        w0 = ((y[1] - y[2]) * (gx - x[2]) + (x[2] - x[1]) * (gy - y[2])) / d
        w1 = ((y[2] - y[0]) * (gx - x[2]) + (x[0] - x[2]) * (gy - y[2])) / d
        w2 = 1 - w0 - w1
        inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
        if not inside.any():
            return
        zz = w0 * z[0] + w1 * z[1] + w2 * z[2]
        sl = (slice(y0, y1 + 1), slice(x0, x1 + 1))
        if blend is None:
            ok = inside & (zz > zbuf[sl])
            zbuf[sl][ok] = zz[ok]
            img[sl][ok] = rgb[k]
            ids[sl][ok] = I[k]
        else:
            ok = inside & (zz > zbuf[sl])          # in front of everything opaque, no depth write
            img[sl][ok] = img[sl][ok] * (1 - blend) + rgb[k] * blend

    opaque = np.nonzero(A >= 0.999)[0]
    for k in opaque:
        fill(k)
    # outlines from the id buffer and depth jumps (before translucent parts, so glass has no hard edges)
    edge = np.zeros((n, n), dtype=bool)
    for dy, dx in ((1, 0), (0, 1)):
        edge[:-dy or None, :-dx or None] |= ids[:-dy or None, :-dx or None] != ids[dy:, dx:]
        edge[:-dy or None, :-dx or None] |= np.abs(zbuf[:-dy or None, :-dx or None] - zbuf[dy:, dx:]) > radius * 0.02
    img[edge] = img[edge] * 0.35
    glass = np.nonzero(A < 0.999)[0]
    glass = glass[np.argsort(pz[glass].mean(axis=1))]   # far to near
    for k in glass:
        fill(k, blend=A[k])
    # downsample
    img = img.reshape(size, ss, size, ss, 3).mean(axis=(1, 3))
    return np.clip(img, 0, 1)


def draw(parts, views, out, mid, radius, alpha=None, title=""):
    T, C, A, I = triangles(parts, alpha or {})
    rx, ry, rz = (radius, radius, radius) if np.isscalar(radius) else radius
    rad = max(rx, ry, rz)
    cols = 2 if len(views) > 1 else 1
    rows = -(-len(views) // cols)
    fig, axes = plt.subplots(rows, cols, figsize=(7 * cols, 7 * rows), dpi=100, squeeze=False)
    for ax in axes.flat:
        ax.axis("off")
    for ax, (caption, elev, azim) in zip(axes.flat, views):
        ax.imshow(rasterize(T, C, A, I, elev, azim, mid, rad))
        ax.set_title(caption)
    if title:
        fig.suptitle(title, y=.96)
    fig.tight_layout()
    fig.savefig(out, bbox_inches="tight"); plt.close(fig)
    print(out.name, out.stat().st_size // 1024, "KB", flush=True)


def pick(parts, names):
    return {k: v for k, v in parts.items() if k in names}


if __name__ == "__main__":
    p = assemble(tilt_deg=90, pan_deg=0, cable=True)
    # directions: +X = towards the phone, the rear wall is at -X. azim = -50 looks from the front-left (+X, -Y)
    draw(p, [("front-left (phone side)", 20, -50), ("rear-right (vents and DC jack)", 20, 130)], OUT / "assembly.png",
         np.array([42.5, 42.5, 70.0]), (80, 80, 175), title="Androsaur stand with the enclosure")
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
