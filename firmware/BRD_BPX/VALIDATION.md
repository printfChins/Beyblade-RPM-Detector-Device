# Validation

## 2026-10-01 First-edge reference / dual-edge input

Host regression tests were rebuilt and executed with `-Wall -Wextra -Werror` and ASan/UBSan.

Validated:
- RPM GPIO interrupt mode is `CHANGE`.
- After measurement is armed, the first actual RPM edge selects the reference polarity for that measurement.
- Rising edge first: only rising-to-rising periods calculate RPM.
- Falling edge first: only falling-to-falling periods calculate RPM.
- The opposite polarity does not update instantaneous RPM, MAX, or BLE profile points.
- The selected reference polarity remains fixed for the entire measurement.
- BLE profile still stores only the earliest 32 valid reference-period points.
- MAX continues updating from valid reference periods after the 32-point profile is full.
- RPM queue overflow does not abort the active capture; it preserves the selected reference polarity and rebuilds only the same-polarity period baseline.
- After the reference is selected, ISR filtering only queues that polarity, preventing `CHANGE` mode from permanently doubling RPM queue traffic.
- LOAD state still requires 100 ms stable time and does not reset an active SPINNING measurement.
- LOAD mode still finishes only when instantaneous RPM is strictly below 20% of MAX, including the normal 300 ms no-reference-edge path that sets RPM to zero.

Result: 8/8 host test binaries passed.

Limitations:
- Arduino/ESP32 firmware binary was not regenerated in this environment because Arduino-ESP32 build tools are not installed here.
- No hardware-in-the-loop test was performed.
