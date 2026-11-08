# Security and operational safety

- This is an unofficial community experiment, not a safety controller. Preserve the
  manufacturer's normal controls and operating safeguards.
- `config.local.h` is ignored, not encrypted. Configured firmware and serial/network
  diagnostics can expose secrets if handled carelessly. Never attach them publicly.
- This release supplies no real protocol or network credentials. Public tests use
  clearly synthetic credentials that must not be tried against real equipment.
- MQTT on port 1883 is unencrypted. Keep the broker on a trusted network and enforce
  topic access controls. The session nonce is public and is not authentication.
- Do not retain command messages. Retained-flag detection is unavailable in the
  chosen MQTT callback; current-session messages and duplicates remain possible.
- GPIO4/DE-/RE wiring, voltage levels and reset-time inactive bias must be checked
  physically before TX. Idle-gap checks do not establish multi-master arbitration.
- A generic ACK does not prove execution. Ambiguous outcomes require reconciliation,
  not automatic retries. A lost MQTT status is not proof that nothing happened.

Do not post credentials, unredacted capture files, device identities or firmware
images in public issues. Use the repository maintainer's private security-reporting
route if one is enabled; otherwise ask for a private reporting channel without
including sensitive details. No reporting address or response-time commitment is
asserted by this document.
