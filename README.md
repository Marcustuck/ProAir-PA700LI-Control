# PA700LI GM3 → MQTT / Home Assistant

An **unofficial, experimental community project** for monitoring a ProAir PA700LI
ventilation controller over GazModem3/RS485 using an Arduino Nano ESP32, with a small
set of observation-backed absolute controls and Home Assistant MQTT discovery.

**Not affiliated with, supported by, or endorsed by ProAir, PLUM, or their partners.**
Product names identify compatibility only. This is not an official ProAir product,
not a production certification, and not a manufacturer-supported API or a substitute
for the controller's normal safety/operating provisions.

> **Monitoring is the default.** Both `ENABLE_CONTROL` and
> `DE_RE_WIRING_CONFIRMED` are **0**. Network and protocol credentials are blank.
> Compilation and synthetic tests pass; Arduino-origin RS485 commands have **not**
> been validated on hardware. Read the bus and wiring limitations before any active use.

## What it does

- Receives existing GM3 bus traffic; decodes temperatures, humidity, CO₂, filter
  depletion, fans/voltages, mode/Auto, bypass and separate Boost/Party status.
- Publishes MQTT discovery and fresh state without optimistic control feedback.
- Optionally offers **absolute Mode, Bypass and main Auto** controls, serialized one
  at a time and confirmed by new controller readbacks—not a generic ACK.
- Uses a bounded length/CRC parser and isolates MQTT/WiFi work from the UART reader.
- Contains **no built-in device login/PIN**, private recordings or captured identity
  metadata. Public tests use explicitly synthetic measurements and credentials.

## Hardware and software

| Item | Configuration |
|---|---|
| Board | Arduino Nano ESP32 |
| Board package tested | Official Arduino ESP32 Boards **2.0.18-arduino.5** |
| MQTT library tested | **PubSubClient 2.8** |
| UART | HardwareSerial(1), **115200 8N1** |
| Receive / transmit | **GPIO18 / GPIO17** |
| Driver enable | **GPIO4**; hardware DE and /RE arrangement must be confirmed |
| Pin numbering | **By GPIO number (legacy)**, mandatory |

Wrong Arduino-numbered pin mode intentionally fails compilation. The code does not
silently remap pins. Use a suitable RS485 transceiver; validate its logic levels,
grounding/isolation and reset-time driver-inactive bias according to the hardware.
Do not connect RS485 bus voltages directly to ESP32 GPIOs.

**GPIO4 is not a proven wiring description.** Before TX, confirm that it is actually
connected to the transceiver DE and that /RE is arranged so LOW disables the driver
and permits receive, while HIGH drives. A listen-only adapter with no connected DE
is not automatically suitable for transmission. Firmware cannot guarantee an inactive
DE before setup; hardware must keep reset/boot from driving the shared bus.

## Install and configure

1. Keep the `proair_mqtt_controls/` folder intact and open
   `proair_mqtt_controls/proair_mqtt_controls.ino` in Arduino IDE.
2. Install/select the official **Arduino Nano ESP32** board package and PubSubClient.
   Choose **Tools → Pin Numbering → By GPIO number (legacy)**.
3. Copy `proair_mqtt_controls/config.example.h` to
   `proair_mqtt_controls/config.local.h`. Edit only that ignored local file.
4. Fill the WiFi/MQTT fields for your own trusted broker. Set `MQTT_CLIENT` to a unique,
   nonempty client identifier. Plain MQTT port 1883 is retained; TLS is not implemented.
5. For monitoring, leave both control opt-ins at **0**. Protocol credentials are not
   needed to passively decode existing traffic. An existing panel/module must supply
   the polls: this sketch sends **no active read requests**.
6. For any separately authorized active trial, first review the risks below and
   confirm GPIO4/DE-/RE wiring. Configure your authorized installation's `GM3_LOGIN`
   and `GM3_PIN` locally, then deliberately set both opt-ins to **1**. Enabling only
   control is a compile error. Empty/invalid protocol credentials keep actuation
   inhibited at runtime even when the opt-ins are enabled.

