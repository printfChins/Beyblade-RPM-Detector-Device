# BRD_OLED V0.12

```cpp
/*
本版以 BRD_OLED V0.11 為基礎，依使用者指定範圍修改。
[修改] 發射後的 RPM 結束門檻由 MAX 50% 改成 MAX 20%。
[新增] 有界 LOAD 邊沿佇列、事件時間去抖與 RPM 事件排序。
[新增] 單次 ADC 的錯誤碼、診斷狀態、有效資料逾時及持續達標恢復。
[保留] 不新增相鄰 RPM 週期合理性檢查；重新開機仍清除低電鎖定。
[修改] 版本、文件與交付 SHA-256 同步為 V0.12。
*/
```

解壓縮後，將完整 `BRD_OLED/` 資料夾放入原專案的 `firmware/`。沿用原建置紀錄中的位置，主程式完整路徑為：

`C:\UserCode\ESP32\Arduino\Beyblade-RPM-Detector-Device\firmware\BRD_OLED\BRD_OLED.ino`

```cpp
/*
貼放方式：各 .cpp / .h 均提供完整檔案，由第 1 行完整取代同名檔案。
主程式已在 ZIP 中整合；若手動合併，依下方主程式修改位置表處理。
Arduino IDE 開啟 BRD_OLED.ino，主檔名稱需與 BRD_OLED 資料夾一致。

[刪減] 交付包不含原 V0.11 的 build/、APP BIN、merged BIN、ELF 或 MAP。
更新既有目錄時，請清除舊建置產物，重新編譯後再燒錄。
不能將 V0.11 BIN 改名當作 V0.12，亦不能以 APP BIN 代替 0x000000 merged 映像。
*/
```

完整交付結構與用途：

| 壓縮檔內路徑 | 用途／本次變更 |
| --- | --- |
| `BRD_OLED/BRD_OLED.ino` | [修改] 啟動／暫停條件加入 ADC 有效性；[新增] 故障恢復後重新待測 |
| `BRD_OLED/brd_config.h` | [修改] V0.12、MAX 20%；[新增] LOAD 佇列與 ADC 故障／恢復時間 |
| `BRD_OLED/brd_measurement.cpp` | [修改] 按事件時間合併 LOAD、RPM 與到期事件；[新增] 有界佇列、溢位復原 |
| `BRD_OLED/brd_measurement.h` | [新增] 量測診斷介面；[修改] begin 可重複呼叫 |
| `BRD_OLED/brd_battery.cpp` | [修改] ESP-IDF 單次 ADC 與校正；[新增] 錯誤狀態、有效資料逾時、恢復計時 |
| `BRD_OLED/brd_battery.h` | [新增] 電池診斷、ADC 故障及量測允許狀態介面 |
| `BRD_OLED/brd_oled.cpp` | [新增] ADC ERR 畫面與 C 字型；[修改] 警示與一般畫面切換 |
| `BRD_OLED/brd_oled.h` | OLED 既有介面；更新檔案位置註解 |
| `BRD_OLED/brd_io.cpp`、`BRD_OLED/brd_io.h` | GPIO 功能不變；更新檔案位置註解 |
| `BRD_OLED/README.md` | V0.12 操作、設定、介面及貼放位置 |
| `BRD_OLED/CHANGELOG.md` | V0.12 新增／修改／刪減紀錄及既有版本歷史 |
| `BRD_OLED/VALIDATION.md` | 本次實際完成的驗證與限制 |
| `BRD_OLED/FILE_MANIFEST.txt` | 包含韌體、文件及 verification 的檔案 SHA-256 |
| `verification/run_host_tests.py` | [新增] 主機回歸測試入口，需 Python 3 與支援 sanitizer 的 g++ |
| `verification/host/test_v012.cpp` | [新增] 38 個測試案例，直接載入交付原始模組與主程式 |
| `verification/host/Arduino.h`、`esp_err.h`、`esp_idf_version.h`、`esp_system.h` | [新增] 主機替代介面，全部位於 verification/host/ |
| `verification/host/driver/gpio.h`、`driver/i2c_master.h` | [新增] GPIO／I2C 替代介面，全部位於 verification/host/ |
| `verification/host/esp_adc/adc_oneshot.h`、`adc_cali.h`、`adc_cali_scheme.h` | [新增] ADC 替代介面，全部位於 verification/host/esp_adc/ |
| `verification/host/hal/adc_types.h`、`soc/gpio_struct.h` | [新增] 主機型別與 GPIO 暫存器模型 |

`verification/` 與 `BRD_OLED/` 並列，不要把測試用標頭放進 Arduino 的程式或 libraries 目錄。

主程式修改位置；行號對應本版 `BRD_OLED/BRD_OLED.ino`：

