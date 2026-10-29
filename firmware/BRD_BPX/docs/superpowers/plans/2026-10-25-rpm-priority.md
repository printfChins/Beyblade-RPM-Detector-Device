# RPM Priority Implementation Plan

> **For agentic workers:** 使用 superpowers:executing-plans 執行，最後獨立審查。

**Goal:** RPM 及 OLED 優先，背景交易不停止正常捕捉。
**Architecture:** IRAM capture、事件狀態機、延後套用設定、獨立完成資料、合作式背景排程。
**Tech Stack:** Arduino ESP32-C3 3.3.11、ESP-IDF 5.x、NimBLE 2.5.1、C++17 host tests。
**Spec:** docs/superpowers/specs/2026-10-25-rpm-priority-design.md

## Global Constraints
保留 V1.2、GPIO、所有既有 UUID/封包；RPM > OLED > 背景；低電及主動 OTA 保護保留。

## Review Focus
- NVS 寫入途中第一筆脈衝到來：事件保留且仍用原設定處理。
- HOLD 內下一次裝載：新量測優先且舊尾段不重複計次。
- 600ms 內連續完成：完成資料不被下一輪清空。
- Queue overflow/計時回繞：不得產生跨缺口錯誤 RPM。
- OTA 取消、失敗、低電與重新連線：不遺留停收狀態。

### Task 1: Capture 與量測完整性
Files: brd_capture.cpp/h, brd_measurement.cpp/h, brd_config.h, tests/test_priority.cpp, tests/stubs。
- [x] 寫測試：保存期間脈衝、HOLD 新量測、回繞/overflow、ISR 診斷。先失敗再修改並通過。
- [x] 原生 IRAM ISR，1024 FIFO，小塊快照；HOLD 顯示與重新武裝分離。

### Task 2: 背景交易及 Session
Files: brd_settings.cpp, brd_bbp_session.cpp/h, brd_bbp.cpp, brd_ota.cpp, tests。
- [x] 寫測試：完成資料跨下一輪、NVS 寫入新事件、延後套用設定、OTA 恢復。先失敗再修改並通過。
- [x] 保存不關捕捉；OTA 獨立維護取得/釋放；完成記錄獨立排隊。

### Task 3: 排程、整合與交付
Files: BRD_BPX.ino, brd_scheduler.cpp/h, brd_settings_ble.cpp, brd_oled.cpp, tests, RPM_PRIORITY_README.md。
- [x] 寫執行順序及 Notify 額度測試，先失敗後通過。
- [x] 執行全部 host 測試、長時間虛擬測試，獨立審查後修正重要問題。
- [x] 完整原始碼及中文修改說明打包，不包含舊 binary，保存修正版。
