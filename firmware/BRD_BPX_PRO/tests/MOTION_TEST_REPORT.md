# Host verification record

`python3 tests/run_motion_tests.py`：11 modules of baseline regression passed (ADC, LOAD hysteresis, GPIO5 3-page input, BLE, measurement, OTA, storage), and syntax checks for seven firmware files and the Arduino `.ino`.

`tests/render_motion.cpp` + `brd_ui_motion.cpp`：20 themes x 7 pages x 16 requested frames rendered without failure. All seven pages ensure `fillScreen` is called exactly once during page entry, never during `update()` in the host render test.

`gif` frames are generated from the actual screen RGB565 pixels of the same C++ animation module, not AI artwork.

**Limits:** this environment does not have Arduino ESP32 board toolchain; runtime host stubs do not validate LCD hardware, ISR latency, BLE throughput, flash/RAM linker allocation or actual SPI signal stability.

Older `tests/run_host_tests.py` expects the removed two-page navigation and legacy fixed UI geometry, so it is no longer the correct runner for this new motion UI. Use the dedicated motion regression runner.
