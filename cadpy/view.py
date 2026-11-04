"""Entry point for the live viewer: `./viewer.sh`. Edit TILT/PAN and save to redraw."""
from ocp_vscode import show

from pantilt import assemble

TILT = 90   # 0 = platform horizontal, 90 = vertical facing +X; free range about -3..100 with the holder and phone
PAN = 0

parts = assemble(tilt_deg=TILT, pan_deg=PAN, cable=True)
alphas = [0.35 if name == "phone" else 1.0 for name in parts]
show(*parts.values(), names=list(parts), alphas=alphas, reset_camera=None)
