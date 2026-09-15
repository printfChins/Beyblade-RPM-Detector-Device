# BRD_BLE_OLED V1.15 時間與 Rollover 檢查

## 主要時間參數

- B1 LIVE：`BLE_LIVE_INTERVAL_MS = 200 ms`。
- BLE packet gate：`BLE_PACKET_INTERVAL_MS = 8 ms`。
- Subscribe settle：`BLE_SUBSCRIBE_SETTLE_MS = 100 ms`。
- Curve ACK timeout：`5000 ms`。
- Curve ACK retry：`1000 ms`。
- Curve Send timeout：`5000 ms`。
- A4 Status timeout：`2000 ms`。

## V1.15 B1 rollover

`live_periodic_millis_wrap` 將 `millis()` 放在 32-bit rollover 前，確認 rollover 前後 B1 仍持續每 200 ms 發送，BLE 連線不因 rollover 或狀態同步被主動斷開。

## 專項結果

共 13 項時間 / rollover 測試通過，包含 measurement micros rollover、雙邊沿 RPM micros rollover、OLED HOLD、Battery recovery/stale、Curve、ACK、Reconnect 與 B1 periodic millis rollover。
