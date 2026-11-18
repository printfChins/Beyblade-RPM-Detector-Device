# BRD BPX V1.4.4

本版在 V1.4.5 的 ESP32-C3 與 128×32 SSD1306 OLED 專案加入 500 mAh、1000 mAh 充電百分比估算檔位。解壓後使用 Arduino IDE 開啟 `BRD_BPX/BRD_BPX.ino`。

本次 GPIO3 FALLING 原始 RPM 量測改造詳見 [RPM_FALLING_RAW_CHANGELOG.md](RPM_FALLING_RAW_CHANGELOG.md)。
版本字串延續原專案，未主動更換韌體版本號。

| 修正 | 行為 |
|---|---|
| [本次修改] RPM 即時優先 | GPIO3 僅 FALLING；每一筆非零 Falling-to-Falling 原始週期直接換算 RPM/MAX，不使用中位數、平滑、二次低轉確認。達結束百分比立即停止取樣，之後才整理曲線與啟動 BLE 應用層傳輸。 |
| 歷史 CRC 損壞 | BLE 照常啟動，歷史暫存在 RAM。保留原 NVS 資料，收到清歷史命令 `0x75` 並保存成功後恢復持久化。 |
| READY 清歷史漏首發 | LOAD／AUTO 清除後可正常記錄第一發 MAX 與曲線。 |
| BLE 更名競態 | 斷線後更新 GAP 名稱和 Scan Response，保留現有 BLE Server 與剛建立的連線。 |

主機回歸已通過。請在 Arduino IDE 編譯 ESP32-C3 目標以產生可燒錄 BIN。

## 舊版 V1.4.4 BLE 參數與名稱設定（2026-11-15）

[修改] 出廠 BLE 名稱 `BEYBLADE_TOOL01`。新增 200/250/300mAh 電池容量選項、可透過 BLE SET/SAVE 設定 1000～10000ms HOLD、韌體版本 READ、藍牙名稱 READ/WRITE 及 NVS 保存狀態查詢。保持既有 BBP 17-byte 封包、OLED、RPM ISR、OTA 與充電硬體 100mA 不變。詳見 `SETTINGS_API.md`；純 HTML 設定頁為 `tools/BRD_BPX_BLE_Config.html`。

## V1.2 BLE OTA

[V1.2 新增] 完整更新方式、檔案放置、主程式修改位置及驗證限制，請先閱讀 [OTA_V1.2_README.md](OTA_V1.2_README.md)。本專案不含未更新的舊 BIN。

# BRD_BPX — BLE 設定與共用 RPM 結束條件

本包以重新提供的 BRD_BPX 專案為基礎，完成五項 BLE 設定、Flash 保存、AUTO / LOAD 共用結束條件及 OLED 開機排版。修改位置已用 `[新增]`、`[修改]`、`[刪減]` 註解標記。

## 使用方式

完整解壓縮，Arduino IDE 開啟 `BRD_BPX/BRD_BPX.ino`。根目錄 `.ino/.cpp/.h` 必須放在同一層；不需手動貼程式片段。

- 原專案使用 ESP32C3 Dev Module、CPU 80 MHz、Flash 4 MB、DIO / 80 MHz；[V1.2 修改] 使用同層的自訂 partitions.csv（雙 OTA App 各 1984 KiB），不可再使用舊版預設分割表。
- 依原專案版本使用 Arduino-ESP32 3.3.11、NimBLE-Arduino 2.5.1；程式要求 ESP-IDF >= 5.3。
- 若要保留 NVS 設定與歷史，停用 Erase All Flash Before Sketch Upload。擴容第一次必須透過 USB 更新分割表；詳見 FLASH_APP_1984_README.md。

## 電池容量檔位

出廠預設電池容量為 **300 mAh**。開機優先載入有效的 NVS 設定。要調整已保存的容量，可開啟 `tools/BRD_BPX_BLE_Config.html`，連線後選擇容量並儲存。容量經 `SET` 參數 `05` 暫存，收到 `SAVED` 後生效並可跨重啟保留。詳細封包見 [SETTINGS_API.md](SETTINGS_API.md)。

| 容量 | 100 mA 估算下，顯示增加 1% 的最短間隔 |
|---|---:|
| 200 mAh | 72 秒 |
| 250 mAh | 90 秒 |
| 300 mAh | 108 秒 |
| 500 mAh | 180 秒 |
| 1000 mAh | 360 秒 |

| 檔案位置（相對於 `BRD_BPX/`） | V1.4.4 修改 |
|---|---|
| `brd_battery_capacity.h` | [新增] 兩個容量常數及共用容量驗證。 |
| `brd_config.h`、`brd_battery.cpp` | [修改] 出廠電池容量、版本、計時公式與充電補償說明。 |
| `brd_settings_model.cpp`、`brd_settings_model.h` | [修改] SET 與 NVS 驗證。[新增] 容量擴充旗標 `bit7`。 |
| `tools/BRD_BPX_BLE_Config.html` | [修改] 初始選擇 300 mAh。[新增] 兩個容量選項。[刪減] JavaScript 重複維護的容量清單。 |

