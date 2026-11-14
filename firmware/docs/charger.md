# Direct and autonomous phone-charger control

## Verified wired profile (0.7.2-auto-charge)

The wired device profile enables autonomous40/80% hysteresis after successful
GPIO6 HIGH/LOW and actual charging-status verification. See `sdkconfig.wired.defaults`.
ESP32 keeps charging independently of the computer/phone application being connected;
stale/missing battery reports allow power according to the existing watchdog policy.
The phone supplies battery measurements, not switching commands. Manual overrides
remain temporary. The following base-profile description refers to the unverified
configuration; this verified profile sets automatic output to y.

Build a fresh wired configuration with
`-DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.wired.defaults"`;
existing local SDK settings must explicitly set `CONFIG_STAND_CHARGER_OUTPUT=y`.

## Direct command protocol

Source0.7.1-charger-control adds standard owner/hub `POST /v1/charger` on the
existing pinned HTTPS8443 connection. The phone is not in the control-command path.
OTA installation remains owner-only; this endpoint does not grant firmware-update rights.

JSON commands:

- `{"enabled":true,"durationMs":60000}`: allow charger input power for60seconds.
- `{"enabled":false,"durationMs":60000}`: deny charger input power for60seconds.
- `{"mode":"auto"}`: clear the override and restore configured automatic policy.

`enabled` must be boolean; duration integer1000..120000ms. Overrides are RAM-only
and expire to automatic policy. The control task checks expiry once per second;
status/observation processing also updates it. Overrides are not replayed at boot.

The standard manual driver defaults to GPIO6, active high, for HW-532 PWM input.
`CONFIG_STAND_CHARGER_MANUAL_CONTROL=y`, GPIO6. Automatic switching remains
separately disabled (`CONFIG_STAND_CHARGER_OUTPUT=n`) until hardware verification.
With automatic control disabled, boot and expired overrides leave the output off.
The ESP32 GPIO uses output with input sensing so status can report `gpioLevel`.

Status battery fields distinguish `outputEnabled` (driver configured),
`chargeAllowed` (effective power command), `policyChargeAllowed` (battery hysteresis),
`automaticEnabled`, `controlMode`, `manualActive`, `manualRemainingMs`, `gpio`,
and pin readback `gpioLevel`. The phone's actual `charging` measurement is separate;
GPIO HIGH is not proof of5V at USB or actual phone charging. The key's enable
polarity and physical charger output still need the first live test.

Computer commands, from the hub directory with the existing private database:

```bash
.venv/bin/python -m app.stand_charger status
.venv/bin/python -m app.stand_charger on --duration-ms 60000
.venv/bin/python -m app.stand_charger off
.venv/bin/python -m app.stand_charger auto
```

Use `--stand stand-<id>` to select exactly one of multiple enrolled stands.
The client verifies TLS pin before sending the delegated key and checks stand/phone
identity in the reply; it prints no credentials. Battery observation injection
remains owner-only. The diagnostic journal records the requested charger mode.
