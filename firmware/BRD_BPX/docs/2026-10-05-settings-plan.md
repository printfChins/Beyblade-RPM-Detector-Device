# BRD BPX BLE 設定實作計畫

**Goal:** 完成四項設定 API、量測後保存、共同百分比結束條件與開機排版。

**Architecture:** 純 C++ 設定模型負責參數檢查、草稿、保存狀態與編碼。ESP32 儲存模組在安全窗口寫入 NVS。獨立 BLE 模組只在 callback 入列並由主 loop 執行。

**Tech Stack:** Arduino-ESP32 / ESP-IDF >= 5.3、NimBLE-Arduino 2.5.1、Preferences、C++、Python 測試執行器。

**Spec:** docs/2026-10-05-settings-design.md

## Global Constraints

- 不改 GPIO、參考極性、32 點曲線與既有 BBP 封包。
- 預設取自 config.h；保留 V1.0。
- ISR 和 BLE callback 不寫 Flash；量測中保留查詢和通知，只拒絕修改與清除命令。
- Flash 成功才切換執行時設定。

## Review Focus

- 百分比相等時不可結束；必須嚴格低於。
- AUTO 未達最低 RPM 時不可發布結果。
- 設定切換不可清掉已完成的 MAX 與待發布曲線。
- Flash 失敗不得回報成功，也不得切換模式。
- 斷線、充電停用與重新連線不能執行舊連線的命令。

## Task 1: 設定模型與測試

- [x] 先寫 test_model.cpp，檢查界限、SAVE 凍結、保存失敗、CRC 損壞、20-byte 狀態。
- [x] 新增 brd_settings_model.h/.cpp，Values、Manager、handle_request、encode/decode。
- [x] 執行主機測試，確認上述行為通過。

## Task 2: 量測整合與保存

- [x] 先建立 GPIO/時間/NVS 模擬，重現 AUTO 下降到 30% 尚未立即結束。
- [x] 新增 brd_settings.h/.cpp，開機載入、保存請求與安全寫入。
- [x] 修改 brd_measurement.cpp/.h，共用百分比、執行時模式、安全暫停及模式中斷重設。
- [x] 測試下降門檻、零脈衝、保存時機、模式套用、MAX 保留與 NVS 失敗。

## Task 3: BLE、充電、OLED

- [x] 新增 brd_settings_ble.h/.cpp，連線世代隔離與有界命令佇列。
- [x] 修改 brd_bbp.cpp，加入設定服務與量測中的 BBP 指令檢查。
- [x] 修改 BRD_BPX.ino，加入設定載入、更新與充電 BLE 策略。
- [x] 修改 brd_oled.cpp 與 config，調整大小字並支援執行時 AUTO 狀態。
- [x] 對新增 BLE 模組做主機語法與操作測試，渲染 128x32 開機畫面檢查。

## Task 4: 交付

- [ ] 完成 API 文件、完整修改檔案/行號清單與測試報告。
- [ ] 執行完整主機測試，檢查 diff 與壓縮包。
- [ ] 更新原上傳 ZIP 的版本，交付完整專案。
