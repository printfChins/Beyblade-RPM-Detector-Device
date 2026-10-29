# BRD BPX V1.2：4 MB Flash 雙 OTA App 擴容

## 交付內容與狀態

- [修改] `BRD_BPX/partitions.csv`：App 0、App 1 從 1280 KiB 擴為 1984 KiB。
- [刪減] 未使用的 1408 KiB SPIFFS 分區。
- [保留] `nvs`（0x9000，20 KiB）、`otadata`（0xE000，8 KiB）、`coredump`（0x3F0000，64 KiB）。
- [保留] V1.2 的 BLE OTA、逐批 ACK、4 KiB Flash 緩衝、RPM、OLED、NVS 歷史與設定。
- 此 ZIP 是**完整原始碼**，不是已重新編譯的韌體 BIN；需要用 Arduino IDE 重新編譯。
- 尚未在 ESP32-C3 實機驗證新的分割表燒錄／OTA。

## 擴容後記憶體地圖

| 名稱 | 起點 | 終點（不包含） | Bytes | KiB |
|---|---|---|---:|---:|
| NVS | `0x009000` | `0x00E000` | 20480 | 20 |
| OTA Data | `0x00E000` | `0x010000` | 8192 | 8 |
| App 0 | `0x010000` | `0x200000` | 2031616 | 1984 |
| App 1 | `0x200000` | `0x3F0000` | 2031616 | 1984 |
| Core Dump | `0x3F0000` | `0x400000` | 65536 | 64 |

0x000000 ～ 0x008FFF 保留作 bootloader、partition table 等 ESP32 系統用途。

## 檔案放置

```text
BRD_BPX_V1.2_BLE_OTA/
└── BRD_BPX/
    ├── BRD_BPX.ino
    ├── partitions.csv       <-- [修改] 必須與 .ino 位於同一資料夾
    ├── brd_config.h
    ├── brd_ota.cpp/.h
    ├── brd_ota_protocol.cpp/.h
    ├── brd_*.cpp/.h
    └── FLASH_APP_1984_README.md
```

## Arduino IDE 步驟

1. 開啟 `BRD_BPX/BRD_BPX.ino`。
2. 板子選擇 ESP32C3 Dev Module；Flash Size 選 4 MB；維持原專案使用的 CPU/Flash Mode 等參數。
3. 先確保 `partitions.csv` 放在與 `BRD_BPX.ino` 相同資料夾；Arduino-ESP32 建置系統會優先採用同層自訂分割表。
4. 先使用 `Verify/Compile`，檢查輸出的 `BRD_BPX.ino.partitions.bin` 對應本檔案的起點與大小。請勿將這次的分割表與舊 BIN 混用。
5. 前次使用預設分割選單時，Arduino IDE 顯示的「最大程式大小」可能仍是選單宣告的容量（如 1280 KiB），而不是自訂 CSV 容量。韌體超過此 IDE 宣告上限時，可使用最大可用程式大小 >= 2031616 Bytes 的分割選單供建置大小檢查；**實際 Flash 分割仍必須確認使用本自訂 CSV**。

## 第一次由舊分割表升級（必須 USB）

1. 先備份裝置上的 NVS 資料（參數與歷史）。
2. Arduino IDE 的 `Tools -> Erase All Flash Before Sketch Upload` 選擇 `Disabled`。
3. 使用 USB 上傳重新編譯的新韌體；正常上傳會更新 Bootloader／Partition Table／App 與 OTA Data，而不需要擦除獨立的 NVS 區域。
4. 上傳後檢查可以正常開機、NVS 設定及歷史可以讀取，並由裝置回報新的 OTA 容量（1984 KiB）。
5. 完成分割表遷移後，後續韌體更新才可以直接使用 BLE OTA。

**不要用目前 Web Flash Download Tool 的 USB 模式直接燒錄 4 MB merged.bin**：該流程設定 `eraseAll: true`，會清除 NVS；即使把 `eraseAll` 改為 `false`，4 MB merged.bin 仍會寫入 NVS 對應位址的內容，因此也不能用來保留設定。

若必須使用自訂 USB 燒錄器，請寫入個別編譯產物的對應位址，而不是完整 merged.bin：Partition Table 在 `0x8000`、Boot App 0 / OTA Data 在 `0xE000`、App 在 `0x10000`；ESP32-C3 Bootloader 在 `0x0`。必須避開 `0x9000`～`0xDFFF` 的 NVS 區間。

## BLE OTA 重要限制

- BLE OTA 只更新應用程式分區，**無法**自行修改 Flash Partition Table。
- 未進行 USB 分割表遷移的舊裝置，仍只有 1280 KiB 的 OTA App 容量。
- 自訂分割表不會修改 `Preferences`/NVS API；但若 USB 使用全片擦除，NVS 仍會遺失。
- SPIFFS 的舊檔案不再能透過 SPIFFS 存取，位址會被新 App 分區重用。
- 50 筆 RPM 與 32 點曲線的完整持久化是另一項功能需求，本次僅擴 App，不改變既有歷史紀錄的儲存格式與行為。

## 復原計畫

USB 上傳失敗時先不要執行全片擦除；使用 USB 重新燒錄相同的新分割表與經編譯的正確 V1.2 App。若 NVS 內容損壞，再從更新前的備份還原，避免使用舊分割表直接覆寫現有 App 配置。
