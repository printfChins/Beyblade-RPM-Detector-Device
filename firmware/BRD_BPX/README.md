# BRD_BPX — BLE 設定與共用 RPM 結束條件

本包以重新提供的 BRD_BPX 專案為基礎，完成四項 BLE 設定、Flash 保存、AUTO / LOAD 共用結束條件及 OLED 開機排版。修改位置已用 `[新增]`、`[修改]`、`[刪減]` 註解標記。

## 使用方式

完整解壓縮，Arduino IDE 開啟 `BRD_BPX/BRD_BPX.ino`。根目錄 `.ino/.cpp/.h` 必須放在同一層；不需手動貼程式片段。

- 原專案使用 ESP32C3 Dev Module、CPU 80 MHz、Flash 4 MB、DIO / 80 MHz、Default Partition。
- 依原專案版本使用 Arduino-ESP32 3.3.11、NimBLE-Arduino 2.5.1；程式要求 ESP-IDF >= 5.3。
- 本次尚未以 ESP32 工具鏈編譯，未附新韌體 BIN，也未帶入原 ZIP 的舊 BIN / build。請重新編譯並完成板子驗證後燒錄。
- 若要保留 NVS 設定與歷史，停用 Erase All Flash Before Sketch Upload。

## 本次功能

### 充電關閉 BLE 兩行小字

[修改] 充電關閉 BLE 設定啟用且 BLE 已停止時，OLED 右下角以小字分兩行顯示 `BLE`、`OFF`，右上角充電閃電保留。一般連線沿用原圖示；單純未連線時留白。修改位置、顯示規則及預覽見 [CHARGING_BLE_ICON_NOTE.md](CHARGING_BLE_ICON_NOTE.md)。

| 設定 | 預設 | 可調範圍 |
|---|---|---|
| 量測模式 | LOAD | LOAD / AUTO |
| AUTO 最低有效 RPM | 2000 | 1～59999 |
| RPM 結束百分比 | 30% | 1～100% |
| 充電時自動關閉 BLE | 停用，充電時保留 BLE | 啟用 / 停用 |

AUTO 與 LOAD 都在 `current_rpm * 100 < max_rpm * finish_percent` 時結束並顯示最終 MAX。30% 指下降到 MAX 的三成；MAX=30000 時，要低於 9000 才結束。AUTO 必須先達最低 RPM 門檻，未達門檻的轉動不發布 MAX。兩種模式達有效條件後皆有 300 ms 無脈衝歸零備援；AUTO 移除原本等待零轉速 1000 ms 的結束條件。

設定在 RAM 草稿修改，SAVE 接受後於非量測時段寫入 Flash，讀回成功才套用，重開保留。量測期間仍可查詢與接收通知；修改設定及清除歷史會拒絕，不在結束後自動執行被拒絕的命令。已接受的 SAVE 若遇到量測開始，則延後保存。完成的 MAX、HOLD 及曲線不會因套用設定而清除。

充電開關預設停用（`BLE_OFF_WHILE_CHARGING=0U`），充電時保留 BLE。已有有效 Flash 設定時仍載入原設定；若之前保存為啟用，需透過設定 API 將參數 `04` 設為 `0` 後 SAVE，才會改為充電時保留 BLE。也可 DEFAULTS 後 SAVE，但會同步重設全部四項參數。

充電開關啟用時只停用 BLE，量測與本機歷史保留；拔除充電後恢復。開機上方大字 `BRD BPX`，下方小字 `FW VER V1.1`；版本號集中由 `brd_config.h` 的 `PROJECT_FIRMWARE_VERSION` 設定。實際繪圖函式的 framebuffer 預覽：

![OLED 開機畫面](docs/OLED_boot_preview.png)

## 原架構保留

RPM GPIO3 使用 CHANGE，每次量測由第一個實際邊沿決定固定參考極性；同極性到同極性為一整圈，另一極性不更新 RPM、MAX 或曲線。最早 32 個有效週期保存為曲線，32 點滿後仍可更新 MAX。LOAD GPIO1，充電 GPIO10，電池 ADC GPIO0，OLED SDA20 / SCL21 均保留。

LOAD 去抖預設 **50 ms**、MAX HOLD **2500 ms**、OLED 更新 100 ms 保留。去抖與 HOLD 時間只由 config.h 設定，本次沒有開放為 API。

既有 BLE 名稱 `BEYBLADE_TOOL_BRD_XXXX`、BBP service `55c40000-f8eb-11ec-b939-0242ac120002`、特徵 `55c4f002-f8eb-11ec-b939-0242ac120002`、17-byte 封包及原廣播內容保留。新增設定 Service 在連線後發現，不加入廣播。`0x51` / `0x74` 可在量測中查詢，`0x75` 量測中拒絕且不新增 ACK。

既有 50 筆歷史與 `brd_bbp/history_v1` 格式保留；新設定使用 `brd_cfg/settings_v1`，兩者分開。

## 文件與測試

| 檔案 | 用途 |
|---|---|
| `SETTINGS_API.md` | 四項參數、UUID、8-byte 請求、20-byte 回覆、App 操作及保存流程 |
| `MODIFICATION_NOTE.md` | 完整新增／修改檔案與主程式加入位置 |
| `VALIDATION.md` | 實際驗證與尚未完成的板子驗證 |
| `CHANGELOG.md` | 本次行為變更 |
| `FILE_MANIFEST.txt` | 交付 ZIP 的實際檔案清單 |
| `tests/run_host_tests.py` | 重建並執行主機回歸測試 |

專案資料夾內執行 `python3 tests/run_host_tests.py`，需要 Python 3、g++ C++17 及 AddressSanitizer / UBSan。實際設定、量測、Session、BLE 排程及 OLED 程式參與測試，GPIO、時間、NVS 和 BLE stack 使用替身。測試不能證明實機 BLE 時序、射頻相容性或 Flash 延遲；驗證限制見 VALIDATION.md。
