# BRD BPX V1.2 — BLE OTA 批次傳輸與 Flash 擴容

## 交付檔案位置

```
BRD_BPX_V1.2_BLE_OTA/
└── BRD_BPX/
    ├── BRD_BPX.ino                 # [保留] 主程式，未修改
    ├── brd_bbp.cpp                 # [新增] NimBLE setMTU(247)
    ├── brd_config.h                # [修改] OTA_COMMAND_QUEUE_SIZE 8 -> 32
    ├── brd_ota.cpp                 # [修改] DATA 累計 ACK；4 KiB 緩衝寫入
    ├── brd_ota_protocol.cpp       # [新增] SYNC command 0x06
    ├── brd_ota_protocol.h         # [新增] FEATURE_BATCH_ACK / batch_mode
    ├── partitions.csv             # [修改] 雙 1984 KiB OTA App，移除 SPIFFS
    ├── partitions_legacy_1280k.csv# [新增] 舊分割表備份
    └── tests/                     # [新增] SYNC、掉包、MTU 247、Flash buffer 測試
```

網頁對應檔案：

```
Web-Flash-Download-Tool/
├── dist/app.js                     # [新增] 輸出實際 Payload / 傳輸模式
├── dist/ota-protocol.js            # [修改] 舊版回退 / 8 包 SYNC / offset 重傳
├── OTA_PROTOCOL.md                # [修改] 記錄新版擴充
└── tests/ota.test.mjs              # [新增] 批次與舊版相容的測試
```

## 新版封包規則

- INFO/HASH/BEGIN/END/ABORT/DATA 保持原封包內容。
- 第 19 Byte protocol_version 維持 1，與舊版共通。
- 新版回覆 flags 的 bit3 表示 FEATURE_BATCH_ACK。
- 新版主機在 BEGIN 後送 SYNC [0x06, session_id]；裝置回覆後切換為批次模式。
- 批次期間 DATA 仍使用 [0x10,id,u32 offset,payload]，不再逐包 Notify。
- 每 8 包完成後，網頁送 SYNC，裝置回覆已接受的累計 offset。
- 若 offset 小於本批結束位置，網頁由該 offset 重送。最多重試 3 次。
- 若未啟用 SYNC，仍逐包回 ACK；舊網頁完全相容。
- MTU 只由 BLE 雙方協商，裝置偏好 247，不保證手機會給 247。
- Flash 累積 4096 Bytes 寫入，END 強制刷新末尾不足 4096 Bytes。
- MD5 驗證與 Flash 實體讀回 MD5、esp_ota_end、切換開機分區規則維持不變。

## App 擴容

| 分區 | 位址 | 大小 |
| --- | --- | --- |
| NVS | 0x9000 | 20 KiB |
| OTA Data | 0xE000 | 8 KiB |
| App 0 | 0x10000 | 1984 KiB |
| App 1 | 0x200000 | 1984 KiB |
| Core Dump | 0x3F0000 | 64 KiB |

**首次導入新分割表必須 USB 更新。** 尚未更新分割表的 V1.2 設備可先透過 BLE OTA 安裝本版 App（只要 BIN 大小 <= 舊 1280 KiB），可使用新的批次傳輸，但容量仍是舊的 1280 KiB。

**注意 NVS 資料**：目前 Web Flash Tool 的 USB 模式 `eraseAll: true`，且完整 merged.bin 會覆蓋 NVS 位址，因而不能直接保留設備設定、50 筆歷史紀錄。若要求保留舊資料，請勿直接使用現有 USB 模式移轉分割表；應先備份 NVS，或另行實作選擇性分區燒錄，跳過 NVS。BLE OTA 不會清除 NVS。

## 燒錄及編譯提醒

1. 使用 Arduino IDE 開啟 `BRD_BPX/BRD_BPX.ino`，維持 ESP32-C3、4MB Flash 及原本 NimBLE-Arduino 2.5.1。
2. `partitions.csv` 與 `.ino` 在相同資料夾，編譯時自動使用自訂分割表；完成後檢查產生的 `.partitions.bin`。
3. **這次只有原始碼，沒有重新編譯的 MCU BIN**；不要將原 ZIP 舊版 `.bin` 當成新版本使用。
4. 遷移分割表使用 USB；既有分割表上更新 App 使用 BLE OTA。
5. 用 OTA 上傳的是 `BRD_BPX.ino.bin`（純 App），不能上傳 4 MB merged.bin。
6. 速度、手機 MTU、長時間連線及斷電恢復必須真機驗證。

## 本次測試

- 韌體主機 C++/sanitizer 全套：通過。
- 網頁 Node 協議測試 15 項：通過（舊模式、掉包、SYNC 遺失、238 Byte payload 等）。
- JS 語法檢查：通過。
- Playwright 手機 UI：本執行環境未安裝 Playwright，沒有跑。
- 真機 MCU 編譯與實際 OTA 傳輸：此執行環境未提供 Arduino/ESP32 工具鏈及裝置，沒有跑。
