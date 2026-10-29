# BRD BPX BLE OTA Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** 完成使用者已確認的方案 A，交付 V1.2 韌體與 GitHub 網頁 BLE OTA。

**Architecture:** 純 C++ Session 負責封包及狀態；NimBLE callback 只入列，主 loop 使用 ESP-IDF OTA 寫備用分區。網站使用同一個協議，逐包等待 ACK、重送與校驗完成。

**Tech Stack:** Arduino-ESP32、NimBLE-Arduino 2.x、ESP-IDF 5.3+、C++、Web Bluetooth、Node 原生測試。

**Spec:** docs/OTA_V1.2_DESIGN.md

## Global Constraints

- MCU 為 ESP32-C3，Flash 4 MB；沿用兩個 0x140000 app 分區。
- 版本 V1.2；保留 BBP/設定 UUID、AUTO/LOAD、GPIO、電池與充電圖示。
- 不在 BLE callback 寫 Flash；資料包最多 240 bytes，回覆固定 20 bytes。
- START SOC 至少 20% 或充電，ADC 有效且量測安全；無有效傳輸 15000 ms 中止。
- 備用分區在 MD5/映像驗證成功後才切換；完成 5000 ms 後重啟。

## Review Focus

- ACK 遺失時重送不得重複寫入；不同內容的重複位移應拒絕。
- 斷線/重連重用同一 handle，不得執行上一代排隊的 BEGIN。
- MTU=23 時控制與狀態仍合法，資料 payload 為 14 bytes。
- 驗證未完成前取消不得顯示成功或改變開機分區。
- BEGIN 的 GPIO 安全窗口與 Flash pause 不得留下量測停止或 NVS 交錯。

### Task 1: 封包與 Session

**Files:** brd_ota_protocol.h/.cpp、tests/test_ota_protocol.cpp。

**Interfaces:** Sink 提供 capacity/begin/write/finish/abort；Session 提供 handle/tick/disconnect/snapshot；Response 為固定 20-byte 格式。

- [x] 寫入成功、重送、錯誤位移、錯誤 image、MD5 finish 失敗、逾時/計時 wrap、安全策略的測試。
- [x] 執行測試，確認 BEGIN 未實作時失敗。
- [x] 實作 Session 與編碼，執行 sanitizer 測試直到通過。

### Task 2: 裝置整合

**Files:** brd_ota.h/.cpp、brd_bbp.cpp、brd_config.h、BRD_BPX.ino、brd_oled.cpp、測試硬體邊界與 run_host_tests.py。

**Interfaces:** brd_ota_create/connected/disconnected/stop/update/is_busy/get_display；使用 Task 1 Session。

- [x] 建立 GATT 佇列、斷線世代、Flash 開始/完成失敗、量測 pause/resume 的測試。
- [x] 執行新增測試確認失敗，再實作硬體層與主 loop/OLED 整合。
- [x] 執行全部原有測試與新增測試、主程式語法檢查。

### Task 3: 網頁

**Files:** dist/ota-protocol.js、dist/app.js、dist/index.html、dist/styles.css、tests/ota.test.mjs、package.json、README.md。

**Interfaces:** OtaClient 使用 Task 1 封包格式；INFO 回傳 MTU/slot 上限，HASH/BEGIN/DATA/END 使用同一個 id。

- [x] 建立真實封包格式、檔案相容性、ACK 遺失、傳輸取消、END ACK 遺失、斷線及校驗失敗的測試。
- [x] 執行測試確認 OTA 功能不存在，再實作可測的傳輸模組與 UI。
- [x] 執行 Node 全套測試與語法檢查，使用本機 DOM/瀏覽器驗證可用介面。

### Task 4: 審查與交付

- [x] 審查兩端協議、錯誤與分區切換，修正影響使用者的問題並回歸測試。
- [x] 記錄實際驗證與實機/編譯限制，更新 CHANGELOG、操作與主程式插入位置。
- [x] 保存完整 V1.2 ZIP，提供完整網站與已驗證 Git patch（GitHub 寫入回傳 403，未建立分支/PR）。