| 行號 | 位置 | 新增／刪減 |
| ---: | --- | --- |
| 37 | setup，電池再次更新之後 | [修改] 改以 `!brd_battery_measurement_allowed()` 判斷暫停；[刪減] 僅檢查低電鎖定的條件 |
| 49 | loop，電池更新之後 | [修改] 相同允許條件，涵蓋低電與 ADC 無有效資料 |
| 52 | 暫停分支 | [保留呼叫] `brd_battery_restart_required()`；其內部已改成持續達標後才回傳 true |
| 62 | 正常分支，`brd_measurement_update()` 前 | [新增] `brd_measurement_begin()`，僅在停用後恢復時重新建立量測；已啟用時不重置 |

設定集中於 `BRD_OLED/brd_config.h`：

| 參數 | V0.12 預設 | 行為 |
| --- | ---: | --- |
| `PROJECT_VERSION` | V0.12 | 開機版本字串 |
| `POST_LAUNCH_FINISH_PERCENT` | 20 | 發射後 RPM <= MAX 的 20% 即結算，仍是單筆有效 RPM 判斷 |
| `LOAD_ISR_QUEUE_SIZE` | 32 | 環形佇列實際保存 31 個 LOAD 邊沿 |
| `LOAD_IR_DEBOUNCE_US` | 1000 us | 依邊沿時間確認連續穩定狀態 |
| `OLED_MAX_HOLD_MS` | 2500 ms | 首幅完整 MAX 畫面送出後的 HOLD 時間 |
| `BATTERY_SAMPLE_INTERVAL_MS` | 1000 ms | 每個週期最多一次 ADC 轉換 |
| `BATTERY_ADC_STALE_TIMEOUT_MS` | 3000 ms | 最後有效資料達此年齡，暫停量測並顯示 ADC ERR |
| `BATTERY_RECOVER_STABLE_MS` | 3000 ms | 低電鎖定後，有效電量連續 >=10% 的確認時間 |
| `BATTERY_RECOVER_MAX_GAP_MS` | 1500 ms | 由取樣間隔的 1.5 倍推得；超過此間隔即重算恢復時間 |
| `BATTERY_LOW_STOP_PERCENT` | 5 | 有效讀值低於 5% 立即鎖定；恰好 5% 可運作 |
| `BATTERY_RECOVER_PERCENT` | 10 | 低電恢復的電量門檻 |
| `RPM_ZERO_TIMEOUT_MS` | 300 ms | 無有效脈衝後歸零；已發射且有有效 RPM 時結算 |
| `POST_LAUNCH_NO_RPM_TIMEOUT_MS` | 1200 ms | 已發射但沒有有效 RPM 的等待上限 |
| `PRELAUNCH_IDLE_RESET_MS` | 3000 ms | 裝載中停止旋轉後重新等待 |
| `OLED_UPDATE_INTERVAL_MS` | 100 ms | 一般畫面啟動更新間隔；分段傳送 |
| `CPU_FIXED_FREQ_MHZ` | 80 MHz | 沿用原設定 |

```cpp
/*
LOAD 事件處理：

ISR 保存 time_us 與 level。主 loop 以固定大小快照取得兩個佇列，
將 LOAD / RPM 邊沿與去抖期限、歸零及結算期限依時間順序處理。
不使用 heap；快照也不放在主迴圈堆疊。

例如主 loop 延遲期間 LOW 已維持 2 ms，再回 HIGH，
現在仍會辨識該次 LOW，而非只保留最後 HIGH。
短於 1 ms 的來回變化仍由去抖排除；恰好 1 ms 的狀態會成立。
相同時間戳記的 RPM 優先於 LOAD 邊沿；同時刻的 RPM 先於歸零期限處理。

LOAD 佇列溢位：缺失的裝載歷史不能推測，進行中的量測作廢，
重新讀取實際 LOAD 並完成一次去抖。已結算的 MAX 可保留到下次有效裝載。
RPM 佇列溢位：丟棄不完整批次並重新建立週期，不以缺失脈衝計算假性低 RPM。
兩者均累計診斷次數。HOLD 與停用期間不保存新的歷史事件。
*/
```

ADC 與低電狀態對照：

| 狀態 | 電量資料／畫面 | 量測與恢復 |
| --- | --- | --- |
| 開機尚未取得有效 ADC | `ADC ERR` | 不啟動量測；每秒重試。有效 >=5% 後自動待測 |
| 有最後有效資料，暫時讀取失敗且資料尚未滿 3 秒 | 保留最後有效電量，錯誤由診斷介面取得 | 可繼續量測；不以錯誤資料觸發低電 |
| 最後有效資料達 3 秒未更新 | `ADC ERR`；診斷仍保留最後有效值 | 清除量測結果並停用 RPM／LOAD 中斷；有效資料恢復後重新待測 |
| 有效電量 <5% | 原大型低電圖示 | 立即進入低電鎖定，優先於 HOLD |
| 已低電鎖定，電量 5%～9% | 原大型低電圖示 | 持續鎖定，恢復計時歸零 |
| 已低電鎖定，每筆有效電量 >=10% 且持續滿 3 秒 | 原大型低電圖示，隨後重新開機 | 呼叫 `esp_restart()` |
| 恢復期間讀取失敗、電量 <10% 或取樣間隔 >1500 ms | 原大型低電圖示 | 恢復計時歸零 |
| 已低電鎖定，又發生 ADC 故障 | 原大型低電圖示優先，故障可從診斷介面讀取 | 不能以失敗或過期資料觸發重啟 |

