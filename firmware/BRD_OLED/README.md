# BRD_OLED V1.1

來源：本次附件 `BRD_BPX(6).zip`。此為獨立的無藍牙 OLED 韌體版本。

## 使用方式

1. 解壓縮後保留 `BRD_OLED` 資料夾名稱。
2. 建議放置於 `C:\UserCode\ESP32\Arduino\BRD_OLED\`。
3. Arduino IDE 開啟 `C:\UserCode\ESP32\Arduino\BRD_OLED\BRD_OLED.ino`。
4. 同一資料夾必須保留全部根目錄 `.cpp`、`.h`，不要混入原 BRD_BPX 的檔案。
5. 在 `brd_config.h` 選擇模式後重新編譯及燒錄。無須安裝 NimBLE-Arduino。

沿用附件的目標設定：ESP32C3 Dev Module、80 MHz、4 MB Flash、DIO、Flash Frequency 80 MHz。
程式使用 ESP-IDF ADC oneshot 與 I2C master API；原專案的編譯期限制為 ESP-IDF 5.3 以上。

## 模式與量測設定

所有參數位於 `BRD_OLED/brd_config.h`，不存在無線設定服務或 Flash 設定載入。
修改此檔後須重新編譯燒錄；原 BRD_BPX 留在 Flash 的設定不會覆寫此版本。

| 設定 | 預設值 | 位置及用途 |
| --- | --- | --- |
| `BRD_MEASUREMENT_MODE` | `BRD_MEASUREMENT_MODE_LOAD` | 第 115 行。選 LOAD 或 `BRD_MEASUREMENT_MODE_AUTO` |
| `AUTO_RPM_THRESHOLD` | 2000 RPM | 第 117 行。AUTO 有效量測門檻 |
| `POST_LAUNCH_FINISH_PERCENT` | 30 | 第 131 行。即時 RPM 嚴格低於 MAX 的 30% 時結束 |
| `LOAD_IR_DEBOUNCE_US` | 50000 us | LOAD 連續穩定 50 ms |
| `OLED_MAX_HOLD_MS` | 2500 ms | MAX 畫面至少保持 2.5 秒 |
| `RPM_ZERO_TIMEOUT_MS` | 300 ms | 無有效脈衝時的結算備援 |
| `AUTO_RESET_ZERO_MS` | 250 ms | AUTO 未達門檻時的歸零時間 |

LOAD 模式：LOAD 穩定 HIGH 後歸零並開始量測；有效旋轉開始後，LOAD 改變不重置 MAX，也不決定發射成功。結束條件由 RPM/MAX 比例與無脈衝備援決定。

AUTO 模式：不掛載 LOAD 中斷，不以 LOAD 決定開始或結束。RPM 曾達 2000 RPM 後，採相同的 MAX 比例及無脈衝備援；未達門檻的短暫轉動歸零，不顯示有效 MAX 結果。

沿用附件的數值，不使用其他歷史版本的 20% 或 100 ms 設定。

## OLED 顯示

開機第一行大字 `BRD OLED`，第二行小字 `FW VER V1.1`。

| 階段 | 第一行 | 第二行 |
| --- | --- | --- |
| LOAD 尚未裝載 | WAIT LOAD | RPM 數值 |
| LOAD 已裝載 | LOADED READY | RPM 數值 |
| AUTO 待命或量測 | AUTO READY | RPM 數值 |
| MAX 自鎖 | HOLD | MAX 數值 |
| 自鎖結束 | 依模式及裝載顯示 | MAX 保留到下次量測開始 |

保留電池、充電、低電警示及 ADC ERR。移除藍牙連線圖示與 BLE/OFF 文字。

## 資料夾與完整檔案

| 完整專案路徑 | 變更及貼放方式 |
| --- | --- |
| `BRD_OLED/BRD_OLED.ino` | 新增獨立主程式，直接開啟，不須貼到舊主程式 |
| `BRD_OLED/brd_config.h` | 修改名稱及編譯期設定，完整檔案 |
| `BRD_OLED/brd_measurement.cpp` | 修改設定來源，刪減協議與 Flash 窗口，完整檔案 |
| `BRD_OLED/brd_measurement.h` | 刪減通訊設定專用介面，完整檔案 |
| `BRD_OLED/brd_oled.cpp` | 修改開機名稱，刪減連線圖示及相依，完整檔案 |
| `BRD_OLED/brd_oled.h` | 保留顯示介面，完整檔案 |
| `BRD_OLED/brd_battery.cpp` | 電池演算法保留，修正路徑及開機門檻註解，完整檔案 |
| `BRD_OLED/brd_battery.h` | 保留電池介面，完整檔案 |
| `BRD_OLED/brd_io.cpp` | 保留 GPIO 初始化與充電讀取，完整檔案 |
| `BRD_OLED/brd_io.h` | 保留 GPIO 介面，完整檔案 |
| `BRD_OLED/tests/` | 主機端測試及硬體替身，Arduino 不須使用此資料夾 |
| `BRD_OLED/VALIDATION.md` | 本次已執行的驗證及限制 |
| `BRD_OLED/FILE_MANIFEST.txt` | 壓縮檔內完整檔案清單 |

每個修改檔案的註解已標示新增、修改、刪減與保留項目。

## 刪減檔案

此版本不包含 `brd_bbp.cpp/.h`、`brd_bbp_protocol.cpp/.h`、`brd_bbp_session.cpp/.h`、`brd_settings.cpp/.h`、`brd_settings_ble.cpp/.h`、`brd_settings_model.cpp/.h`。

同時移除 UUID、廣播、連線、Notify、封包重試、曲線/歷史儲存、設定 API、充電關閉藍牙策略及其 OLED 提示。
不帶入來源的 `build/`、舊 `.bin/.elf/.map`，避免混淆不同版本。

## 已保留的硬體設定

| 功能 | 腳位 |
| --- | --- |
| RPM 輸入 | GPIO3 |
| LOAD 輸入 | GPIO1，HIGH 為裝載 |
| 電池 ADC | GPIO0，470k/470k 分壓 |
| 充電偵測 | GPIO10，LOW 為充電，內部上拉 |
| OLED SDA/SCL | GPIO20/GPIO21，128x32 SSD1306，0x3C |
| LED | GPIO8，沿用附件固定關閉 |

RPM、LOAD、ADC 與 I2C 不啟用內部上下拉；充電偵測為例外。
電池開機門檻、運行低電鎖定、恢復穩定判斷與 ADC 警示均保留來源行為。

## 驗證

已通過六個主機端測試執行檔，詳見 `VALIDATION.md`。
此環境無 ESP32 工具鏈，尚未執行 ESP32 目標編譯與實機燒錄測試；壓縮檔僅提供完整原始碼。
