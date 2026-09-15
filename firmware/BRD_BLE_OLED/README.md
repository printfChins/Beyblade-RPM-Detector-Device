# BRD_BLE_OLED V1.15

`BRD_BLE_OLED V1.15` 延續 V1.13 的雙邊沿完整一圈 RPM 量測，BLE 更新為 **BRD Reliable BLE Protocol V4**。

## V1.15 主要修改

- `PROJECT_VERSION` 更新為 `V1.15`。
- BLE 廣播名稱固定為 `BRD_XXXX`，`XXXX` 取 ESP32-C3 eFuse MAC 尾 4 碼 HEX。
- 移除 `0x81 STATE` 封包。
- 移除 `C5 STATE_ACK`。
- 移除 `state_seq`、READY pending 與 LOAD READY ACK 狀態機。
- `B1 LIVE` 改回連線且訂閱後 **每 200 ms 固定發送**。
- `B1 State / Flags` 為 App 唯一即時狀態來源。
- `WAIT_LOAD / LOADED_READY / SPINNING / RESULT_PENDING` 都會持續收到 B1。
- 單一 B1 Notify 掉包時，下一個 200 ms 封包會重新同步目前狀態。
- 韌體不因 BLE ACK、Notify、狀態同步或 timeout 主動呼叫 `disconnect()`。
- B2 LAUNCH 與 Reliable Curve A1/A2/A3/A4 + C1/C2/C3/C4 完整保留。
- V1.13 Rising/Falling 雙邊沿完整 360 度 RPM 計算保留。
- Curve 第一筆 `0 ms / 0 RPM`、MAX 35% 結束條件保留。

## GATT

| 用途 | UUID 尾碼 | Property | 內容 |
|---|---|---|---|
| Service | `0001` | - | BRD Service |
| Data | `0002` | READ / NOTIFY | `B1/B2/A1/A2/A3/A4` |
| Control | `0003` | WRITE | `C1/C2/C3/C4` |
| Firmware | `0004` | READ | `V1.15` |
| Diagnostic | `0005` | READ | Protocol V4 / BLE diagnostics |

## App 狀態同步

```text
Connect
-> Subscribe 0002 Notify
-> 約 100 ms settle
-> B1 LIVE 開始每 200 ms固定送出

WAIT_LOAD
-> B1 State=0 持續發送

LOAD 完成
-> B1 State=1 LOADED_READY 持續發送

開始旋轉
-> B1 State=2 SPINNING_LOADED 持續發送，同時帶 Current RPM

發射
-> B2 LAUNCH 一次
-> B1 State=3 SPINNING_LAUNCHED 持續發送

Result Pending
-> B1 State=4 持續發送
```

App 不需要對 LOAD/READY 回 ACK，也不需要處理 `0x81` 或 `C5`。

## RPM 量測

RPM GPIO 使用 `CHANGE` 同時捕捉 Rising / Falling。每一筆 RPM 都由同極性完整一圈週期計算：

```text
Falling[n-1] -> Falling[n] = 360° RPM
Rising[n-1]  -> Rising[n]  = 360° RPM
```

因此約每半圈更新一筆完整一圈 RPM，但不直接用半圈換算，不會因白黑占空比不對稱產生假高 MAX。

## 文件

- `BLE_API_V1.15.txt`：App 實作 API。
- `BLE_PROTOCOL.md`：Protocol 說明。
- `CHANGELOG.md`：版本修改。
- `VALIDATION.md`：主機測試結果。
- `TIME_REVIEW.md`：時間 / rollover 檢查。

## 電源管理注意

正常 BLE protocol timeout 不會主動斷線。V1.15 已移除待機 OLED OFF 與 Deep-sleep；裝置待機時維持 OLED、BLE 與主迴圈正常運作。低電保護仍可停用 BLE。
