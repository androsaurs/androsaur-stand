# Standard servo control without a phone

Firmware0.7.0 implements manual servo commands as a standard release feature.
The computer/hub talks directly to ESP32 over pinned HTTPS8443, using the existing
delegated hub key; the phone may be off. Owner-key requests work too. Neither
anonymous Wi-Fi clients nor invalid keys can control motors. This permission does
not grant OTA, ownership replacement, credential management, battery telemetry
injection or face-observation injection; those retain owner-only authorization.

## Endpoints

- `POST /v1/servos/center`, JSON `{}`: both channels1500us,50Hz,5seconds.
- `POST /v1/servos`, JSON `{"panUs":1525,"tiltUs":1500,"durationMs":1000}`:
  explicit pulse widths and duration. Integers only; each pulse1450..1550us,
  duration20..5000ms. Missing, fractional and out-of-range values are rejected.
- `POST /v1/servos/stop`, JSON `{}`: disable both PWM channels.
- `GET /v1/status`: capability `servos`; `tracking` includes access, available
  pulse bounds, configured GPIOs, last command time, commanded pulses, remaining
  duration and `outputEnabled`. Commanded values are not measured shaft positions.

PWM turns off automatically at the end of each command even if the client loses
connection. A reboot does not repeat the command or start PWM. No command is
automatically retried after a network error: inspect status/journal first.
Manual control does not turn on autonomous face tracking. The journal records
centering and manual pulse/duration commands without credentials.

## Default build and hardware

`CONFIG_STAND_SERVO_MANUAL_CONTROL=y`, panGPIO4 and tiltGPIO5 are normal defaults
for this stand's confirmed wiring. `CONFIG_STAND_SERVO_OUTPUT` (automatic tracking)
remains off until mechanics and tracking limits are calibrated. The temporary
`CONFIG_STAND_SERVO_CENTER_TEST` flag from0.6.5 has been replaced.

The uncalibrated manual envelope is deliberately narrow.1500us is nominal neutral;
an unknown initial shaft position can still cause a larger first movement. There
is no angle feedback. Do not interpret pulse changes as measured degrees.

## Computer command

From the `androsaur-hub` directory, with its existing private database:

```bash
.venv/bin/python -m app.stand_servo status
.venv/bin/python -m app.stand_servo center
.venv/bin/python -m app.stand_servo move --pan-us 1525 --tilt-us 1500 --duration-ms 1000
.venv/bin/python -m app.stand_servo stop
```

For several enrolled stands, select exactly one using `--stand stand-<id>`.
The command loads the delegated credential privately, verifies the enrolled TLS
certificate before sending it, verifies stand/phone identity in the reply, and
prints only public controller status. No phone, debug APK, running hub web server
or key in command-line arguments is needed for these servo commands.
