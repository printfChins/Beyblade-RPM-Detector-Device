# BRD_OLED — V0.10

本專案由使用者提供的 `BLE_RPM_V1.9(2).zip` 製作為 **V0.10 無藍牙版本**，保留 LOAD、RPM 量測、OLED 顯示，以及電量與充電圖示。本次以最新附件 `BRD_OLED(1).zip` 為基礎，新增發射後 MAX 自鎖期間的 `HOLD` 顯示。電量低於 5% 時停用鎖定，鎖定後恢復至 10% 或以上會重新開機。版本字串維持 `V0.10`。

## 使用位置

解壓縮後，使用 Arduino IDE 開啟 `BRD_OLED/BRD_OLED.ino`。主檔名稱必須與資料夾名稱一致。

這是一個完整、獨立的新版本資料夾。各檔案均從第 1 行使用完整內容，不需要將片段貼入 V1.9。不要將 V1.9 的 `brd_ble.cpp` 或其他舊模組混放進此資料夾，因為 Arduino 會一起編譯同資料夾內的原始碼。

若放在 Windows 的 `C:\UserCode\BRD-Firmware\firmware`，主程式完整位置即為：

`C:\UserCode\BRD-Firmware\firmware\BRD_OLED\BRD_OLED.ino`

## 本次 HOLD 顯示修改位置

完整專案的資料夾為 `BRD_OLED/`，版本字串維持 `V0.10`。若更新既有專案，以下三個完整檔案必須一起替換，均從第 1 行使用交付內容；主程式 `BRD_OLED.ino` 不需修改。

| 檔案位置 | 本次變更 |
| --- | --- |
| `BRD_OLED/brd_measurement.h` | [新增] `brd_display_t.hold_active`，提供實際自鎖狀態 |
| `BRD_OLED/brd_measurement.cpp` | [新增] 在畫面資料中填入 HOLD 狀態 |
| `BRD_OLED/brd_oled.cpp` | [新增] H 字型、HOLD 文字與自鎖狀態變更後的畫面更新；[刪減] 自鎖期間只按 LOAD 顯示文字的舊判斷 |

| 狀態 | 第一行 | 第二行 |
| --- | --- | --- |
| 發射後仍在量測 | `WAIT LOAD` | `RPM xxxx` |
| 量測完成且正在自鎖 | `HOLD` | `MAX xxxx` |
| 自鎖結束、尚未重新裝載 | `WAIT LOAD` | `MAX xxxx` |
| 重新裝載且去抖完成 | `LOADED READY` | `RPM 0`，開始轉動後更新即時轉速 |

- HOLD 對應既有 MAX 自鎖旗標，並非 MAX 仍可見的整段期間。
- 自鎖時間沿用本次附件的 `OLED_MAX_HOLD_MS = 2500`，即 2.5 秒；仍從首幅完整 MAX 畫面送出時計時。OLED 失效時沿用原有備援計時。
- 自鎖中忽略 LOAD，電量與充電圖示依原有機制更新；圖示重繪不延長自鎖。
- 自鎖狀態改變後要求下一幅畫面更新，一般畫面仍分段傳送；螢幕文字實際切換會經過傳送時間。
- 低電警示優先於 HOLD，沿用圓角電池、左側亮條與中央閃電圖示。
- [刪減] 交付 ZIP 排除附件內修改前的 `build/`、BIN、ELF、MAP 等建置產物。請在 Arduino IDE 重新編譯及燒錄，以產生包含 HOLD 的韌體。

## 先前低電功能的主程式新增位置

檔案：`BRD_OLED/BRD_OLED.ino`。以下行號以 ZIP 內更新後的完整主程式為準。

