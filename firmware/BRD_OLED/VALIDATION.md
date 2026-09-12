# V0.12 驗證紀錄

```cpp
/*
本文件只記錄本次實際完成的驗證，不沿用 V0.11 文件中的測試結論。
測試對象為交付的完整 brd_measurement.cpp、brd_battery.cpp、brd_io.cpp、
brd_oled.cpp 與 BRD_OLED.ino，GPIO、時間、ADC、I2C 及 MCU 重啟使用替代介面。

已執行 38 個主機回歸案例，全數通過。
編譯使用 C++17、-Wall -Wextra -Werror -Wshadow -pedantic，
啟用 AddressSanitizer / UndefinedBehaviorSanitizer，未回報對應錯誤。
LeakSanitizer 未啟用，未宣稱完成記憶體洩漏驗證。
*/
```

| 案例 | 確認內容 |
| --- | --- |
| `threshold20` | MAX 20000 時，5000 RPM 不結束；4000 RPM 進入 HOLD |
| `one_dropped_edge` | 20000 RPM 漏一個邊沿形成 10000 RPM 時，不再觸發原 50% 結束 |
| `spike_policy_unchanged` | 插入假邊沿仍可形成 MAX 60000，確認未擅自加入相鄰週期檢查 |
| `load_backlog` | 主迴圈延後處理時，LOW 2 ms 再 HIGH 仍辨識發射及下次裝載 |
| `load_short_bounce` | LOW 999 us 的跳動不通過 1000 us 去抖 |
| `load_exact_debounce` | LOW 恰好 1000 us 後回 HIGH，前一狀態仍被承認 |
| `load_rpm_chronology` | 同批次 LOAD、RPM 與結算按時間順序處理；HOLD 後忽略重新裝載 |
| `timeout_chronology` | 停轉結算期限早於較晚的 LOAD HIGH，優先形成並保持結果 |
| `unload_before_first_rpm` | 卸載後才出現的首個 RPM 邊沿不冒充有效發射 |
| `load_overflow` | LOAD 溢位累計診斷、作廢進行中量測、重新完整去抖後可再次量測 |
| `rpm_overflow` | RPM 溢位不以遺失脈衝間距計算，之後兩個邊沿重新建立有效 RPM |
| `hold_unlock_generation` | 完整 MAX 畫面起算 2.5 秒；解鎖重新去抖；舊畫面回報不鎖住新量測 |
| `micros_wrap` | LOAD、RPM、去抖及 20% 門檻跨 micros 回繞仍正常 |
| `millis_wrap_hold` | HOLD 計時跨 millis 回繞仍正常 |
| `no_rpm_timeout` | 沒有形成有效 RPM 的發射，在 1200 ms 到期後清除 |
| `normal_30k` | 持續 30000 RPM 正確；重複 begin 不重置既有量測 |
| `adc_single_conversion` | 每週期最多一次 ADC；3700 mV 換算 20%；不重複建立 handle |
| `adc_error_keeps_value` | 讀取失敗保留有效 7%，不觸發低電；保留最近失敗錯誤碼與累計次數 |
| `adc_stale` | 有效資料達 3 秒未更新時進入 ADC 故障；下一筆有效 7% 可恢復 |
| `adc_stale_latch_wrap` | 長時間故障跨 millis 回繞不會重新把舊讀值當成有效 |
| `oled_boot_version` | 開機 framebuffer 使用 BRD 與 V0.12 |
| `adc_boot_error` | 開機 ADC unit 建立失敗時暫停，但不設低電鎖定；下週期可恢復 |
| `adc_config_retry` | 通道配置失敗可重試，已建立的 ADC unit 不重複配置 handle |
| `adc_calibration_retry` | 校正 handle 建立失敗可重試；校正轉換失敗不改寫有效電量 |
| `adc_invalid_data` | 負 raw／負校正電壓標為資料錯誤，不寫入 SOC |
| `valid_zero_is_low` | API 成功的有效 0 mV 仍觸發低電，與讀取錯誤分開 |
| `exact_five` | 恰好 5% 允許量測 |
| `recovery_duration` | >=10% 的四筆每秒有效樣本跨滿 3 秒後才要求重啟 |
| `recovery_error_break` | 恢復期間任一讀取失敗會重置計時 |
| `recovery_low_break` | 恢復期間回到 7% 會重置計時，維持低電鎖定 |
| `recovery_gap` | 過長取樣間隔不能算作連續達標，需要重新累積時間 |
| `recovery_millis_wrap` | 恢復計時跨 millis 回繞仍正常 |
| `reboot_policy_unchanged` | 重新 begin 模擬重新開機後，7% 可恢復，確認未新增跨重啟鎖定 |
| `main_adc_fault_resume` | 主 loop 在 ADC 逾時時停用量測、顯示 ADC ERR，恢復後不重啟 MCU 即重新待測 |
| `main_low_restart` | 主程式低電分支只在恢復時間達標後要求重啟 |
| `oled_adc_boot_recover` | 開機 ADC 故障不等待版本畫面；C 字型存在；成功後恢復一般畫面 |
| `oled_retry_hold` | MAX 畫面失敗不算完成；I2C 復原後開始 HOLD；重繪不延長自鎖 |
| `gpio_pulls` | GPIO10 保留上拉，GPIO0／1／3／20／21 不啟用內部上下拉 |

已由實際程式 framebuffer 輸出開機、HOLD／MAX 及 ADC ERR 畫面，放大檢視文字、數字與邊界。另分別對所有 `.cpp` 與 `.ino` 使用替代標頭做獨立語法檢查，避免單一測試 translation unit 掩蓋缺少 include 的問題。

重跑方式：在解壓縮目錄使用 `python3 verification/run_host_tests.py`。需 Python 3、g++ 及 AddressSanitizer／UndefinedBehaviorSanitizer 支援；可加 `--output <目錄>` 保存測試產生的 PGM 畫面。此測試不需要修改 firmware 原始碼。

```cpp
/*
驗證限制：

本環境未安裝 Arduino-ESP32 board package 與 RISC-V 交叉編譯工具鏈。
未完成 ESP32-C3 實際編譯、連結、燒錄、I2C / IR 實體量測或功耗量測。
替代介面的成功不能當成目標晶片的編譯與即時性能保證。

ADC API 與結構另外核對 Espressif 官方文件及 ESP-IDF v5.3 的標頭宣告。
測試的 ADC 錯誤由替代介面注入，未驗證實機 ADC 故障發生率。
沒有用示波器驗證 ISR 延遲、臨界區耗時、IR 邊沿品質或 ADC 節點精度。

esp_restart() 在測試中以例外中止模擬本輪執行，確認觸發條件，
未宣稱完成真實 MCU 重啟或硬體供電恢復驗證。

交付 ZIP 不含 BIN；所有 V0.12 韌體需在原 Arduino 環境重新編譯。
*/
```
