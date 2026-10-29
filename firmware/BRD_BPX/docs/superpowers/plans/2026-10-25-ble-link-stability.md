# BLE Link Stability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 移除可重現的 BBP 傳送失敗／佇列滿強制斷線，保持 RPM 優先。

**Architecture:** 同一個非阻塞主迴圈狀態機保留待送頁面；失敗以時間戳節流。命令佇列仍有界，只拒絕超額新命令，記錄 GAP reason 供實機追查。

**Tech Stack:** C++、Arduino-ESP32 3.3.11、NimBLE-Arduino 2.5.1、既有 ASan/UBSan host runner。

**Spec:** `docs/superpowers/specs/2026-10-25-ble-link-stability.md`

## Global Constraints

- RPM > OLED > BLE；保留 V1.2、UUID、17-byte 封包、12 頁順序、32 筆佇列。
- 每輪最多一次 Notify，失敗後 100ms 重試，成功後 10ms 分頁；不忙等。
- 未有實機證據，不宣稱現場根因已確認；提供完整原始碼 ZIP，不附舊 BIN。

## Review Focus

- 成功一頁後塞車，不跳頁、不混合新舊快照：Task 1 case1。
- 輪詢突發塞滿佇列，不清掉已接受命令：Task 1 case2。
- RPM 長時間優先讓 BLE 延遲，第一個失敗不能斷線：Task 1 case4。
- 真正斷線重連或重新訂閱，不恢復舊頁面：Task 1 cases5/6。
- 失敗重試不能壓過 RPM：Task 1 case3 及真實主迴圈整合測試。

### Task 1: BBP 連線保留與回歸

**Files:** `brd_bbp.cpp/.h`、`brd_config.h`、`tests/stubs/NimBLEDevice.h`、`tests/test_bbp_link.cpp`、`tests/test_priority_ble.cpp`、`tests/run_host_tests.py`、交付說明。

**Interfaces:** 保留所有既有 API；`brd_bbp_get_diagnostics()` 新增斷線次數／最後 GAP reason。Notify 與 disconnect 透過既有 NimBLE 邊界測試。

- [x] 寫六個 BBP 連線案例，記錄實際 disconnect 呼叫；觀察 cases1～4 因既有錯誤行為而失敗。
- [x] 刪除 BBP 佇列滿／通知逾時主動斷線，失敗保留頁面並節流，新增唯讀斷線 reason。
- [x] 六個案例轉綠；加主迴圈失敗 Notify 期間的 RPM／額度整合驗證。
- [x] 執行 `python3 tests/run_host_tests.py`；完整 suite、既有 13 個 RPM 案例皆成功。
- [x] 審查差異、更新完整說明及清單，產生來源一致的 ZIP；由交付流程保存並回覆下載連結。