## 本次功能

### 充電關閉 BLE 兩行小字

[修改] 充電關閉 BLE 設定啟用且 BLE 已停止時，OLED 右下角以小字分兩行顯示 `BLE`、`OFF`，右上角充電閃電保留。一般連線沿用原圖示；單純未連線時留白。修改位置、顯示規則及預覽見 [CHARGING_BLE_ICON_NOTE.md](CHARGING_BLE_ICON_NOTE.md)。

| 設定 | 預設 | 可調範圍 |
|---|---|---|
| 量測模式 | LOAD | LOAD / AUTO |
| AUTO 最低有效 RPM | 2000 | 1～59999 |
| RPM 結束百分比 | 30% | 1～100% |
| 充電時自動關閉 BLE | 停用，充電時保留 BLE | 啟用 / 停用 |
| 電池容量 | 300 mAh | 200 / 250 / 300 / 500 / 1000 mAh |
| HOLD 時間 | 2500ms | 1000～10000ms |

LOAD 的確認值須小於或等於 MAX 乘上結束百分比，AUTO 則須嚴格小於。MAX=30000、結束百分比=30% 時，LOAD 以 9000 RPM 為含等號的門檻。AUTO 須先確認至少一次達到最低 RPM 門檻。達有效條件後，兩種模式皆可在無有效脈衝持續 300 ms 時結算。

設定在 RAM 草稿修改，SAVE 接受後於非量測時段寫入 Flash，讀回成功才套用，重開保留。**RPM 採樣期間不執行 BBP/設定 Notify、OTA 排程及 NVS 寫入；收到的查詢命令由 BLE 佇列延後到停止採樣後回覆**。修改設定及清除歷史仍遵守 BUSY／HOLD 安全窗口，不在結束後自動執行被拒絕的命令。已接受的 SAVE 若遇到量測開始則延後保存。

充電開關預設停用（`BLE_OFF_WHILE_CHARGING=0U`），充電時保留 BLE。已有有效 Flash 設定時仍載入原設定；若之前保存為啟用，需透過設定 API 將參數 `04` 設為 `0` 後 SAVE，才會改為充電時保留 BLE。也可 DEFAULTS 後 SAVE，但會同步重設全部五項參數。

充電開關啟用時只停用 BLE，量測與本機歷史保留；拔除充電後恢復。開機上方顯示 `BRD BPX`，下方版本 `FW VER V1.4.4` 取自 `brd_config.h` 的 `PROJECT_FIRMWARE_VERSION`。下圖為舊版 framebuffer 的版面參考。

![OLED 開機畫面](docs/OLED_boot_preview.png)

## 原架構保留

**GPIO3 使用 FALLING，不再啟用 RISING/CHANGE。** LOAD 裝載去抖成功才啟用 RPM 中斷；開始量測後停止 LOAD 中斷；每筆原始非零週期直接算 RPM 與 MAX，記錄在 16,384 筆固定 RAM（64 KiB）中，直到達到停止百分比。停止後才轉換為官方 BBP 32 點曲線、盡可能保留峰值。RAM 滿後仍計算即時/MAX，但會標記原始資料截斷。GPIO1 LOAD、GPIO10 充電、GPIO0 ADC、OLED SDA20 / SCL21 保留。

LOAD 去抖預設 **50 ms**、MAX HOLD **2500 ms**、OLED 更新 100 ms 保留。LOAD 去抖由 `brd_config.h` 設定，HOLD 可經 BLE 設定。

出廠 BLE 名稱為 `BEYBLADE_TOOL01`。BBP service `55c40000-f8eb-11ec-b939-0242ac120002`、特徵 `55c4f002-f8eb-11ec-b939-0242ac120002`、沿用 17-byte 封包及既有廣播結構。新增設定 Service 在連線後發現，不加入廣播。`0x51` / `0x74` 量測期間可收件，但**延後到停止採樣後**回覆；`0x75` 量測中拒絕且不新增 ACK。

既有 50 筆歷史與 `brd_bbp/history_v1` 格式保留；新設定使用 `brd_cfg/settings_v1`，兩者分開。

## 文件與測試

| 檔案 | 用途 |
|---|---|
| `SETTINGS_API.md` | 五項參數、UUID、8-byte 請求、20-byte 回覆、App 操作及保存流程 |
| `MODIFICATION_NOTE.md` | 完整新增／修改檔案與主程式加入位置 |
| `VALIDATION.md` | 實際驗證與尚未完成的板子驗證 |
| `CHANGELOG.md` | 本次行為變更 |
| `FILE_MANIFEST.txt` | 交付 ZIP 的實際檔案清單 |
| `tests/run_host_tests.py` | 重建並執行主機回歸測試 |

專案資料夾內執行 `python3 tests/run_host_tests.py`，需要 Python 3、g++ C++17 及 AddressSanitizer / UBSan。實際設定、量測、Session、BLE 排程及 OLED 程式參與測試，GPIO、時間、NVS 和 BLE stack 使用替身。測試不能證明實機 BLE 時序、射頻相容性或 Flash 延遲；驗證限制見 VALIDATION.md。
