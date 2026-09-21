# BRD_BBP — OLED 藍牙圖示替換版

修訂識別：`API_V1.1_R4`，2026-09-21。依使用者提供的 `BRD_BLE_OLED(3).zip`，採用該專案原生 **7×13 OLED 藍牙符號**，點陣、尺寸與位置皆直接沿用。專案與開機字串仍為 `BRD_BBP`。

## 開啟與替換方式

完整解壓縮後，以本包的 `BRD_BBP/` 替換舊專案資料夾，Arduino IDE 開啟 `BRD_BBP/BRD_BBP.ino`。根目錄全部 `.cpp/.h` 與 `.ino` 放在同一層。主程式沒有新增片段，不需手動貼入程式。

- Arduino-ESP32 3.3.11；NimBLE-Arduino 2.5.1。
- ESP32C3 Dev Module；CPU 80 MHz；Flash 4 MB、DIO、80 MHz；Default Partition。
- 直接燒錄：`firmware/BRD_BBP_ESP32C3_4MB_0x000000.bin`，位址 `0x000000`。
- 完整 4 MB 映像會覆寫 NVS；需要保留歷史時，使用一般專案上傳並停用 Erase All Flash Before Sketch Upload。

## 新增、修改與刪減

| 標記 | 內容 |
|---|---|
| 新增 | 從附件移入 `oled_draw_bluetooth_icon()`，使用原生 7×13 逐列點陣。 |
| 修改 | 連線時呼叫附件圖示，位置 x119..125、y17..29。 |
| 刪減 | 移除 R3 的 7×7 點陣與 2×2 像素放大迴圈。 |

本次正式程式只修改 `brd_oled.cpp`。位置及完整檔案替換方式見 `CHANGELOG.md`，差異見 `docs/BRD_BBP_OLED_ICON_R4.patch`。

## 曲線規則

| 裝載後事件順序 | 本次固定參考 | 每筆曲線的整圈週期 |
|---|---|---|
| 正緣先觸發 | 正緣 | 本次正緣時間－前次有效正緣時間 |
| 負緣先觸發 | 負緣 | 本次負緣時間－前次有效負緣時間 |

第一個參考事件只建立基準；下一個有效同類事件取得整圈週期後，立即記錄。初始 GPIO 電位不決定參考，第一個有效 RPM 來自哪邊也不會改選。同一次量測固定使用相同參考；重新裝載或既有量測重置後才重新選擇。

兩種邊沿仍各用自己的整圈時間計算 `RPM = floor(60000000 / period_us)`。曲線只記錄參考邊沿的 `raw = round(period_us / 8)`，保存前 32 個有效完整週期，尾端補零；第 33 圈之後仍可更新 MAX。發射時保留裝載後的前段曲線。

範例：正緣先到，正緣週期 6000 us、負緣週期 8000 us。即時 RPM 仍會依序更新 10000、7500；曲線只保存正緣的 raw 750，每圈一筆。若負緣先到，則全程保存負緣週期。

連續有效取樣時，32 點代表 32 圈，逐點累加 `raw / 125.0` 得到毫秒時間軸，不再重複累加同一圈。週期仍有 8 us 量化；停頓、被丟棄的無效資料與 raw 飽和不會自動補成絕對時間戳。詳 `docs/SINGLE_REV_REFERENCE.md`。

History 代表 SP 保留整次雙邊沿 MAX RPM 的 BRD 測試映射，可與所選單圈曲線峰值不同，不宣稱等同原廠 SP 評分。發射後完成與至少 600 ms 的發布下限、32 格容量、17-byte BLE 封包均沿用。

## OLED 藍牙圖示

[修改] 完整採用附件 `BRD_BLE_OLED/brd_oled.cpp` 的 `oled_draw_bluetooth_icon()`。圖示為 7×13，位於 x119..125、y17..29；已建立 BLE 連線時顯示，斷線重畫時清除。五位 RPM/MAX 的最右像素為 x105，圖示不遮住文字、HOLD、電池或充電圖示。

預覽在 `docs/OLED_BLE_R4_Preview.png`，左為 R3，右為 R4。影像由實際繪圖函式輸出 framebuffer 後放大，不是 OLED 實板照片。本次已逐點比對，圖示輸出與附件相同。

## API 判定

依原報告相同 30 項條件，沿用 R3 的判定為 **29 項符合、0 項部分符合、1 項不符合**。第 24、26 項已恢復單圈資料與前 32 圈語意；第 27 項原建議「發射後起錄」，本版依你的要求保留「裝載後取得有效週期就起錄」。詳細比對在 `docs/BRD_BBP_API_Conformance_Review.md`。

註解：既有 NVS 曲線不重算，格式沒有 R2／R3 取樣模式欄位；R3 起的新 Shot 採單圈曲線，R4 沿用此規則。

## BLE 與 CMD

