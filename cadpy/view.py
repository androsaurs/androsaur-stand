"""Entry point for the live viewer: `./viewer.sh`. Edit TILT/PAN and save to redraw."""
from ocp_vscode import show

from pantilt import assemble

TILT = 90   # 0 = platform horizontal, 90 = vertical facing +X; free range about -3..100 with the holder and phone
PAN = 0

from palette import STYLE

parts = assemble(tilt_deg=TILT, pan_deg=PAN, cable=True)
colors = [STYLE.get(n, ("#999999", 1.0))[0] for n in parts]
alphas = [STYLE.get(n, ("#999999", 1.0))[1] for n in parts]
show(*parts.values(), names=list(parts), colors=colors, alphas=alphas, reset_camera=None)