| 行號 | 新增內容 | 放置位置 |
| ---: | --- | --- |
| 16 | `#include <esp_system.h>` | [新增] 標頭區：引入軟體重啟介面 |
| 29 | `brd_battery_begin();` | [保留] setup：GPIO 初始化之後、OLED 初始化之前先取樣 |
| 35 | `brd_battery_update();` | [新增] setup：版本畫面結束後重新檢查電量 |
| 36 | `if (brd_battery_is_low_locked())` | [新增] setup：低電時禁止啟動 RPM／LOAD 中斷 |
| 45 | `brd_battery_update();` | [修改] loop：電量判斷移至量測處理之前 |
| 47 | `if (brd_battery_is_low_locked())` | [新增] loop：低電鎖定優先於一般功能 |
| 49 | `brd_measurement_stop();` | [新增] 低電分支：停用中斷並清除量測資料 |
| 52 | `esp_restart();` | [新增] 低電分支：電量恢復至 10% 或以上時重啟 |
| 54 | `brd_oled_update();` | [新增] 低電分支：只更新沒電警示，之後直接返回 |

## 完整資料夾與檔案

所有檔案均直接放在 `BRD_OLED/`，沒有額外的原始碼子資料夾。

| 相對檔案位置 | 用途與變更 |
| --- | --- |
| `BRD_OLED/BRD_OLED.ino` | [修改] 完整主程式；初始化與量測、電池、OLED 呼叫順序 |
| `BRD_OLED/brd_config.h` | [修改] 版本、GPIO、RPM、電池 ADC、低電與恢復門檻、LOAD 去抖、MAX 自鎖與 OLED 重試參數 |
| `BRD_OLED/brd_battery.cpp` | [修改] GPIO0 每秒單次 ADC、原版 SOC 表、低電鎖定與恢復判斷 |
| `BRD_OLED/brd_battery.h` | [新增] 低電鎖定及重啟條件介面；保留電量讀取介面 |
| `BRD_OLED/brd_io.cpp` | [修改] 充電 DET 使用上拉，其餘輸入腳無上下拉；GPIO8 關燈 |
| `BRD_OLED/brd_io.h` | [修改] GPIO 初始化與充電 DET 讀取介面 |
| `BRD_OLED/brd_measurement.cpp` | [新增] 提供 HOLD 顯示旗標；保留低電停用、單機量測、去抖、MAX 保持 |
| `BRD_OLED/brd_measurement.h` | [新增] 畫面資料中的 hold_active；保留量測介面 |
| `BRD_OLED/brd_oled.cpp` | [新增] HOLD 狀態顯示與 H 字型；保留圓角電池低電警示、電量、充電圖示與 I2C 重試 |
| `BRD_OLED/brd_oled.h` | [修改] OLED 初始化與更新介面 |
| `BRD_OLED/README.md` | [新增] 使用位置、功能與設定說明 |
| `BRD_OLED/CHANGELOG.md` | [新增] 相對附件 V1.9 的修改、刪減清單 |
| `BRD_OLED/VALIDATION.md` | [新增] 已執行的主機模擬檢查與驗證限制 |
| `BRD_OLED/FILE_MANIFEST.txt` | [新增] 交付檔案清單與 SHA-256 |

## GPIO

| 功能 | GPIO | 韌體設定與行為 |
| --- | ---: | --- |
| RPM IR | 3 | INPUT；內部上拉、下拉皆關閉；FALLING 中斷，每下降緣代表一圈 |
| LOAD IR | 1 | INPUT；內部上拉、下拉皆關閉；CHANGE 中斷；HIGH 已裝載、LOW 未裝載 |
| 電池 ADC | 0 | ADC；內部上拉、下拉皆關閉；12-bit、11 dB、470k/470k 分壓 |
| 充電偵測 | 10 | INPUT_PULLUP；內部上拉開啟、下拉關閉；LOW 顯示充電圖示，HIGH 隱藏 |
| 原狀態 LED | 8 | OUTPUT；固定 HIGH 關燈 |
| OLED SDA | 20 | I2C；初始化與復原皆不啟用內部上拉、下拉 |
| OLED SCL | 21 | I2C；初始化與復原皆不啟用內部上拉、下拉 |

