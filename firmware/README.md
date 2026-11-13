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
- `/v1/logs`: persistent diagnostic ring (owner or delegated hub key; last 128 boot/Wi-Fi/IP/HTTPS events);
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

The prepared 0.3 firmware supports `status`, `policy`, and `tracking`: quiet state
is persisted, tracking input/controller is implemented, servo PWM is disabled until
calibrated. Audio and sensor drivers, siren and OTA are not implemented or advertised.
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

### Tracking implementation (included in 0.4.0-battery, flashed 2026-11-13)

Temporary bring-up Wi-Fi TX ceiling is 10 dBm (previously20) via
`CONFIG_ESP_PHY_MAX_WIFI_TX_POWER`. This caps PHY initialisation data as well as
normal operation; a post-start `esp_wifi_set_max_tx_power()` call alone would be
too late for the brownout seen during PHY init. Existing local sdkconfig must also
be updated, because defaults do not override saved values. `/v1/status.phyTxLimitDbm`
reports the configured PHY ceiling, not a measurement of RF output or board power.
Expect reduced radio range; actual effect on supply stability must be measured.
Brownout detection stays enabled. This ceiling is installed on the development board.
TLS allocations use its 8 MB PSRAM, retaining the standard 16 KB incoming record limit
and leaving internal memory for Wi-Fi/BLE. HTTPS permits five concurrent sessions.
Status includes free internal bytes and the largest internal block for diagnostics.

Owner-only `POST /v1/tracking` accepts `{v:1,session,seq,ageMs,visible,x,y}`.
X/Y are signed normalised face errors, X right and Y up. Lost target uses null X/Y.
Each accepted new observation computes one bounded incremental correction; duplicates,
older sequence numbers, retired sessions, out-of-range coordinates and age >500 ms
are rejected. A 20 ms task smooths pulse changes and stops changing them after loss
or expiry, retaining holding PWM. It never repeats a correction for the same frame.

Servo configuration is in `idf.py menuconfig` → Androsaur tracking. GPIO defaults
to -1, limits to the zero-width 1500…1500 us range, output disabled. Confirm GPIO,
neutral, actual safe pulse limits and axis directions before enabling output;
the neutral pulse is applied at startup. Limits are not inferred from CAD angles.
Gains currently start at 40 us/unit, max step20 us, slew100 us/s, deadband0.04 and
must be tuned on the real mechanism. `tracking` capability means the input/controller
exists; `/v1/status.tracking.outputEnabled` states whether PWM is actually enabled.

Host controller checks, no connected servo required:

```bash
cc -std=c11 -Wall -Wextra -Werror -I main main/tracking_core.c tests/test_tracking_core.c -lm -o /tmp/stand-tracking-test
/tmp/stand-tracking-test
```

The firmware build is checked. Do not flash or enable motors while the board is
resetting with `Brownout detector was triggered`; first restore stable power.

0. **Bring-up of the bare board (done, 2026-11-12).** Chip ESP32-S3 rev v0.2, 16 MB flash, 8 MB PSRAM (80 MHz, memory test OK),
   Wi-Fi radio starts, heartbeat each 2 s without resets. Found on real hardware: the main task stack (3.5 KB by default)
   overflowed in the Wi-Fi scan; now 8 KB. The radio scan found no networks because the home network is 5 GHz only
   (channel 52); the ESP32-S3 needs a 2.4 GHz network.
1. BLE setup, Wi-Fi station, mDNS and pinned HTTPS authorization implemented. OTA remains to be done.
2. Servos on GPIO4 (pan) and GPIO5 (tilt): LEDC PWM, smoothing, angle limits (docs/schematic.svg).
3. I2S to PCM5102A (BCK 15, LRCK 16, DIN 17), volume ceiling about 15% of full scale before the first sound.
4. Charger switch on GPIO6 (HW-532 module).

### Phone battery and charging (0.4.0-battery)

The owner phone sends `POST /v1/battery` over the same pinned HTTPS pool as face
observations. The hub credential cannot submit battery measurements. Packet:

```json
{"v":1,"session":"00000000-0000-0000-0000-000000000001","seq":1,"ageMs":20,"percent":75,"charging":true}
```

Percent is an integer 0…100; `charging` is the phone's actual Android charging
status, not presence of a cable. Unknown readings are omitted, never sent as 0%.
A separate latest-only battery queue shares the serial sender with camera results,
so no TLS handshake is needed per report and camera/hub availability is not required.
Android checks system battery state each second, sends on change and repeats at
15-second intervals. Measurement age is rechecked after TLS connection; packets
older than 60 seconds, duplicate sequences and retired sessions are rejected.

ESP owns charging policy: default resume at <=40%, pause at >=80%, retain the
previous decision in between. Restarting the phone session retains hysteresis.
No report at boot, or no fresh battery data for more than 10 minutes, allows power;
removing ownership also allows power. These are software decisions, not proof of
physical USB voltage. `/v1/status.battery` reports `percent`, `charging`, `ageMs`,
`seq`, `chargeAllowed`, thresholds, state and `outputEnabled` separately.

`menuconfig` → Androsaur phone charging configures thresholds, GPIO and active
polarity. Physical switching defaults OFF, GPIO -1. GPIO6 in the schematic is only
a proposal: confirm wiring and the HW-532 enable polarity before enabling it.
On a confirmed output the initial software state is power enabled; reset/unpowered
MCU behavior also depends on actual wiring and pull resistors and is not guaranteed
by this firmware. Charging control reduces time at high state of charge; it does
not diagnose or guarantee prevention of battery swelling or overheating.

Host policy checks:

```bash
cc -std=c11 -Wall -Wextra -Werror -I main main/battery_core.c tests/test_battery_core.c -o /tmp/stand-battery-test
/tmp/stand-battery-test
```
