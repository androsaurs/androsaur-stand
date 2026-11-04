"""Entry point for the live viewer: `./viewer.sh`. Edit TILT/PAN and save to redraw."""
from ocp_vscode import show

from pantilt import assemble

TILT = 90   # 0 = platform horizontal, 90 = vertical facing +X; free range about -3..100 with the holder and phone
PAN = 0

# colour and opacity per part (hex); anything not listed falls back to grey
STYLE = {
    "base": ("#2f6fdd", 1.0),          # blue leg + plate
    "carrier": ("#8e44ad", 1.0),       # purple
    "servo_pan": ("#555555", 1.0),
    "servo_tilt": ("#555555", 1.0),
    "plate_horn": ("#17a2b8", 1.0),    # teal
    "plate_shaft": ("#7fd3df", 1.0),   # light teal
    "platform": ("#e67e22", 1.0),      # orange
    "body": ("#f1c40f", 1.0),          # yellow holder spine
    "grip": ("#d35400", 1.0),          # dark orange clip
    "phone": ("#2c3e50", 0.35),
    "plug": ("#000000", 1.0),
    "cable": ("#c0392b", 1.0),         # red
    "box_lower": ("#6c8ea4", 0.35),    # steel blue, see-through
    "box_upper": ("#b8a1d6", 0.35),    # lilac, see-through
    "speaker": ("#e74c3c", 1.0),       # red
    "shelf": ("#2ecc71", 0.8),         # green
    "psu": ("#e8c547", 0.6),           # yellow placeholder block
    "dac": ("#9b59b6", 1.0),           # purple
    "amp": ("#f39c12", 1.0),           # amber
    "esp32": ("#00a86b", 1.0),         # jade
    "foot0": ("#333333", 1.0), "foot1": ("#333333", 1.0), "foot2": ("#333333", 1.0), "foot3": ("#333333", 1.0),
}

parts = assemble(tilt_deg=TILT, pan_deg=PAN, cable=True)
colors = [STYLE.get(n, ("#999999", 1.0))[0] for n in parts]
alphas = [STYLE.get(n, ("#999999", 1.0))[1] for n in parts]
show(*parts.values(), names=list(parts), colors=colors, alphas=alphas, reset_camera=None)