GPIO3、GPIO1、GPIO0 在 `brd_io.cpp` 以 `INPUT` 設定，並呼叫 `gpio_set_pull_mode(..., GPIO_FLOATING)`；GPIO0 隨後由電池模組初始化 ADC，維持無內部上下拉。GPIO10 充電 DET 依使用者修正要求例外使用 `INPUT_PULLUP` 與 `GPIO_PULLUP_ONLY`，符合原電路 LOW 充電、滿電高阻的配置。其模式參數為 `brd_config.h` 的 `CHRG_DET_INPUT_MODE`；充電狀態提供給 OLED 的閃電圖示。

I2C 使用原生 ESP-IDF master driver，明確指定 `enable_internal_pullup = false`，避免 `Wire.begin()` 初始化流程啟用內部上拉。此設定涵蓋本專案使用的訊號腳；不重設 Flash、USB 或其他未使用的晶片腳位。

SDA/SCL 仍需要電路或 OLED 模組上的外部上拉電阻；本版關閉的是 MCU 內部上拉、下拉。GPIO1/3 的有效 HIGH/LOW 由外部電路提供。

## 操作與顯示

1. 上電先讀取電量。低於 5% 時直接進入沒電警示；其餘情況在成功初始化 OLED 後顯示 `BRD` 與 `V0.10`，維持 1500 ms，再次檢查電量後才啟動量測。
2. 未裝載時第一行顯示 `WAIT LOAD`，第二行顯示 `RPM 0`。
3. LOAD 連續穩定 HIGH 通過去抖後，第一行顯示 `LOADED READY`，清除上次 MAX。
4. 第一個 RPM 下降緣建立時間基準；第二個有效下降緣開始依 `60000000 / period_us` 計算 RPM。
5. 裝載期間顯示即時 RPM。LOAD 穩定變 LOW 後進入發射後量測，顯示 `WAIT LOAD` 與即時 RPM。
6. 發射後 RPM 降至本輪 MAX 的 50% 或以下即結束；如果脈衝停止，300 ms timeout 後結束。至少一筆有效 RPM 即可保留 MAX；空發射不建立假數值。
7. 完成後第二行顯示 `MAX xxxx`。本版沿用 V1.9 的 `MAX` 標籤與字體大小，數值單位為 RPM，最高顯示範圍為 60000。
8. 自鎖期間第一行顯示 `HOLD`，第二行保留 `MAX xxxx`；MAX 完整畫面送出後依目前設定保持 2.5 秒，期間不處理 LOAD。解鎖後重新讀取實際 LOAD 並重新計算去抖。若仍未裝載，MAX 持續保持；若已裝載，去抖通過後清零。

上述流程為正常模式；低電鎖定可以立即中止任一量測步驟或 MAX 自鎖。裝載中停止轉動時，300 ms 後即時 RPM 歸零；未發射且停止滿三秒後，回到等待本次旋轉。CPU 固定 80 MHz，沒有自動關閉 OLED 或 Deep-sleep。

## 低電警示與恢復重啟

| 當前狀態 | 取樣後的整數電量 | 行為 |
| --- | --- | --- |
| 上電／正常運作 | 小於 5% | 鎖定一般功能，停用 GPIO3 RPM 與 GPIO1 LOAD 中斷，清除事件佇列、即時 RPM、MAX 與自鎖 |
| 尚未觸發低電鎖定 | 5% 或以上 | 允許正常運作；恰好 5% 不觸發 |
| 已低電鎖定 | 小於 10% | 持續顯示沒電圖示；回升到 5%～9% 仍不恢復量測 |
| 已低電鎖定 | 10% 或以上 | 呼叫 `esp_restart()` 重新啟動設備，再依開機電量檢查啟動量測 |

