"""Manufacturing utility: export a private QR label once, never show its secret in logs.

Run with: uv run --with pyserial --with 'qrcode[pil]' tools/setup_label.py --port /dev/ttyACM0
Normal user setup only needs the printed label and the Android app.
"""
import argparse
import json
import os
import re
import time
from pathlib import Path


def valid_label(raw: str) -> dict:
    label = json.loads(raw)
    if not isinstance(label, dict) or label.get("kind") != "androsaur-stand" or label.get("v") != 1:
        raise ValueError("not an Androsaur label")
    if not re.fullmatch(r"stand-[0-9a-f]{12}", label.get("id", "")):
        raise ValueError("invalid stand identity")
    if label.get("name") != "ANDRO_" + label["id"][-6:]:
        raise ValueError("invalid BLE name")
    if not re.fullmatch(r"[0-9a-f]{32}", label.get("pop", "")) or not re.fullmatch(r"[0-9a-f]{64}", label.get("fingerprint", "")):
        raise ValueError("invalid label credential")
    return {k: label[k] for k in ("kind", "v", "id", "name", "pop", "fingerprint")}


def export(port: str, destination: Path) -> Path:
    import serial
    import qrcode
    from PIL import Image, ImageDraw

    with serial.Serial(port, 115200, timeout=0.2) as console:
        console.dtr = False
        console.rts = False
        deadline = time.monotonic() + 20
        next_request = 0.0
        label = None
        while time.monotonic() < deadline:
            if time.monotonic() >= next_request:
                console.write(b"factory-label\n")
                next_request = time.monotonic() + 2
            line = console.readline().decode("utf-8", "replace").strip()
            if line.startswith("STAND_LABEL:"):
                label = valid_label(line.removeprefix("STAND_LABEL:"))
                break
    if label is None:
        raise RuntimeError("stand did not return a label; check firmware and UART port")
    qr = qrcode.make(json.dumps(label, separators=(",", ":")), box_size=8, border=4).convert("RGB")
    image = Image.new("RGB", (qr.width, qr.height + 64), "white")
    image.paste(qr)
    draw = ImageDraw.Draw(image)
    draw.text((qr.width // 2, qr.height + 16), "ANDROSAUR", fill="black", anchor="mt")
    draw.text((qr.width // 2, qr.height + 36), label["id"], fill="black", anchor="mt")
    destination.mkdir(parents=True, exist_ok=True)
    path = destination / (label["id"] + ".png")
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "wb") as output:
        image.save(output, format="PNG")
    return path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--out", type=Path, default=Path("labels.local"))
    args = parser.parse_args()
    try:
        print(export(args.port, args.out))
    except Exception:
        raise SystemExit("Could not export the label; credentials were not printed.") from None
