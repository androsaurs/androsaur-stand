# androsaur-stand firmware

ESP-IDF 5.5 firmware for the stand's ESP32-S3 (DevKitC-1, N16R8: 16 MB flash, 8 MB octal PSRAM).
Decision of 2026-11-12: the stand is a Wi-Fi module of Androsaur (HTTP and MCP on the home network, mDNS
`_androsaur-stand._tcp`, audio streamed over Wi-Fi, OTA updates), protected by a token.

## Build and flash

```bash
. ~/esp/esp-idf/export.sh           # ESP-IDF v5.5, installed with: ./install.sh esp32s3 (Python 3.10-3.13)
idf.py -DIDF_TARGET=esp32s3 build
idf.py -p /dev/ttyACM0 -b 921600 flash     # the port labelled UART on the board (WCH bridge, 1a86:55d3)
```

The console (logs, flashing) is UART0, the same cable. The other USB-C port is the chip's native USB (GPIO19/20), not used yet.
`sdkconfig` and `build/` are not committed; `sdkconfig.defaults` is. Local secrets go to `*.local.*` files (ignored by git).

## BLE pairing and authorization (2026-11-12)

Architecture: [`../../.github/docs/architecture.md`](../../.github/docs/architecture.md).
BLE uses Espressif Security 2, patch 1 (SRP-6a and AES-GCM), with mandatory random
128-bit proof of possession and username `androsaur`. Security 0/1 are disabled.
NimBLE preferred MTU is 517 so SRP-3072 handshake messages fit the Android transport.
The stand accepts one owner phone. Owner and delegated hub keys are independent;
only their hashes are stored. HTTPS on port 8443 is authenticated by a certificate
fingerprint received over the secured BLE session. No HTTP fallback is provided.

- Power on: `ANDRO_XXXXXX` is automatically discoverable over BLE with no five-minute timeout.
- Android panel: «Добавить подставку» → scan the exterior QR label → select the
  home 2.4 GHz network → enter its password. No computer, terminal or BOOT is needed.
- The label includes a random persistent 128-bit PoP, hardware ID, BLE name and
  TLS fingerprint. The PoP survives reboots and owner changes and is never printed
  on normal startup. Android stores it with the owner key in Keystore-encrypted storage.
- The camera module reads QR offline and returns it only to a signature-trusted app.
- Wi-Fi changes and «Отвязать» work over BLE from the existing phone, including
  when Wi-Fi is unavailable. Release revokes owner/hub rights and forgets Wi-Fi.
- Lost phone: three power cycles (3–5 seconds each) open a two-minute recovery
  window. Scan the label and explicitly confirm replacing the phone in the app.
  Neither a QR scan alone nor power cycles alone erase an existing binding.
- BOOT held ten seconds and released is retained only as a legacy emergency reset.
- Optional «Разрешить управление из хаба» delegates a separate key. The hub shows
  the stand inside its agent card and polls it directly, independently of the phone.
- mDNS `_androsaur-stand._tcp`, TXT `id`/`protocol`, port 8443. mDNS/IP are not trusted
  until certificate verification succeeds.
- Wi-Fi modem sleep is disabled (`WIFI_PS_NONE`): this mains-powered peripheral
  must respond to LAN discovery, ARP and control while BLE remains available.
- The authenticated BLE `info` response includes the current station IPv4 address
  while associated. Android uses this if mDNS fails, then authenticates over pinned
  HTTPS as usual. The last verified address is retained across Android restarts.
- `/v1/status`: owner or hub key; `/v1/hub`: owner only, set/revoke hub key;
  `/v1/policy`: persist boolean `quiet`, owner or hub key. All require HTTPS/Bearer.

### One-time manufacturing label

The builder does this while the board is accessible, not the person connecting it:

```bash
uv run --with pyserial --with 'qrcode[pil]' tools/setup_label.py --port /dev/ttyACM0
```

This invokes the physical UART `factory-label` command and saves the printable QR
as `labels.local/stand-<id>.png` with private file permissions. The directory is
ignored by Git. Print the label and attach it to the exterior of the enclosure.
Never publish it: it contains a setup credential. No owner/hub key or Wi-Fi password
is included. The same label remains valid after a normal reboot or owner release.

Capabilities are `status` and `policy` only: quiet state is persisted, but audio,
motor and sensor drivers are not implemented yet. Siren and OTA are not advertised.
Future MCP methods must reuse these authorization checks. NVS corruption fails
closed instead of automatically erasing ownership. Private TLS keys remain in NVS;
physical flash encryption/secure boot are not enabled in this prototype.

Stand traffic always uses pinned HTTPS. The existing phone-to-hub connection follows
hub transport settings; HTTPS is needed there to protect delegated keys in transit.

When upgrading a checkout from bring-up step 0, use a fresh configuration to avoid
the old `sdkconfig` overriding BLE/security defaults:

```bash
idf.py -B build-ble -DSDKCONFIG=sdkconfig.ble.local.config build
idf.py -B build-ble -DSDKCONFIG=sdkconfig.ble.local.config -p /dev/ttyACM0 -b 921600 flash
```

## Hardware roadmap

0. **Bring-up of the bare board (done, 2026-11-12).** Chip ESP32-S3 rev v0.2, 16 MB flash, 8 MB PSRAM (80 MHz, memory test OK),
   Wi-Fi radio starts, heartbeat each 2 s without resets. Found on real hardware: the main task stack (3.5 KB by default)
   overflowed in the Wi-Fi scan; now 8 KB. The radio scan found no networks because the home network is 5 GHz only
   (channel 52); the ESP32-S3 needs a 2.4 GHz network.
1. BLE setup, Wi-Fi station, mDNS and pinned HTTPS authorization implemented. OTA remains to be done.
2. Servos on GPIO4 (pan) and GPIO5 (tilt): LEDC PWM, smoothing, angle limits (docs/schematic.svg).
3. I2S to PCM5102A (BCK 15, LRCK 16, DIN 17), volume ceiling about 15% of full scale before the first sound.
4. Charger switch on GPIO6 (HW-532 module).