- 每 1 秒單次取樣；一旦該次取樣換算出低於 5%，當輪 loop 先停用量測，再更新警示。判斷間隔由 `BATTERY_SAMPLE_INTERVAL_MS` 控制。
- OLED 清除原畫面，中央只顯示圓角電池、左側短條與閃電；單色 OLED 將參考圖紅條呈現為亮色像素。警示優先於未送完的一般畫面及 HOLD 自鎖期間。
- 鎖定期間不執行 RPM、LOAD、MAX 或一般電量／充電圖示更新；只保留恢復判斷所需的 ADC、OLED 警示及必要的系統排程。這是韌體停用，沒有控制外部電源切斷。
- 上電就低於 5% 時不播放版本畫面、不掛載量測中斷。正常開機畫面等待結束後也重新檢查電量，避免其間電量下降仍啟動量測。
- OLED 故障不影響低電停用或達到 10% 後的重啟。顯示器復原後依目前鎖定狀態重畫警示。
- 恢復條件以 ADC 換算的電量為準，不以插入充電器或 GPIO10 的充電狀態作為立即解除條件。軟體鎖定狀態保存於本次上電期間。

門檻比較使用 SOC 表插值及四捨五入後的整數百分比，與一般電池圖示使用相同資料；不是獨立的固定電壓比較器。

## 電量與充電圖示

- 電池圖示恢復至 OLED 右上角 `(108, 0)`，17x9 像素本體與 3x5 端子；內部填滿寬度依原版 Li-Po 電量估算值變化。
- 充電時於電池左側 `(100, 1)` 顯示原版 5x7 閃電圖示；停止充電時清除圖示。充電中仍顯示實際估算電量，不固定填滿電池。
- 電量與充電圖示在 `WAIT LOAD`、`LOADED READY`、即時 RPM 與 MAX 畫面均可顯示，位置與原版 V1.9 相同。
- 上電先取得第一筆 ADC；正常與低電模式均每 1 秒只呼叫一次 `analogReadMilliVolts(GPIO0)`，使用 470k/470k 分壓換算電池電壓，再依 V1.9 的 SOC 表插值。
- 保留原版不使用 dummy conversion、取樣平均或 IIR 濾波的設定。校正倍率預設 `1000 / 1000`，可於 `brd_config.h` 調整。
- 正常模式的電量更新與充電狀態切換會要求下一幅畫面更新；已開始的畫面仍完整送完，避免混合兩個狀態。低電警示例外，會優先取代一般畫面。
- MAX 自鎖期間仍更新電量與充電圖示；這些重繪不會清除 MAX 或重新計算自鎖起點。

### 沿用的鋰電池電量曲線

以下為 `brd_battery.cpp` 內的電壓估算表；相鄰點之間線性插值並四捨五入，範圍限制為 0%～100%。這是原專案的估算曲線，沒有更換為新電芯的實測曲線。

| 電池電壓（mV） | 電量（%） |
| ---: | ---: |
| 3300 | 0 |
| 3500 | 5 |
| 3600 | 10 |
| 3700 | 20 |
| 3750 | 30 |
| 3800 | 40 |
| 3850 | 50 |
| 3900 | 60 |
| 3950 | 70 |
| 4000 | 80 |
| 4050 | 85 |
| 4100 | 90 |
| 4150 | 95 |
| 4200 | 100 |

## 可調參數

所有參數位於 `BRD_OLED/brd_config.h`，LOAD 去抖與 MAX 自鎖放在同一區塊。

