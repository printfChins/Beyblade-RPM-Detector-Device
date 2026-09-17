# CHANGELOG

## V1.16 - 2026-09-15

- [修改] `PROJECT_VERSION` -> `V1.16`。
- [修改] BLE TX Power 由 `-6 dBm` 調整為 `0 dBm`。
- [保留] BLE Device Name `BRD_XXXX`、Protocol V4、B1 每 200 ms 持續狀態同步。
- [保留] B2、Reliable Curve A1~A4/C1~C4、雙邊沿完整一圈 RPM、35% 結束條件。
- [保留] 已移除待機 OLED OFF / Deep-sleep；低電保護仍可停用 BLE。

## V1.15 - Standby Sleep Removed

- [修改] 移除 30 秒待機 OLED OFF。
- [修改] 移除 5 分鐘待機 Deep-sleep 與 GPIO1 喚醒流程。
- [修改] WAIT_LOAD / LOADED_READY 長時間待機仍維持 OLED、BLE 與主迴圈正常運作。
- [保留] 低電量 / ADC 故障保護邏輯，不屬於待機休眠。

## V1.15 - 2026-09-14

- [修改] `PROJECT_VERSION` -> `V1.15`。
- [修改] BLE Device Name 固定為 `BRD_XXXX`，其中 `XXXX` 為 ESP32-C3 eFuse MAC 最低 16-bit 的 4 碼大寫 HEX。
- [修改] BLE Protocol -> `BRD Reliable BLE Protocol V4`。
- [刪除] `0x81 STATE` Packet。
- [刪除] `C5 STATE_ACK`、`state_seq`、LOAD READY pending/ACK 狀態機。
- [修改] `B1 LIVE` 改回 BLE connected + subscribed 後每 200 ms 固定發送。
- [修改] `B1 State / Flags` 成為 App 唯一狀態同步來源。
- [修改] WAIT_LOAD / LOADED_READY / SPINNING / RESULT_PENDING 都持續送 B1；單包掉包由下一筆 Snapshot 自動恢復。
- [保留] 韌體 protocol 不主動呼叫 BLE `disconnect()`。
- [保留] B2、Reliable Curve A1~A4/C1~C4、雙邊沿完整一圈 RPM、35% 結束條件。
- [驗證] 78 項主機硬體替身回歸通過；13 項 millis/micros/ACK/B1 rollover 專項通過。

## V1.14 - 2026-09-14

- [修改] `PROJECT_VERSION` -> `V1.14`。
- [修改] BLE Protocol -> `BRD Reliable BLE Protocol V3`。
- [刪除] `0006 State READ / INDICATE`。
- [修改] `0x81 State` 改由 `0002 NOTIFY` 傳送。
- [新增] `C5 STATE_ACK`：`C5 + State + state_seq(le16)`。
- [修改] `LOADED_READY` 每 200 ms 重送，收到正確 C5 後停止。
- [修改] 錯誤/舊 C5 只忽略，不中止量測、不斷線。
- [刪除] State Indication / ATT Confirmation / 2 秒 timeout 主動斷線 recovery。
- [修改] State Notify 失敗只按 200 ms 節流重試，不可壟斷 Curve TX。
- [保留] B1/B2、Reliable Curve A1~A4/C1~C4、雙邊沿完整一圈 RPM、35% 結束條件。

# BRD_BLE_OLED 修改紀錄

## V1.13 - 2026-09-14

- [修改] `PROJECT_VERSION` 更新為 `V1.13`。
- [修改] RPM 中斷由 `FALLING` 改為 `CHANGE`，同時捕捉 Rising / Falling。
- [新增] RPM ISR queue 每筆保存 `timestamp + level`。
- [新增] Rising / Falling 各自維護上一個同極性 timestamp。
- [修改] 每筆 RPM 仍以同極性完整 360 度週期計算，不直接以半圈時間換算。
- [改善] 兩組完整一圈窗口相差約 180 度，穩定後約每半圈更新一次 RPM，提高 MAX 與 Launch snapshot 的時間解析度。
- [保留] MAX 不平均、不平滑、不做 IIR 或候選峰值濾波。
- [保留] BLE Reliable Protocol V2 與所有 GATT / packet 格式；Firmware Revision 更新為 `V1.13`。
- [驗證] 新增不對稱半圈、每半圈更新、雙邊沿 micros 回繞與 Launch 使用最新雙邊沿 RPM 案例。
- [驗證] 79 項主機硬體替身回歸通過；13 項 millis/micros/ACK/State rollover 專項通過；8 個 `.cpp/.ino` 原始檔 C++17 語法檢查通過。

## V1.12 - 2026-09-13

- [修改] `PROJECT_VERSION` 更新為 `V1.12`。
- [修改] BLE Protocol Version 更新為 `2`。
- [新增] GATT `0006 State` Characteristic，UUID `7f510006-1b15-4d5f-9f4d-9b3c7a1d9a10`，Properties = READ / INDICATE。
- [新增] `0x81 State Snapshot/Event`：`state + flags + state_seq`。
- [新增] `state_seq` 只在 State / Flags 真正變更時遞增；新訂閱 Snapshot 不遞增。
- [新增] 0006 新訂閱立即排入最新 State Snapshot。
- [新增] State Indication 使用 NimBLE `onStatus()` 的 ATT Confirmation 成功狀態才清除 pending。
- [新增] State Indication 100 ms retry gate 與 2 秒 Confirmation timeout；逾時主動斷線，重連後重新同步 Snapshot。
- [修改] B1 LIVE 只在 SPINNING / `telemetry.active` 時每 200 ms Notify；WAIT_LOAD / READY / RESULT idle 不再週期送 B1。
- [保留] B2 LAUNCH、Reliable Curve A1/A2/A3/A4、Control C1/C2/C3/C4。
- [保留] V1.11 的 0/0 RPM 起點、35% 停止門檻、send/ACK/A4 timeout、Duplicate ACK、Session / Packet Index 防護與 Deep-sleep 保護。
- [驗證] 73 項主機硬體替身回歸通過；12 項 millis/micros/ACK/State rollover 專項通過；8 個 `.cpp/.ino` 原始檔 C++17 語法檢查通過。

## V1.11 - 2026-09-13

- [修改] `PROJECT_VERSION` 更新為 `V1.11`。
- [刪除] Web Compatibility BLE 傳輸路徑與 C0 mode switch，只保留 Reliable Protocol。
- [刪除] 舊 Web 50 ms 第二曲線 buffer。
- [保留] B1/B2/A1/A2/A3/A4 與 C1/C2/C3/C4。
- [新增] Reliable event curve 第一筆固定 `0 ms / 0 RPM`，CRC32 包含該筆。
- [新增] START/DATA/END 5 秒 send timeout。
- [新增] A4 STATUS 2 秒 timeout。
- [保留] Duplicate ACK idempotent、Session 驗證、C2 packet index bounds check。
- [修改] 發射停止門檻由 MAX 20% 改為 MAX 35% 以下。
- [修改] `record.ready` 存在時禁止 Deep-sleep，包含 TX_TIMEOUT 狀態。
- [修改] OLED Boot Logo 僅 `ESP_RST_POWERON` 顯示。

## V1.10

- 加入 V1.9 Reliable API 相容路徑與既有 Web Compatibility 雙協定。
- 加入 30 秒 OLED OFF / 5 分鐘 Deep-sleep / GPIO1 HIGH 喚醒。
- Firmware Revision READ 與 OLED BLE connection icon。
