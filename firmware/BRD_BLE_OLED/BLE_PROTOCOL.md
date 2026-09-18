# BRD Reliable BLE Protocol V4

對應韌體：`BRD_BLE_OLED V1.16`

## 架構

V1.16 BLE TX Power = 0 dBm；BLE Protocol V4 封包格式不變。

V1.16 維持週期式狀態同步。Device 只使用 `0002 NOTIFY` 傳送 B1/B2 與 Reliable Curve，不再存在獨立 State Packet 或 State ACK。

- `0002`：Device -> App，READ / NOTIFY，傳送 `B1 / B2 / A1 / A2 / A3 / A4`。
- `0003`：App -> Device，WRITE，傳送 `C1 / C2 / C3 / C4`。
- `0004`：Firmware Revision READ，V1.16 回傳 `V1.16`。
- `0005`：Diagnostic READ，Protocol Version = 4。

## B1 LIVE：唯一狀態來源

BLE 已連線且 App 訂閱 `0002` 後，Device 每 `200 ms` 固定送出一筆 B1。

```text
Byte 0     0xB1
Byte 1     State
Byte 2     Flags
Byte 3-4   Current RPM LE16
Byte 5-6   Max RPM LE16
Byte 7-8   Launch RPM LE16
Byte 9-10  Curve Duration ms LE16
Byte 11-12 Curve Sample Count LE16
```

State：

- 0 `WAIT_LOAD`
- 1 `LOADED_READY`
- 2 `SPINNING_LOADED`
- 3 `SPINNING_LAUNCHED`
- 4 `RESULT_PENDING`

Flags：

- bit0 Loaded
- bit1 Active / Spinning
- bit2 Launch Valid
- bit3 Result Pending
- bit4 Charging
- bit5 WAIT_ACK

App 必須以最新一筆 B1 的 State / Flags 覆蓋本地狀態。B1 是週期 Snapshot，不是事件佇列；單筆掉包不需要 ACK，下一個 200 ms Snapshot 會重新同步。

## 已移除

V1.16 不使用：

- `0x81 STATE`
- `C5 STATE_ACK`
- `state_seq`
- LOAD READY pending
- LOAD READY ACK timeout/retry state machine
- State Indication / ATT Confirmation

舊 App 若仍送 C5，V1.16 不會把它視為有效控制命令。

## 不主動斷線

V1.16 protocol flow 不因 Notify failure、Curve ACK timeout、A4 timeout 或未知控制命令主動呼叫 BLE `disconnect()`。

V1.16 維持無待機 Deep-sleep；待機不會因超時關閉 BLE stack。低電保護仍可停用 BLE。

## B2 LAUNCH

B2 只在發射事件送一次：

```text
Byte 0    0xB2
Byte 1-2  Launch RPM LE16
Byte 3-4  Max RPM at Launch LE16
Byte 5-6  Launch Time ms LE16
Byte 7-8  Launch Sample Index LE16
```

V1.13 起 RPM 使用 Rising/Falling 雙邊沿，但每筆 RPM 都是同極性 edge 到同極性 edge 的完整 360 度週期值。

## Reliable Curve

- `A1 START`
- `A2 DATA`，每包最多 4 samples，每 sample = `time_ms LE16 + rpm LE16`
- `A3 END`，含 Session / Packet Count / Sample Count / CRC32
- `A4 STATUS`
- `C1 ACK`
- `C2 Selective Retransmission`
- `C3 Full Retransmission`
- `C4 Abort`

Curve 第一筆固定為 `0 ms / 0 RPM`。

完整位元組格式以 `BLE_API_V1.16.txt` 為準。
