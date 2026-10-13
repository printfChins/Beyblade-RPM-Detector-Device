# BRD_OLED V1.1 驗證紀錄

## 已執行

以 g++、C++17、`-Wall -Wextra -Werror`、AddressSanitizer 及 UndefinedBehaviorSanitizer 編譯並執行。硬體邊界由主機端替身提供，量測、OLED、電池、IO 與主程式使用真正原始碼。

指令：在 `BRD_OLED` 根目錄執行 `python3 tests/run_host_tests.py`。

| 執行檔 | 結果 | 驗證內容 |
| --- | --- | --- |
| LOAD measurement | 通過 | 裝載前忽略 RPM、50 ms 去抖、卸載不清 MAX、比例嚴格小於、HOLD/重新啟動、無脈衝結算 |
| AUTO measurement | 通過 | 不掛載 LOAD 中斷、未達門檻歸零、達門檻結算、HOLD/重新啟動 |
| LOAD OLED | 通過 | 實際 RPM/MAX framebuffer 沒有連線圖示或停用文字，保留電池圖示 |
| AUTO OLED | 通過 | 同上，使用 AUTO 編譯設定 |
| LOAD sketch | 通過 | 主程式與全部四個模組完整連結，電池優先開機、ADC 故障停止與恢復、低電停止與電量恢復重啟 |
| AUTO sketch | 通過 | 同上，確認 AUTO 不掛載 LOAD 中斷 |

兩種量測模式皆執行 30000 RPM、首觸發邊沿極性、micros 回繞、RPM 佇列溢位、超過 32 筆後 MAX 持續更新，以及畫面回報後的 MAX 保持測試。

移除 OLED 提示的回歸案例先在來源版執行，因右下角仍有連線圖示而如預期失敗，再於新分支通過。

靜態相依檢查：十個 production 檔案均無 NimBLE、BBP、設定 API 或 Preferences 相依。電池與 IO 扣除註解及空白後，與附件的可執行程式碼相同。另已完成來源與新分支的唯讀程式碼審查。

## 範圍限制

主機硬體替身不驗證真實 ADC 電壓誤差、GPIO 電氣特性、中斷延遲或 I2C 時序。
執行環境沒有 Arduino-ESP32 工具鏈，尚未完成 ESP32 目標編譯，也未執行實機燒錄或量測。沒有提供 .bin。
