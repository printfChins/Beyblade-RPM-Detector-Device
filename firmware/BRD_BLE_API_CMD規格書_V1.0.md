# Beyblade RPM Detector BLE API CMD 規格書

- 專案縮寫：BRD
- 文件版本：V1.0
- 日期：2026-07-31
- 對應韌體：BRD Dual IR Launch Recorder
- GPIO1 邏輯：HIGH = 已裝載，HIGH → LOW = 發射

## 1. 介面摘要

| 項目 | 設定值 |
|---|---|
| 裝置名稱 | `BRD_XXXX` |
| Service UUID | `7f510001-1b15-4d5f-9f4d-9b3c7a1d9a10` |
| Notify Characteristic UUID | `7f510002-1b15-4d5f-9f4d-9b3c7a1d9a10` |
| Property | `READ | NOTIFY` |
| ATT MTU | 23 bytes |
| Byte order | Little Endian |
| LIVE 週期 | 50 ms |
| 曲線名義採樣週期 | 50 ms |

> 目前沒有 WRITE Characteristic，因此 APP → Device 控制 CMD 尚未實作。以下 CMD 為 Device → APP Notify Packet ID。

## 2. 狀態碼

| 值 | 名稱 | 說明 |
|---:|---|---|
| 0 | `WAIT_LOAD` | 等待裝載 |
| 1 | `LOADED_READY` | 已裝載，等待 RPM 邊緣 |
| 2 | `SPINNING_LOADED` | 裝載中旋轉 |
| 3 | `SPINNING_LAUNCHED` | 已發射，持續量測 |
| 4 | `RESULT_PENDING` | 結果待傳或傳送中 |

## 3. Packet ID

| ID | 名稱 | 長度 | 說明 |
|---|---|---:|---|
| `0xB1` | LIVE | 13 | 即時 RPM 與狀態 |
| `0xB2` | LAUNCH | 9 | 發射事件 |
| `0xA1` | CURVE_START | 16 | 結果摘要與曲線開始 |
| `0xA2` | CURVE_DATA | 6/10/14/18 | 1～4 筆曲線樣本 |
| `0xA3` | CURVE_END | 1 | 曲線結束 |

## 4. 0xB1 LIVE

| Byte | 欄位 | 型別 |
|---|---|---|
| 0 | Packet ID = `0xB1` | uint8 |
| 1 | state | uint8 |
| 2 | flags | uint8 |
| 3～4 | current_rpm | uint16 LE |
| 5～6 | max_rpm | uint16 LE |
| 7～8 | launch_rpm | uint16 LE |
| 9～10 | elapsed_ms | uint16 LE |
| 11～12 | curve_sample_count | uint16 LE |

Flags：bit0 loaded、bit1 measurement active、bit2 launch marker valid、bit3 result pending、bit4 charging。

## 5. 0xB2 LAUNCH

| Byte | 欄位 | 型別 |
|---|---|---|
| 0 | Packet ID = `0xB2` | uint8 |
| 1～2 | launch_rpm | uint16 LE |
| 3～4 | max_rpm_at_launch | uint16 LE |
| 5～6 | launch_time_ms | uint16 LE |
| 7～8 | launch_sample_index | uint16 LE |

## 6. 0xA1 CURVE_START

| Byte | 欄位 | 型別 |
|---|---|---|
| 0 | Packet ID = `0xA1` | uint8 |
| 1～2 | sample_count | uint16 LE |
| 3～4 | nominal_sample_interval_ms | uint16 LE |
| 5～6 | duration_ms | uint16 LE |
| 7～8 | max_rpm | uint16 LE |
| 9～10 | launch_rpm | uint16 LE |
| 11～12 | launch_time_ms | uint16 LE |
| 13～14 | launch_sample_index | uint16 LE |
| 15 | flags；bit0 = launch marker valid | uint8 |

## 7. 0xA2 CURVE_DATA

- Byte 0：`0xA2`
- Byte 1：本包樣本數 N，1～4
- 每筆樣本 4 bytes：`time_ms uint16 LE` + `rpm uint16 LE`
- 封包長度：`2 + N × 4`

發射點可能額外插入樣本，因此繪圖 X 軸必須使用每筆 `time_ms`，不能直接使用 index × 50 ms。

## 8. 0xA3 CURVE_END

單一 byte：`A3`。收到後確認累積樣本數等於 A1 的 `sample_count`，一致時才完成曲線。

## 9. 結果傳輸時序

```text
B1 LIVE（持續）
B2 LAUNCH（發射時一次）
B1 LIVE（持續）
A1 CURVE_START
A2 CURVE_DATA（多包）
A3 CURVE_END
```

## 10. 曲線重組

1. 收到 A1：清空上一筆暫存並保存摘要。
2. 收到 A2：依順序附加樣本。
3. 收到 A3：驗證樣本總數。
4. 以 `launch_sample_index` 標示發射點；`0xFFFF` 表示無效。
5. 最終成績與曲線以 A1～A3 為準，B1 僅作即時 UI。

## 11. 目前未實作

- APP → Device 開始 / 停止量測
- 修改停止 RPM 門檻
- 手動清除或要求重送結果
- 查詢韌體版本
- 應用層 ACK、CRC、封包序號
