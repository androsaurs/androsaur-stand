# Isolated observation channel (0.9.1)

## Problem and boundary

The management HTTPS server on 8443 runs one HTTPD task. Its synchronous TLS
handshake and request reads can delay other requests, including legacy tracking.
The servo task then stops changing positions after the existing 500 ms freshness
limit. The hub's camera graph travels independently and can remain live throughout.

This release isolates observations without weakening freshness, pinning or ownership.
The 500 ms limit is unchanged. It does not promise to fix every mechanical jerk or
every Wi-Fi loss. OTA/flash operations can still briefly stall flash-resident code;
timing counters expose delays rather than assuming a perfectly periodic task.

## Protocol and compatibility

Authenticated management `/v1/status` advertises `observationPort:8444`. Android
0.15.0 uses that port when advertised and otherwise retains the old 8443 protocol.
Both ports use the same enrolled certificate and TLS pin. The owner key stays in
Android's existing encrypted binding. The hub/delegated key cannot inject observations.

Android 0.15.1 additionally polls `/faces` on a dedicated IO coroutine at a 100 ms
cadence, with a 200 ms module read timeout. It no longer waits behind the main
assistant loop's audio events, status refresh, profiling or identity requests.
Frame-ID deduplication and analysis-start timestamps are unchanged; faster polling
does not manufacture new observations or reset the age of an old frame.
Android 0.15.2 drains an HTTP 400 observation rejection and drops that frame while
keeping the verified TLS pool. A stale-frame rejection no longer causes a new
handshake/reconnect delay for subsequent fresh frames. No failed frame is retried.

The isolated endpoint accepts HTTP/1.1 keep-alive:

Starting with 0.9.1, accepted observation sockets explicitly enable TCP_NODELAY,
and each small HTTP reply is sent as one TLS record (headers plus body), avoiding
the Nagle/delayed-ACK interaction between two small writes on a persistent stream.

- owner-only `GET /v1/status`: identity/warm-up reply (`ok`, `id`, `phoneId`);
- owner-only `POST /v1/tracking`: existing camera packet and acknowledgement;
- owner-only `POST /v1/battery`: existing battery packet and acknowledgement.

There are no motor/manual/settings/OTA operations on this port. Existing management
and legacy observation endpoints remain available on 8443. Tracking's measured
server receive/processing delay is added to the packet age before validation.

Each accepted socket gets an independent worker, with two bounded slots. TLS uses
nonblocking sockets and the ESP-IDF asynchronous handshake API, a 1500 ms handshake
deadline, and a sleeping/select wait between attempts. An established stream runs
at priority 6; management HTTP and handshake workers have priorities 5 and 4.
Incomplete requests expire after 200 ms from their first received byte; an idle
connection expires after 15 seconds. Headers/body are limited to 1536/512 bytes.
Duplicate lengths/auth headers, chunked encoding, Expect, folded headers and
unsupported routes are rejected. No unbounded allocation/task creation or payload
logging. LWIP socket capacity is 16; existing local sdkconfigs need that setting too.

## Servo and persistence

The servo task runs at priority 7 with a 20 ms delay-until schedule. It waits at
most 2 ms for its mutex; a missed acquisition is counted. Settings writes are
serialized with a separate settings mutex. NVS write/commit happens outside the
servo mutex; the new configuration is applied only after a successful commit.
Stop disables PWM before attempting its persistence. Status takes a short snapshot
under the servo mutex and builds JSON after releasing it. PWM return codes are checked.

`tracking.timing` contains boot-lifetime high-water marks/counters:

- `maxTickMs`, `loopAgeMs`: actual interval and time since last servo iteration;
- `maxLockWaitUs`, `lockTimeouts`: mutex contention;
- `maxObservationGapMs`: intervals between accepted tracking packets;
- `staleStops`: fresh-to-expired transitions (a reported lost face is not expiry);
- `pwmErrors`: LEDC update errors, not measured shaft position.

`observationTransport` contains accepted connections, handshake failures, incomplete
request timeouts, accepted packet count, rejected connections/auth attempts,
maximum handshake duration and maximum request parse/dispatch duration. The latter
excludes waiting for the next request and response transmission. Counters are RAM
only; normal observations and per-tick commands are not written to NVS.

## Verification

Host parser checks (including deterministic malformed-input fuzzing):

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -I main \
  main/observation_http.c tests/test_observation_http.c -o /tmp/stand-observation-test
/tmp/stand-observation-test
```

On hardware, keep the real phone stream active and record `seq`, packet count,
freshness, loop timing and PWM errors before/during/after each test. Open a slow
TLS peer or incomplete HTTP-header peer on 8443 and then on the spare 8444 slot.
They must not stop the existing observation stream. Check that delegated hub
credentials are rejected by 8444. Do not inject invented camera coordinates or
change motor ranges for this test. Test normal status/settings/stop/OTA and legacy
compatibility separately. Measured results belong in the project's session brief.
