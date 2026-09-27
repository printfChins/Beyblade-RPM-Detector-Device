BRD_BBPX LOAD / AUTO 雙模式版

本次原始碼已新增 AUTO 模式，但目前工作環境沒有 Arduino CLI / Arduino-ESP32
完整編譯工具鏈，因此沒有產生新的 ESP32-C3 binary。

為避免誤燒，已移除原包內未包含 AUTO 修改的舊 R4：
- BRD_BBPX.ino.esp32c3.bin
- firmware/BRD_BBP_ESP32C3_4MB_0x000000.bin

請使用本包原始碼重新編譯。
建議環境沿用原專案：
- Arduino-ESP32 3.3.11
- NimBLE-Arduino 2.5.1
- ESP32C3 Dev Module
- CPU 80 MHz
- Flash 4 MB / DIO / 80 MHz
- Default Partition

模式設定位置：../brd_config.h
預設：BRD_MEASUREMENT_MODE_LOAD
AUTO：BRD_MEASUREMENT_MODE_AUTO