| 項目 | 值 |
|---|---|
| 名稱 | `BEYBLADE_TOOL01` |
| Service | `55c40000-f8eb-11ec-b939-0242ac120002` |
| 共用特徵 | `55c4f002-f8eb-11ec-b939-0242ac120002` |
| 特徵屬性 | Notify、Write、Write Without Response |
| 封包 | 固定 17 bytes，多 byte 值為 Little Endian |
| TX 功率 | 0 dBm |

Service 放主廣播，完整名稱放 Scan Response。共用 F002 接收命令是本包實作，原廠 Write UUID 仍未完全確認。

| 一個 byte 命令 | 行為 |
|---|---|
| `0x51` | 回傳一頁 A0。 |
| `0x74` | 依序送 B0~B7、70~73 共 12 頁；可重讀最新完整 Shot。 |
| `0x75` | 清除 RAM 歷史、曲線、MAX、Counter，作廢目前 Capture，安排保存；不增加自創 ACK。 |
| `0x61`／未知 | 安全忽略，不改資料。 |

預設模式 1 同時支援命令與事件通知：訂閱穩定 100 ms 後送 A0，已識別的 LOAD 改變送 A0，Shot 發布後送 12 頁。A0 byte3 在已裝載為 0x04、未裝載為 0x00。主機即使每 500 ms 輪詢，也可維持模式 1；主機須按 Header 與 Shot Counter 整理可能重複的結果。

`BBP_COMPAT_AUTONOTIFY=0` 只保留舊純輪詢及舊 flags 語意供回歸比對，**不符合預設模式的全部通知條件**。本包無固定每 500 ms 自動 Notify；10 ms 是待送頁面的排程間隔。

12 頁由一致快照產生，傳送中不插入另一批資料。Notify 提交失敗重試同一頁，持續 1.5 秒失敗斷線；32 筆命令佇列溢位也斷線。重新連線或訂閱不續傳舊批次。Notify 成功只表示交給 BLE stack，沒有應用層 ACK；缺頁由主機再送 0x74 整組重讀。

## 歷史與保存

最近 50 筆使用環形緩衝，輸出由舊到新。B6 offsets 7/9/11 分別為終身 MAX、累計次數、有效筆數；B7 byte16 是 B0~B6 的 16-byte payload 總和 mod 256。內部 Counter 為 uint32，BLE 欄位為 uint16，主機需處理 65535 後的回繞。

NVS 使用 `brd_bbp/history_v1`、181-byte version 1 blob 與 CRC32。保留長度、版本、CRC、數值範圍、環形索引及曲線尾端零值檢查；只移除曲線峰值必須等於代表 SP 的假設。舊版有效資料可以載入，既有曲線維持原內容，新收集規則從下一次 Shot 生效。若改回舊韌體，舊解碼器可能拒絕獨立代表值或空曲線的新資料；未承諾降版相容。

Flash 僅在既有安全時段保存。0x75 的 RAM 清除不表示 Flash 已寫入；寫入前突然斷電，尚未保存的結果或清除可能遺失。首次 NVS 載入失敗時不覆寫舊資料，BBP 暫停初始化並重試。無 BLE 連線仍可建立本地歷史。

## GPIO 與既有量測參數

| 功能 | GPIO／設定 |
|---|---|
| RPM IR | 3，INPUT、CHANGE、兩種邊沿各以整圈計時 |
| LOAD IR | 1，INPUT、CHANGE、HIGH=裝載 |
| 電池 ADC | 0，470k/470k 分壓、單次換算 |
| 充電偵測 | 10，INPUT_PULLUP、LOW=充電 |
| 充電 LED | 8 |
| OLED | SDA20／SCL21、外部上拉、128×32 SSD1306 |

LOAD 去抖 1000 us、發射後降至 MAX 的 20% 結束、無脈衝 300 ms 結束、MAX 自鎖 2.5 秒、OLED 更新 100 ms。既有低電與 ADC 錯誤保護沿用。Battery Raw 為 SOC 對應 0~250 的 BRD 映射，非原廠電池標定。

## 資料夾結構與驗證

| 位置 | 內容 |
|---|---|
| `BRD_BBP/BRD_BBP.ino` | Arduino 主程式 |
| `BRD_BBP/*.cpp`、`BRD_BBP/*.h` | 完整正式模組 |
| `BRD_BBP/CHANGELOG.md` | 新增／修改／刪減及行號 |
| `BRD_BBP/VALIDATION.md` | 本次實際測試、編譯結果與限制 |
| `BRD_BBP/docs/` | API、單圈參考說明、30 項比對、歷史報告、差異與驗證日誌 |
| `BRD_BBP/firmware/` | 本次完整 4 MB 燒錄檔與說明 |
| `BRD_BBP/tests/` | 7 組宿主測試及硬體替身 |

完整檔名清單見 `FILE_MANIFEST.txt`。在專案資料夾執行 `python3 tests/run_host_tests.py`；使用 g++ C++17、ASan/UBSan。這些測試包含真實協定、Session、量測與 BLE 排程程式，但硬體邊界使用替身，尚未執行實板、無線擷取及官方 App 驗證。