| 參數 | 預設值 | 單位與用途 |
| --- | ---: | --- |
| `LOAD_IR_DEBOUNCE_US` | 1000 | us；最新 LOAD 邊沿後連續穩定時間 |
| `OLED_MAX_HOLD_MS` | 2500 | ms；MAX 完整畫面送出後最短自鎖時間，設定需至少 2000 |
| `OLED_RETRY_INTERVAL_MS` | 1000 | ms；OLED 初始化或 I2C 傳送失敗後的重試間隔 |
| `OLED_I2C_TIMEOUT_MS` | 10 | ms；單次 I2C 傳送等待上限參數 |
| `OLED_UPDATE_INTERVAL_MS` | 100 | ms；一般畫面更新間隔 |
| `OLED_BOOT_VERSION_DISPLAY_MS` | 1500 | ms；上電版本畫面保持時間 |
| `BATTERY_SAMPLE_INTERVAL_MS` | 1000 | ms；正常與低電模式的單次 ADC 取樣間隔 |
| `BATTERY_LOW_STOP_PERCENT` | 5 | %；低於此值觸發停用鎖定 |
| `BATTERY_RECOVER_PERCENT` | 10 | %；鎖定後達到此值或以上重新開機 |
| `BATTERY_LOW_LOOP_DELAY_MS` | 20 | ms；低電分支每輪讓出 CPU 的等待時間 |
| `BATTERY_LOW_OLED_REFRESH_MS` | 1000 | ms；沒電圖示成功送出後的重繪間隔 |
| `RPM_ZERO_TIMEOUT_MS` | 300 | ms；無有效脈衝後即時轉速歸零及發射後結束 |
| `PRELAUNCH_IDLE_RESET_MS` | 3000 | ms；裝載中未發射且停止時重新等待旋轉 |
| `POST_LAUNCH_NO_RPM_TIMEOUT_MS` | 1200 | ms；沒有形成有效 RPM 的空發射等待上限 |

OLED 一般畫面分成數個 I2C 傳送，每個 loop 最多送一個一般畫面封包，讓主迴圈能先處理 RPM 與 LOAD。畫面初始化或故障復原會包含 probe、初始化指令；不在量測中重播開機等待畫面。

低電警示在量測中斷已停用後一次送完整幅畫面，之後每秒重繪。每個 I2C 封包仍有等待上限，任一包失敗即返回，交由原有重試流程恢復。

OLED 失敗時每秒嘗試 I2C 復原及 SSD1306 重新初始化；沒有控制 OLED 電源或 RESET 的額外 GPIO，因此此機制不是硬體斷電重啟。正常模式下 OLED 持續失敗時，量測繼續執行；MAX 自鎖以量測完成時間作為解鎖備援。若 OLED 稍後恢復且結果仍存在，首幅完整 MAX 畫面送出後依 `OLED_MAX_HOLD_MS` 重新計時。低電模式下量測持續停用，不因顯示器故障而恢復。

## Arduino 設定

- 開發板：`ESP32C3 Dev Module`。
- ESP32 board package：使用支援 ESP-IDF 5.3 或更新版本的 Arduino-ESP32；原附件建置紀錄為 3.3.11，本專案以原附件的環境為使用基準。
- CPU Frequency：80 MHz；程式亦呼叫 `setCpuFrequencyMhz(80)`。
- Flash Size：沿用原附件 4 MB。
- Flash Mode：沿用原附件 DIO。
- 不需要安裝 NimBLE-Arduino、Adafruit SSD1306 或其他外部 OLED 函式庫。
- 本版使用 Arduino / ESP-IDF 的 C++ 介面，因此保留 `.ino`、`.cpp`、`.h` 結構。
- ZIP 僅包含完整原始碼與文件，不包含舊版或未驗證的 BIN；請用 Arduino IDE 編譯後上傳。

## API 依據

- [Espressif ESP32-C3 I2C 文件](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32c3/api-reference/peripherals/i2c.html)：I2C bus/device 初始化、內部上拉開關、probe、transmit 與 bus reset。
- [Arduino-ESP32 I2C HAL 原始碼](https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/esp32-hal-i2c.c)：框架 I2C 初始化的內部上拉設定。
- [Espressif ESP32-C3 系統 API 文件](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/system/misc_system_api.html)：`esp_system.h` 與 `esp_restart()` 軟體重啟。
