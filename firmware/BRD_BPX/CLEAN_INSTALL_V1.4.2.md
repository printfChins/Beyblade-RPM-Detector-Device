# BRD_BPX V1.4.2 — 乾淨完整原始碼安裝說明

本 ZIP 直接取自最後確認版本 `BRD_BPX_V1.4.2_HOLD_2500ms.zip`。
所有原有 `.ino`、`.cpp`、`.h`、`partitions.csv` 內容**逐位元組完全保留**；
僅額外提供這份安裝文件，避免將 V1.3／V1.4 的檔案混入 V1.2 架構。

## Windows 安裝位置

`C:\UserCode\ESP32\Arduino\Beyblade-RPM-Detector-Device\firmware\BRD_BPX\`

1. 關閉 Arduino IDE。
2. 將舊 `firmware\BRD_BPX` 整個資料夾改名為 `BRD_BPX_backup`（如已存在備份，先換不同名稱）。
3. 將本 ZIP 內的 **BRD_BPX 整個資料夾**解壓縮至 `firmware\`，**不要直接合併解壓到既有 BRD_BPX**。
4. 開啟 `firmware\BRD_BPX\BRD_BPX.ino` 並使用原 V1.2 ESP32-C3 的 Arduino 開發環境編譯。
5. 檢查新的 `BRD_BPX` 資料夾內 **不存在** 下列舊版檔案：
   - `brd_scheduler.cpp`
   - `brd_scheduler.h`
   - `brd_capture.cpp`
   - `brd_capture.h`

以上四個檔案來自其他 RPM 擷取／排程架構，本版本並未引用。
`brd_measurement_background_ready()` 是那個架構的函式，不應新增到這份韌體。

## 程式重點

- 韌體版本：`V1.4.2`。
- OLED 即時值標籤：`SP`；數值計算公式**不變**。
- LOAD HOLD：`2500 ms`，保留完成後暫停量測及 WAIT LOAD 重新武裝。
- BLE：使用 V1.2 傳輸架構，加上命令 Queue 滿／Notify 逾時不主動斷線修正。
- 電池：21 點 SOC 放電曲線與充電補償，不使用 FULL 偵測。
- OTA、檔案目錄、原始碼皆維持原版本。

## 檢查與限制

所有原始碼已與上一次交付 ZIP 逐位元組確認一致，且不含 brd_scheduler 或 brd_capture 檔案。
使用 `tests/run_host_tests.py` 可進行主機端回歸測試；不是 ESP32-C3 燒錄檔測試。
本 ZIP 不含編譯產物 `.bin`；尚須以 Arduino IDE 重新編譯及實機確認。