Protocol credential strings must be printable ASCII, 1–24 characters each. The
encoder validates lengths and sizes its frame within a fixed upper bound. This is
not a claim that every controller accepts every credential length. No credentials
are generated, discovered, logged or supplied by the project.

`config.h` loads `config.local.h` if present; otherwise it uses the entirely blank
example. Do not edit real values into the example or commit local configuration.
Configured firmware images can contain secrets: `.gitignore` is not encryption, and
built images must also be treated as private.

The fixed MQTT root/device identifier `pa700li` is for **one controller**. A unique
MQTT client identifier alone does not isolate discovery entities for several units;
multiple-controller installations require consistent topic/identity changes.

## Implemented controls

| Home Assistant control | Selection | Verified wire value |
|---|---|---|
| Mode select | Compliance Level 1 / 2 / 3 / Pause | p1123, **four-byte LE** 3 / 4 / 5 / 6 |
| Bypass select | Auto / Closed / Open | p816, **four packed bytes** `21 01 10 00` / `09 01 10 00` / `11 01 10 00` |
| Main Auto switch | ON / OFF | absolute p241, **one byte** 1 / 0 |

These parameter/value semantics came from controlled observation on a single
installation. That does not establish compatibility across all hardware/firmware.
Credentials are configuration, not part of a public “universal” replay frame.

- **L1 is not “Base”; L3 is not “Boost”.** Fan percentages depend on configuration and
  overrides, not fixed mode constants.
- Bypass Auto is separate from main ventilation Auto. The observed p816 high bytes
  are not a general-purpose bitmask. Fresh readback must match one of the three
  complete supported layouts; otherwise controls fail closed.
- Auto requires fresh agreeing Compliance-mode copies. It does not implicitly leave
  Pause. Select a Compliance mode, await confirmation, then set Auto.
- As an additional conservative policy, all controls are inhibited during active
  short Boost (`p237=2`) or nonzero Party state, and when override state is stale.
- Bypass selection confirmation proves the reported selection, **not damper movement**.

### Deliberately not implemented

No short-Boost transmitter, Party ON/arbitrary Party value, timer-setting commands,
physical mode-cycle/Pause buttons or invented p239 behavior. The remote short-Boost
activation opcode is unknown. Short Boost remains read-only telemetry (about 15
minutes). Party remains a separate read-only state (often 3 hours). p237=1 is not
interpreted as short Boost.

Observed physical p225 flags are documented only: `08` Auto **toggle**, `10` relative
DOWN and `20` relative UP. Never blindly replay/retry toggles or relative steps.
Only their encodings are tested synthetically; there is no MQTT interface for them.
Captured Party OFF p1126=0 is also not exposed. Prefer the absolute selectors above.

## Confirmation, timing and bus safety

One command can be active at a time. Invalid values, busy state, unknown layouts,
uninitialized/stale state and mismatched copies are rejected. Numeric strings are
not coerced to zero. Already-matching fresh targets send no frame.

The engine waits at most 3 seconds for an empty parser/UART and a randomized 20–30 ms
quiet interval, checks RX again, enables DE, sends **once**, drains TX and returns DE
LOW. It then waits up to 20 seconds for fresh, post-TX readbacks:

- Mode: p1123 **and** independent p243 copy must agree on the requested mode.
- Auto: p234 **and** its independent copy must agree, with fresh agreeing Compliance mode.
- Bypass: a new complete p816 value must equal the requested packed selection.

The generic A9/E5 ACK identifies neither the parameter nor the particular master.
It never proves success. Status JSON on `pa700li/control/status` uses `pending`,
`confirmed`, `rejected`, `timeout` or `uncertain`, with a reason and local attempt/session
numbers. Partial writes/readbacks and ambiguous outcomes are not retried.
**There are no automatic write retries.** A new valid request is a new intent; reconcile
actual state first. Status delivery is best-effort QoS0, not a durable transaction log.

