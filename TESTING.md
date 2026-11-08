# Public-source validation

Validated **2026-11-08 (Europe/Dublin)** against this public, credential-configurable
source—not by reusing results from an earlier private prototype.

## Results

| Check | Result |
|---|---|
| Official Nano ESP32 2.0.18-arduino.5, GPIO numbering, monitoring defaults | PASS; flash 735,725 bytes; static RAM 58,648 bytes |
| Same target, control + wiring-confirmation compile flags enabled | PASS; flash 737,613 bytes; static RAM 58,648 bytes |
| Arduino-numbered pin mode | Expected FAIL from the GPIO numbering guard |
| Control enabled without wiring confirmation | Expected FAIL from the DE-/RE opt-in guard |
| GCC14, C++17, `-Wall -Wextra -Wpedantic -Werror`, ASan + UBSan | PASS: **260,268 checks**, no sanitizer findings |
| Independent Python discovery JSON assertions | PASS: 16 sensors + 3 controls |

The Nano ESP32 flash and static-RAM sizes, and the intentional pin-numbering and
missing-wiring guard failures, are that local official-board-package record. Cloning
this repository does not repeat them, and they are not a hardware, broker, or Home
Assistant test. The host sanitizer command below is the check that can be repeated
without the Arduino package.

Actual builds used the installed official Arduino board package and PubSubClient 2.8,
not mock headers. Both enabled-build credential fields remained **blank**: compilation
exercises that code path, not successful device authentication or actuation. Runtime
unit tests separately exercise valid **synthetic** credentials and missing/invalid
credential rejection. No real credentials were supplied in build flags or test input.
Normalized fixture frames and CRCs are generated with the fictional credentials
`TESTUSER` / `DEMO`. They are not raw captures from a controller.

## What the tests cover

- **13 synthetic write fixtures**, independently generated in Python with explicitly
  non-device credentials and recomputed CRC16/XMODEM, are matched byte-for-byte by
  the C++ encoder. Nine represent exposed absolute control combinations; four exercise
  documentation-only p225 flags/Party OFF encodings, not extra MQTT interfaces.
- **19 synthetic telemetry fixtures**, not recordings or normalized recordings,
  exercise eight known block layouts, all 21 decoded fields, every supported mode,
  scalar widths/endianness and status-byte fingerprints.
- Invalid/blank/null/nonprintable/overlong protocol credentials fail closed; the
  accepted 24-character boundary and maximum 68-byte write frame are bounds-tested.
- Parser tests cover complete frames, every truncation point of a synthetic frame,
  CRC/end/length corruption, nested/suffix recovery, embedded marker bytes and
  100,000 deterministic noise bytes under sanitizers.
- Strict callback parsing rejects old-session envelopes, unsupported/ambiguous aliases,
  malformed numeric strings, empty/oversize/NUL/control-character payloads.
- Engine tests cover busy/disabled/stale/uninitialized/mismatched-copy rejection,
  active-override inhibition, Compliance prerequisites, already-matching no-ops,
  convergence before TX, fresh independent post-TX readback, ACK insufficiency,
  timeout/uncertain/partial-write outcomes, session cancellation, timer rollover
  and **zero automatic retries**. Initial zero fans do not prevent mode acceptance.
- Numeric sensor tests reject uninitialized/stale defaults and render countdown FFFF
  as `None`. Discovery tests independently inspect the JSON options/state topics,
  nonce templates, availability, non-retained command settings and buffer truncation.

## Reproduce host tests

With Python3 and a C++17 compiler supporting AddressSanitizer/UndefinedBehaviorSanitizer:

```sh
./proair_mqtt_controls/extras/tests/run_tests.sh
```

The script creates only generated fictional fixtures and local build outputs. It
reads no recordings, account configuration or hardware, and makes no network request.
`CXX` can select the compiler; `HOST_GCC_PREFIX` is optional for split toolchains.
Generated data/header/executable/discovery output can be regenerated and need not be
committed. Fixed test credentials are not valid installation credentials.

## Reproduce board builds

Install the official board package and PubSubClient 2.8, then run from the project root:

```sh
arduino-cli compile --fqbn arduino:esp32:nano_nora:PinNumbers=byGPIONumber proair_mqtt_controls
arduino-cli compile --fqbn arduino:esp32:nano_nora:PinNumbers=byGPIONumber \
  --build-property 'compiler.cpp.extra_flags=-DENABLE_CONTROL=1 -DDE_RE_WIRING_CONFIRMED=1' \
  proair_mqtt_controls
# These should fail with their specific safety-guard messages:
arduino-cli compile --fqbn arduino:esp32:nano_nora:PinNumbers=default proair_mqtt_controls
arduino-cli compile --fqbn arduino:esp32:nano_nora:PinNumbers=byGPIONumber \
  --build-property 'compiler.cpp.extra_flags=-DENABLE_CONTROL=1' proair_mqtt_controls
```

Use a clean checkout with **no** local configuration for these reproducible blank-
credential builds. Do not put credentials into command-line flags or attach build logs
containing local configuration to public issues.

## What these results do NOT establish

No firmware upload/reset, MQTT broker connection, live Home Assistant integration,
RS485 transmission, physical DE-/RE verification, bus collision/arbitration result,
hardware loss rate or actuator behavior was tested for this public preparation.
Synthetic tests are regression checks, not new capture or hardware evidence. Field
mappings/control values derive from controlled observation of one installation and
may not apply to another firmware profile.

UART hardware overflow/error monitoring is not installed. Parser counters do not prove
losslessness. Network/DNS/driver stalls and watchdog recovery remain untested in situ,
although network work is isolated from the sole UART reader. QoS0 statuses are not
durable execution receipts. Retained-flag filtering is not possible with this callback;
nonce validation is unit-tested, not a substitute for broker access control.
