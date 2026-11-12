"""Read-only BLE setup interoperability probe for builders, not part of user setup.

uv run --with bleak --with cryptography --with protobuf --with pillow --with zxing-cpp \
  tools/probe_setup.py --label labels.local/stand-<id>.png --idf /path/to/esp-idf
"""
import argparse
import asyncio
import json
import os
import sys
from pathlib import Path


async def probe(label: dict, negative: bool = False) -> bool:
    from bleak import BleakClient, BleakScanner
    from security.security2 import Security2

    device = await BleakScanner.find_device_by_filter(lambda _, adv: adv.local_name == label["name"], timeout=12)
    if device is None:
        raise RuntimeError("stand not discovered")
    async with BleakClient(device, timeout=15) as client:
        async def raw(endpoint, data):
            if isinstance(data, str):
                data = data.encode("latin-1")
            characteristic = "0000" + endpoint + "-0000-1000-8000-00805f9b34fb"
            await client.write_gatt_char(characteristic, data, response=True)
            return bytes(await client.read_gatt_char(characteristic))

        version = json.loads(await raw("ff51", b"ESP"))["prov"]
        assert version["sec_ver"] == 2 and version["sec_patch_ver"] == 1
        password = label["pop"]
        if negative:
            password = ("a" if password[0] != "a" else "b") + password[1:]
        security = Security2(1, "androsaur", password, False)
        try:
            response = None
            for _ in range(4):
                request = security.security_session(response)
                if request is None:
                    break
                response = (await raw("ff52", request)).decode("latin-1")
            else:
                raise RuntimeError("handshake did not finish")
        except Exception:
            if negative:
                return False
            raise
        if negative:
            return True

        async def exchange(request):
            data = security.encrypt_data(json.dumps(request, separators=(",", ":")).encode())
            return json.loads(security.decrypt_data(await raw("ff53", data)))

        info = await exchange({"op": "info"})
        assert info["ok"] and info["id"] == label["id"] and info["fingerprint"] == label["fingerprint"]
        networks = await exchange({"op": "networks"})
        assert networks["ok"] and isinstance(networks["networks"], list)
        print("PASS: protected identity and 2.4 GHz scan; networks:", len(networks["networks"]))
        return True


async def main(label):
    assert await probe(label)
    await asyncio.sleep(1)
    assert not await probe(label, negative=True)
    print("PASS: wrong QR secret rejected")
    await asyncio.sleep(1)
    assert await probe(label)
    print("PASS: reconnect; no ownership/Wi-Fi changes requested")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", type=Path, required=True)
    parser.add_argument("--idf", type=Path, required=True)
    args = parser.parse_args()
    os.environ["IDF_PATH"] = str(args.idf)
    sys.path.insert(0, str(args.idf / "tools/esp_prov"))
    from PIL import Image
    import zxingcpp
    from setup_label import valid_label

    try:
        code = zxingcpp.read_barcode(Image.open(args.label))
        if code is None:
            raise ValueError("label is not readable")
        label = valid_label(code.text)
        asyncio.run(asyncio.wait_for(main(label), 90))
    except Exception as error:
        raise SystemExit("Setup probe failed: " + type(error).__name__) from None