```cpp
/*
恢復時間以成功樣本確認：預設每秒一次，從第一筆 >=10% 的有效樣本起算，
需要在 0、1、2、3 秒取得四筆有效且達標的樣本，才算持續滿 3 秒。
未取樣的時間不會單獨使恢復條件成立。

ADC 為原生 oneshot 單次 raw 讀取，再使用 curve fitting 校正為 mV。
校正、分壓與 SOC 換算沒有增加 ADC 轉換次數。
ADC_ATTEN_DB_12 使用原 ADC_11db 相同的硬體衰減檔位。
資料只有在讀取、校正和數值檢查全部成功時才更新。
ESP_OK 且有效 0 mV 仍視為低電，不能把真實低電當成讀取失敗。

硬體雜訊若產生一筆 API 回報成功的錯誤電壓，仍可能影響電量判斷。
本版未加入取樣平均或 IIR，也未宣稱可以辨認所有類比雜訊。
重新開機仍清除低電鎖定；本版未新增跨重啟保存。
*/
```

診斷介面位於完整 `brd_battery.h`、`brd_measurement.h`；由主 loop 呼叫，不在 ISR 使用。沒有新增 Serial 或占用 GPIO20／21 的 UART 初始化。

| 介面／欄位 | 說明 |
| --- | --- |
| `brd_battery_get_diagnostics()` | 回傳電池診斷快照 |
| `status` | NOT_SAMPLED、OK、INIT_ERROR、READ_ERROR、CALIBRATION_ERROR 或 DATA_ERROR |
| `last_attempt_error` | 最近一次嘗試的 `esp_err_t`；成功為 ESP_OK |
| `last_failure_error`、`last_failure_ms` | 最近一次失敗的錯誤碼與時間；成功後仍保留 |
| `last_attempt_ms`、`last_success_ms` | 最近嘗試與最近成功的時間 |
| `success_count`、`failure_count`、`consecutive_failures` | 成功、失敗及連續失敗次數；計數器飽和不回繞 |
| `last_valid_raw`、`last_valid_adc_mv` | 最後有效 raw 與 ADC 校正電壓 |
| `last_sample_valid`、`has_valid_sample` | 最近樣本是否有效、是否曾取得有效樣本 |
| `adc_fault_active` | 從未有效或有效資料逾時；故障保持到下一次有效樣本 |
| `recovery_pending`、`recovery_elapsed_ms` | 恢復候選是否仍有效、已由有效樣本確認的持續時間 |
| `brd_measurement_get_diagnostics()` | 回傳量測診斷快照 |
| `load_queue_overflows`、`rpm_queue_overflows` | 佇列溢位批次數 |
| `load_stable_transitions`、`launch_events` | 已通過去抖的 LOAD 變化次數、辨識的發射事件次數 |

GPIO 與功能：

| 功能 | GPIO | 設定 |
| --- | ---: | --- |
| RPM IR | 3 | INPUT、FALLING，每下降緣一圈，無內部上下拉 |
| LOAD IR | 1 | INPUT、CHANGE，HIGH 裝載，無內部上下拉 |
| 電池 ADC | 0 | 12-bit，470k／470k 分壓，無內部上下拉 |
| 充電 DET | 10 | INPUT_PULLUP，LOW 表示充電 |
| LED | 8 | 沿用原程式，固定 HIGH 關燈 |
| OLED SDA／SCL | 20／21 | SSD1306 128x32、0x3C、400 kHz，使用外部上拉 |

Arduino IDE 使用 `ESP32C3 Dev Module`、80 MHz、4 MB Flash、DIO。原專案建置紀錄的 Arduino-ESP32 套件為 3.3.11；本版 API 需求仍是 ESP-IDF 5.3 或更新版本。本次沒有 ESP32-C3 工具鏈與開發板，交付為完整原始碼，需在原 Arduino 環境重新編譯與燒錄；主機驗證結果見 `VALIDATION.md`。

API 依據：[ESP32-C3 ADC oneshot](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32c3/api-reference/peripherals/adc_oneshot.html)、[ADC calibration](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32c3/api-reference/peripherals/adc_calibration.html)、[ESP-IDF v5.3 calibration 結構與宣告](https://github.com/espressif/esp-idf/blob/v5.3/components/esp_adc/include/esp_adc/adc_cali_scheme.h)。
