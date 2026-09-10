# V0.10 已執行驗證

本次使用實際交付的 `.ino`、`.cpp` 與 `.h` 原始碼，在主機上替換 Arduino、GPIO、ADC、時間與 I2C 硬體介面後執行模擬。測試檔與硬體替代介面保留在工作暫存區，沒有放入 Arduino 專案，避免被誤編譯。

## 結果

新增低電鎖定後，共 31 個測試案例全部通過。包含既有 22 個量測、GPIO、電量與充電圖示案例，以及 9 個低電停用、警示與恢復重啟案例；主迴圈、量測及 OLED 路徑改動後，全部案例均重新執行。

| 案例 | 實際檢查範圍 |
| --- | --- |
| pins | [本次重跑] GPIO10 為 INPUT_PULLUP 且僅上拉；其餘專案輸入腳無上下拉、I2C 內部上拉禁止、GPIO8 關燈 |
| rpm | 未裝載不計算、首脈衝只作基準、20000/30000 RPM 換算、短雜訊脈衝 |
| debounce | HIGH/LOW/HIGH 跳動後必須重新完成整段去抖 |
| max_hold | MAX 完整畫面後兩秒保持、重繪不延時、解鎖重新去抖、舊畫面不鎖下一輪 |
| lock_history | 丟棄自鎖期間 LOAD 歷史；MAX 維持至有效新裝載 |
| timeouts | 一筆有效 RPM 的結果、空發射 timeout、即時歸零、重新旋轉、裝載中閒置重置 |
| overflow | 佇列溢位且脈衝逾時時，保留已取得的正確最大值 |
| overflow_resync | 短時間佇列溢位不誤觸 50% 結束；後續重新建立 RPM 週期 |
| micros_wrap | 首次邊沿時間為零、長時間裝載後量測、micros 回繞的 RPM 換算 |
| millis_wrap | millis 回繞時兩秒自鎖與解鎖 |
| oled_recovery | OLED 未接時的初始重試、運行中斷線後復原、量測持續可用 |
| oled_bus_failure | I2C bus 建立失敗後可重試 |
| oled_device_failure | I2C device 建立失敗後可重試 |
| oled_steps | 一般畫面每個 loop 最多一個 I2C 傳送；MAX 完整畫面後正確自鎖 |
| oled_partial | 部分 MAX 畫面傳送失敗不能標記為完成；復原後重新送出 |
| repeat | 連續 100 次裝載、轉動、發射、MAX 與解鎖循環 |
| battery_conversion | 電池 mV 換算、SOC 插值、上下限、單次讀取且不平均或濾波 |
| battery_interval | 1 秒取樣間隔、不重複取樣、millis 回繞後正常更新 |
| charge_icons | GPIO10 極性、充電圖示出現與清除、正常電量填充與 0% 時優先顯示沒電警示 |
| max_battery_charge | MAX 自鎖中電量與充電圖示更新，不清除結果或延長自鎖 |
| battery_while_rpm | 持續模擬 20000 RPM 時，ADC 與 OLED 傳送期間仍正確處理 RPM 事件 |
| charge_oled_recovery | OLED 初始化故障後，使用最新電量與充電狀態恢復圖示，GPIO 上下拉設定正確 |
| low_boot | 開機 4% 直接顯示警示、不等待版本畫面、不掛載量測中斷；ADC 持續更新、不讀取一般充電圖示狀態、不反覆重啟 |
| low_exact_five | 開機恰好 5% 不觸發低電停用，仍可正常量測 RPM |
| low_running | 量測中降至 4% 時停用兩個中斷、清除佇列與結果；重複停用只解除中斷一次，延遲事件無法記錄，一般畫面改為警示 |
| low_max_preempt | 低電警示優先於 MAX 自鎖，舊 MAX 畫面完成回報不再啟動自鎖 |
| low_hysteresis_restart | 4% 觸發後升至 5%／7%／9% 仍鎖定，恰好 10% 要求重啟一次；模擬重新進入 setup 後正常量測、不持續重啟 |
| low_splash_drop | 開機版本畫面期間由 40% 降至 4%，再次取樣後不啟動量測中斷 |
| low_oled_fault | 低電開機且 OLED 缺席時仍停用；OLED 恢復後顯示警示；電量 10% 時即使 OLED 故障也要求重啟 |
| low_partial_warning | 警示畫面傳送中途失敗後可重試補畫，量測中斷持續停用 |
| low_millis_wrap | millis 回繞時不會錯誤解除鎖定，達到 10% 仍會要求重啟 |

已檢視由實際 OLED framebuffer 產生的畫面，確認電量圖示、充電圖示、LOADED READY、WAIT LOAD、RPM、MAX 及本次大型空電池警示的位置和邊界。

主機 C++17 編譯使用 `-Wall -Wextra -Werror -Wshadow -pedantic`，各 `.cpp` 與 `.ino` 亦分別完成語法檢查。模擬案例以 AddressSanitizer / UndefinedBehaviorSanitizer 執行，未回報對應錯誤；環境不支援 LeakSanitizer，未宣稱執行記憶體洩漏驗證。

另外檢查所有交付原始碼，僅 GPIO10 充電 DET 使用 `INPUT_PULLUP` / `GPIO_PULLUP_ONLY`；其餘專案輸入腳不啟用內部上下拉。ADC 單次讀取集中在 `brd_battery.cpp`。沒有 `INPUT_PULLDOWN`、NimBLE 呼叫、BLE 模組引用、Wire 呼叫、Serial 初始化或 Deep-sleep 呼叫。輸出 ZIP 不含舊版建置產物。

## 驗證限制

本環境未安裝 ESP32 Arduino board package 與 RISC-V 交叉編譯工具鏈，因此沒有完成真正的 ESP32-C3 韌體編譯、連結或燒錄，也未以實體 OLED／IR 電路測試。主機語法與模擬結果不能代替開發板上的硬體驗證；原生 I2C API 使用方式另外參照 README 所列 Espressif 官方文件。

重啟案例使用 `esp_restart()` 替代介面確認呼叫條件，再模擬重新進入 setup；未執行真實 MCU reset。低電與恢復測試使用模擬 ADC 電壓，未驗證實際電池、分壓誤差或充放電時的百分比準確度。

ZIP 不提供 BIN，需於原使用者 Arduino 環境編譯產生。
