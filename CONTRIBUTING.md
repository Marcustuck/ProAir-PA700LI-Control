# Contributing

This project is experimental. Respect the repository's existing maintainer-selected
MIT LICENSE and retain its copyright notice. Do not replace that file with a dependency
licence or alter authorship notices by assumption. Dependencies retain their own terms.

For a change:

1. Preserve monitoring-only defaults and both independent control opt-ins.
2. Keep protocol/network secrets in ignored local configuration; never embed shared
   credentials, packet dumps, real device IDs, broker addresses or session logs.
3. Run `./proair_mqtt_controls/extras/tests/run_tests.sh` with Python3 and a C++17
   compiler supporting ASan/UBSan. It generates fictional fixtures locally.
4. Compile the actual Nano GPIO-numbered target, including the enabled code path,
   and check that Arduino numbering and missing wiring confirmation still fail.
5. Explain behavior changes and how they are tested. A compile pass or synthetic
   frame comparison is not a hardware-transmission result.

Do not infer new controls from a coincidental telemetry change. Separate observed
status, write encoding, ACK timing and independent readback evidence. Avoid replaying
toggles, relative controls or ambiguous commands. Do not remove freshness checks,
serialization or no-retry behavior merely to make a test pass.

Use fabricated, clearly labelled fixtures. If a real capture is necessary for private
investigation, arrange an appropriate private channel first; sanitize all credentials,
metadata and unknown payload regions before proposing public test data.
