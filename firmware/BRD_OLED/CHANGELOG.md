# V0.10 修改紀錄

來源：使用者附件 `BLE_RPM_V1.9(2).zip`，原資料夾 `BLE_RPM_V1.9/`。

## 修改

- 專案資料夾與主程式改為 `BRD_OLED_V0.10`，版本字串改為 `V0.10`。
- GPIO3、GPIO1 設為不啟用內部上下拉的 INPUT；GPIO0 電池 ADC 不啟用內部上下拉；GPIO8 固定關燈。
- [後續修正] GPIO10 充電 DET 恢復內部上拉 `INPUT_PULLUP`，下拉關閉；讀取 LOW/HIGH 控制 OLED 充電圖示。版本維持 V0.10。
- GPIO20、GPIO21 改用原生 I2C master driver，初始化及故障復原皆禁止內部上拉。
- OLED 保留 `LOADED READY / WAIT LOAD`、`RPM xxxx / MAX xxxx` 與開機版本畫面，並恢復右上角電量與充電閃電圖示。
- [後續恢復] 加回 `brd_battery.cpp/.h`，沿用 V1.9 的 GPIO0、470k/470k 分壓、12-bit/11 dB、單次 ADC 直接換算與 SOC 插值表。
- [本次修改] ADC 取樣間隔由 10 秒改為 1 秒，供正常與低電模式判斷；維持單次讀取、不平均、不加入 IIR。
- [本次修改] 主 loop 先更新電量，再判斷是否允許一般功能；低電停用優先於 RPM、LOAD、MAX 自鎖與一般 OLED 畫面。
- [後續恢復] 電量與充電狀態納入 OLED 畫面變更判斷，圖示更新不更動量測狀態或 MAX 自鎖時間。
- OLED 由獨立 Task 改為主 loop 分段寫入，畫面、量測資料與 I2C 皆由同一執行路徑處理。
- 量測完成直接保留 OLED MAX；新版本沒有等待傳輸或 ACK 的狀態。
- LOAD 每次最新邊沿皆重新計算去抖，即使中間多次跳動後回到相同候選電位，也不能沿用舊時間。
- 無脈衝時即時 RPM 歸零，不製造 50% MAX 的替代量測值。發射後正常結束門檻仍為 MAX 的 50%。
- 移除 BLE 曲線點數限制，至少一筆有效 RPM 即可形成 OLED MAX 結果。

## 新增

- [本次新增] 電量小於 5% 時鎖定；恰好 5% 不觸發。鎖定後 5%～9% 繼續停用，達到 10% 或以上才透過 `esp_restart()` 重新開機。
- [本次新增] `brd_measurement_stop()` 停用 RPM／LOAD 中斷，清除事件佇列、量測與 MAX 資料；重複呼叫不重複解除中斷，延遲到達的舊事件或畫面完成回報無法重啟量測。
- [本次新增] 低電 OLED 畫面只顯示中央大型空電池與驚嘆號，取代一般文字、電量及充電圖示；正常模式仍保留原有圖示。
- [本次新增] 開機初次 ADC 即低電時略過版本畫面與量測啟動；正常開機畫面結束後再檢查電量。
- [本次新增] 低電時只執行 ADC、OLED 警示及必要系統排程；OLED 初始化或傳送故障不阻止鎖定與恢復重啟。
- [本次新增] 低電／恢復門檻、低電 loop 等待與警示重繪間隔集中於 `brd_config.h`，並檢查門檻關係 `0 < stop < recover <= 100`。
- `OLED_MAX_HOLD_MS = 2000`，與 `LOAD_IR_DEBOUNCE_US = 1000` 放在同一個 cfg 區塊。
- MAX 完整畫面成功送出後開始自鎖。重繪同一結果不延長自鎖；舊畫面不能鎖住下一次量測。
- 自鎖期間停止處理 LOAD 事件；到期後取樣實際電位並重新完整去抖。
- OLED 開機初始化失敗或運行中 I2C 傳送失敗，每秒重試復原。
- 每次一般畫面寫入都有單次 I2C 等待上限；OLED 故障不進入無限等待。
- RPM 佇列溢位時丟棄不完整的佇列並重新建立週期，避免用缺失脈衝算出錯誤的低 RPM。
- 以獨立有效旗標取代 micros 值為零的判斷，並處理計時器回繞。

## 刪減

以下舊檔案不再放入新版本：

| 原檔案 | 原因 |
| --- | --- |
| `brd_ble.cpp`、`brd_ble.h` | 移除 BLE 初始化、廣播、連線、Notify、控制命令及可靠傳輸 |
| `BLE_API.txt` | 此版本沒有 BLE API |
| `brd_power.cpp`、`brd_power.h` | 移除 OLED 自動關閉與 Deep-sleep |
| `brd_log.cpp`、`brd_log.h` | 移除 Serial 記錄與 UART 初始化 |
| `brd_context.cpp`、`brd_context.h` | 改為量測模組內部狀態，不再共享 BLE、ADC、休眠欄位 |
| `brd_types.h` | 移除曲線點型別；量測狀態型別改放 `brd_measurement.h` |
| `brd_utils.cpp`、`brd_utils.h` | 移除封包編碼與本版不再使用的工具函式 |
| 原 `.bin`、`.elf`、`.map`、`build/` | 都是 V1.9 編譯產物，不屬於 V0.10 |

另外移除 16384 點曲線緩衝、LAUNCH RPM 歷史緩衝、Session、CRC、ACK、重傳與 BLE 裝置名稱相關狀態。保留 RPM ISR 的短佇列，供主 loop 計算轉速使用。
