# BRD_BPX V1.4 BLE 電量 Notify 30 秒定時更新

> 檔案位置：`BRD_BPX/docs/BLE_BATTERY_NOTIFY_30S_V1_4.md`。本次僅改 BLE 狀態封包觸發方式，**不更動電池 SOC 計算**。

## 修改目標

- [新增] APP 完成 BBP Notify 訂閱後，延續既有訂閱穩定等待 `100ms`，主動送一筆最新 `0xA0`。
- [新增] 自最近一次成功交給 NimBLE stack 的 `0xA0` 起，每 `30000ms` 定期發送最新 `0xA0`。
- [刪減] 裝載 LOAD 變化時立即觸發 `0xA0`。
- [保留] APP 寫入 `0x51` 主動查詢會得到 `0xA0` 回覆，該筆 A0 同樣重設 30 秒計時。
- [保留] 發射資料完成後仍主動傳送 `B0~B7`、`70~73` 共 12 頁結果。

## 排程與負載保護

- RPM ISR / 量測優先、OLED 次之，BLE Notify 維持低優先權；沿用 `brd_scheduler_take_notify_slot()`，不在 ISR 處理 Notify 或 ADC。
- `BBP_NOTIFY_INTERVAL_MS=10ms` 是傳送各頁間隔；**新增**的 `BBP_BATTERY_NOTIFY_INTERVAL_MS=30000ms` 才是 A0 自動更新週期。
- `0xA0` 必須在目前回覆、12 頁曲線傳輸結束後才開始發送。當 BLE 因背景量測忙碌而延後時只發送**一筆最新**電量，不一次補送多包。
- Notify 失敗時保留同一包，沿用 `BBP_NOTIFY_RETRY_INTERVAL_MS=100ms` 節流重試，不強制斷線；30 秒計時只有成功排入 BLE stack 後才重設。NimBLE `notify()==true` **不等於手機 APP 應用層已收到確認**。
- 斷線或取消訂閱後，不再定期 Notify；重訂閱後重新發一次 A0 並重設週期。

## A0 協定及 APP

使用原 BBP Service 與原 Characteristic，**不更動 UUID**。

| Byte | 值或意義 |
|---|---|
| 0 | `0xA0` |
| 3 | 狀態旗標；相容模式 bit2 為發送當下 LOAD 狀態，不再依裝載切換即時更新 |
| 4 | 電量 raw `0..250`；韌體先由 `brd_battery_get_percent()` 取得平滑後的 `0..100%` |
| 7..8 | 終身最高值（維持原格式） |
| 9..10 | 累積次數（維持原格式） |
| 11..16 | 裝置 UID（維持原格式） |

APP 可繼續以 `battery_percent = battery_raw * 100 / 250` 解碼；因協定單位為 0.4%，可能存在小幅量化誤差。APP **必須真正訂閱 Notify** 且處理每一筆新 A0，單純建立 GATT 連線不會自動收到通知。如果 APP 自己在 LOAD 事件發送 `0x51`，裝置仍會依原命令規則回覆 A0；如需完全禁止此 APP 行為，需同步調整 APP 查詢策略。

## 修改檔案位置

```text
BRD_BPX/
├── BRD_BPX.ino                         # [保留] 不修改
├── brd_config.h                       # [新增] 獨立 30000ms 設定與斷言；[修改] 協定模式註解
├── brd_bbp.cpp                        # [修改] A0 訂閱/週期/優先權/重試計時；[刪減] LOAD 即時 A0
├── tests/
│   ├── test_bbp_link.cpp              # [新增] 情境 7..12、最新 SOC 測試
│   ├── test_priority_ble.cpp          # [修改] LOAD 不再產生額外 A0 的整合測試預期
│   └── run_host_tests.py              # [修改] 執行 12 組 BLE 連線情境
├── docs/
│   └── BLE_BATTERY_NOTIFY_30S_V1_4.md # [新增] 本文件
└── CHANGELOG.md                       # [新增] 本次修改記錄
```

## 驗證

- 12 組 BLE 連線與通知情境：含既有 6 組和新增 6 組。
- 全套 13 個主機端測試執行檔，含 13 組 RPM 優先權情境、長時間 RPM、OLED、OTA、電池 SOC、GATT。
- 僅對韌體模組與主程序進行主機端 Stub 編譯/執行與語法檢查，不等於 ESP32-C3 實機編譯及空中 BLE 實測。
- 保留 V1.4 版本字串，未附任何舊 `.bin`。
