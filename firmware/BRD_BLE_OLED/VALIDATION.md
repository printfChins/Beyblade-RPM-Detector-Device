# BRD_BLE_OLED V1.16 驗證紀錄

## BLE Device Name 驗證

- 模擬 ESP32-C3 eFuse MAC：`...ABCD`。
- 預期 BLE Device Name：`BRD_ABCD`。
- `gatt_identity` 回歸測試會驗證此規則。


## Host Regression

最終 V1.16 工作樹執行 `verification/run_host_tests.py`：

- 78 / 78 cases PASS。
- 8 個 `.ino/.cpp` 來源檔通過 C++17 `-Wall -Wextra -Werror -Wshadow -pedantic` syntax check。
- AddressSanitizer / UndefinedBehaviorSanitizer 啟用於主機測試。

## V1.16 BLE 驗證

- `live_periodic_wait_load`：WAIT_LOAD 每 200 ms 持續 B1，無 0x81。
- `live_periodic_loaded_ready`：LOADED_READY 每 200 ms 持續 B1，無 C5 需求。
- `live_periodic_no_c5`：舊 C5 誤送不會停止 B1 或斷線。
- `live_periodic_millis_wrap`：B1 cadence 跨 millis rollover 正常。
- `live_periodic_state_change`：WAIT_LOAD -> READY -> SPINNING 均由週期 B1 同步。
- `live_notify_failure_never_disconnects`：Notify 失敗不觸發 protocol disconnect，恢復後 B1 繼續。

## Time / Rollover

`verification/time_review/run_time_review.py`：13 / 13 PASS。

## 限制

主機測試使用硬體替身，不能取代 ESP32-C3 + 真實 NimBLE + 手機/上位機的 RF、GATT timing 與功耗實機驗證。

- `standby_never_sleeps`：模擬待機 10 分鐘，OLED 不關閉、BLE stack 保持啟用、Deep-sleep 不觸發。

## V1.16 BLE TX Power 驗證

- `PROJECT_VERSION == "V1.16"`。
- `BLE_TX_POWER_DBM == 0`。
- NimBLE `setPower()` 測試替身只接受 0 dBm，確保初始化路徑實際使用新設定。
