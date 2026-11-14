# Wi-Fi firmware update

Firmware 0.6.4-ota supports owner-only OTA over the existing pinned HTTPS connection.
The first installation requires USB, including the rollback-enabled bootloader.
Existing local SDK configurations must explicitly set
`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`; the OTA source refuses to build without it.
Two 4 MiB app slots are already allocated in `partitions.csv`.
`CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=6144` reserves stack for HTTPS startup
and NVS diagnostics in the IP event handler; set it explicitly in existing local
SDK configurations too. The OTA guard has a 6144-byte stack for rollback and logging.
Status reports `httpsStartupStackFreeBytes` and `eventTaskStackBytes` for verification.

## Protocol

`POST /v1/ota`, owner Bearer key only (not the delegated hub key):

- `Content-Type: application/octet-stream`
- `Content-Length`: full raw application `.bin` size, between 288 bytes and the slot size.
- `X-Stand-Id`: the enrolled stand ID.
- `X-Image-SHA256`: 64 lowercase hexadecimal characters, SHA-256 of the entire `.bin`.
- Body: `build-ble/androsaur-stand.bin`, not a merged flash image or bootloader.

Use the BLE-enrolled certificate pin **before sending the key**. No HTTP fallback,
redirects or implicit retry of an upload. The server validates ESP32-S3 chip ID and
`androsaur-stand` project name before opening the inactive flash slot. It streams the
image with bounded RAM, a 10-second receive timeout and a 180-second overall deadline.
SHA-256 and ESP-IDF image validation must succeed before the next boot slot changes.
An interrupted/invalid upload leaves the running slot selected. Uploads temporarily
occupy the HTTPS server; tracking observations expire normally rather than queueing.

An accepted upload replies `{"ok":true,"rebooting":true,"confirmationRequired":true}`
and reboots after 1.5 seconds. A lost reply does not mean it is safe to upload again:
reconnect and inspect status first.

`GET /v1/status` includes capability `ota` and an `ota` object with `partition`,
`state`, full `elfSha256`, `version`, `busy`, `rollbackEnabled`, `confirmWithinS`,
and `maxImageBytes`. State is `pending_verify` or `valid`.

After reconnecting, verify stand/phone identity, expected ELF hash, new running slot,
and at least 15 seconds of uptime. Then send owner-only `POST /v1/ota/confirm`,
empty body, `X-ELF-SHA256` equal to the running image's full ELF hash. The ELF hash
is in the app descriptor, binary offsets 176–207. Confirmation is idempotent for the
same running image. A wrong hash is rejected even when the app is already valid.

Without confirmation within 120 seconds from boot, the new firmware marks itself
invalid and rolls back/reboots. The bootloader also rejects a still-pending image
on its next boot (for example after a crash/reset). A build that hangs forever
without a reset or omits the OTA guard cannot be recovered by a software task that
isn't running: only install compatible stand images. This is not secure boot or
publisher signature verification; the owner key authorizes the image, the pinned
TLS connection protects transport, and hashes detect corruption.

NVS ownership, Wi-Fi, TLS identity and diagnostic journal are outside both app slots.
The journal records `ota_start`, `ota_failed`, `ota_staged`, `ota_confirmed`,
and `ota_rollback`. OTA does not enable unconfigured servo or charging GPIOs.

## Builder verification with the paired Android phone

The Android `StandClient` supports upload/confirmation with its existing encrypted
owner binding. The debug APK additionally exposes a shell-`DUMP`-protected foreground
service so builders can test without exporting owner credentials. This is a debug
tool, not a new release-app update screen. Install the current debug APK first.

```bash
adb push build-ble/androsaur-stand.bin /data/local/tmp/androsaur-ota.bin
adb shell run-as dev.hram.androsaur cp /data/local/tmp/androsaur-ota.bin files/stand-ota.bin
adb shell run-as dev.hram.androsaur chmod 600 files/stand-ota.bin
adb shell am start-foreground-service -n dev.hram.androsaur/.StandOtaService --es op upload
adb logcat -d -s StandOta:I
```

`upload` checks the final slot, image hash and authenticated confirmation. Other
explicit test operations: `reject-sha`, `reject-image`, `reject-confirm`, and
`stage-only`. `stage-only` deliberately omits confirmation to exercise rollback;
allow more than 120 seconds plus network reconnection before checking the old slot.
Tests serialize: wait for the previous service invocation to finish before starting
the next. Secrets, request headers and binding JSON are never logged.