Mode acceptance and fan startup are different events. On a sampled Pause to
Compliance Level 2 transition, mode acceptance preceded fan startup by
16.686–18.196 s. That figure is a sampled bound, not a universal delay, and initial
zero fans do not by themselves indicate command failure. Boost may override fans
without changing the underlying mode.

### Multi-master warning

The captured source address **0x0064** is also used by the existing WiFi module.
Listen-before-talk and DE turnaround reduce risk but are **not verified arbitration,
address ownership or collision detection**. Another master may start at the same
instant; a matching readback need not establish which master caused it. There has
been no hardware validation of an Arduino-origin command. Keep manufacturer controls
available and do not rely on this experiment for safety-critical ventilation.

## Home Assistant and MQTT migration

Discovery uses `homeassistant/.../pa700li_<id>/config`, with actual `state_topic`s and
`optimistic:false`. Mode options are exactly **Compliance Level 1**, **Compliance
Level 2**, **Compliance Level 3**, **Pause**. Bypass options are **Auto**, **Closed**,
**Open**. Command discovery uses **`retain:false`** and QoS0.

State topics include `pa700li/state/mode`, `pa700li/state/bypass` and
`pa700li/sensor/auto`. Numeric sensors publish fresh readings non-retained and expire
after 30 seconds; control state is limited to 15-second-old agreeing observations.
Invalid/uninitialized readings are not zeros. The countdown idle sentinel is `None`
(unknown/not running), not an invented numeric sample. Filter data is correctly
labelled **Filter Depletion**, not remaining life. `pa700li/sensor/boost` is read-only
raw p237 status, labelled to distinguish the short-Boost value 2.

Node Last Will is offline. Control availability additionally requires enabled opt-ins,
configured protocol credentials, fresh coherent telemetry, no active command/override
and a 10-second guard after each MQTT connection.

### Session-scoped command payloads

PubSubClient does **not** pass the retained flag to its callback. This code cannot
inspect that flag and does not claim to filter retained packets directly.

Each MQTT connection creates a fresh non-secret nonce at `pa700li/control/session`.
Commands must have the envelope `<current_nonce>:<value>`; for example:

```text
<current_nonce>:Compliance Level 2    → pa700li/set/mode
<current_nonce>:Open                  → pa700li/set/bypass
<current_nonce>:ON                    → pa700li/set/auto
```

HA discovery adds the nonce automatically through select templates and switch
payloads. Custom publishers must read the current nonce and use this envelope.
Old naked payloads and old Base/Compliance/Boost mode aliases are rejected.

Previous-connection envelopes are rejected, including old retained commands delivered
on reconnect. This is **replay mitigation, not authentication**: authorized broker
clients can read the nonce, send duplicates or retain a current-session payload.
The callback still cannot distinguish those cases. Clear old retained command topics
before enabling, never retain command messages, and restrict broker publish access.
Only absolute operations are exposed; a fresh matching target is a no-op.

On a configured connection, the sketch intentionally removes the obsolete unsupported
Boost switch with an empty retained payload at
`homeassistant/switch/pa700li_boost/config`. It replaces Mode discovery with corrected
options, updates Auto in place, clears legacy retained sensor states and publishes
fresh values non-retained. No unrelated discovery objects are removed. Old custom
MQTT automations need deliberate migration to the new labels/envelope.

## Development and validation

See [TESTING.md](TESTING.md) for results from the **public credential-configurable
version**, reproducible commands and explicit untested areas. All distributed fixtures
are **generated synthetic data**, not anonymized or original recordings. Wire CRCs
are recomputed with obvious non-device test credentials; they do not reproduce
private authenticated frames or demonstrate hardware execution.

See [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Source in this repository is
covered by the existing MIT `LICENSE` and the copyright notice in that file.
Keep `LICENSE` verbatim; do not replace it with a dependency licence.
Dependency licences remain separate.
