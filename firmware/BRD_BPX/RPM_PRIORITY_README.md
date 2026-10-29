# BRD BPX V1.3 RPM 優先修正版

本包另含後續 BLE 連線修正，先閱讀 `BLE_LINK_STABILITY_README.md`。以下 RPM 優先修改全部保留。

修正日期：2026-10-25。基準：BRD_BPX(20261024-143226).zip。
韌體顯示版本依使用者要求更新為 V1.3；本修正代號 RPM_PRIORITY_20261025。

## 使用方式與完整檔案位置

解壓縮後使用完整 `BRD_BPX/` 資料夾，Arduino IDE 開啟 `BRD_BPX/BRD_BPX.ino`。所有根目錄 `.cpp`、`.h` 必須一起保留；不需手動拼貼程式。

```text
BRD_BPX/
  BRD_BPX.ino
  brd_capture.cpp / brd_capture.h
  brd_scheduler.cpp / brd_scheduler.h
  brd_measurement.cpp / brd_measurement.h
  brd_bbp.cpp
  brd_bbp_session.cpp / brd_bbp_session.h
  brd_settings.cpp / brd_settings_ble.cpp
  brd_oled.cpp / brd_config.h / brd_io.cpp
  brd_ota.cpp
  其餘原有 .cpp / .h
  RPM_PRIORITY_README.md
  docs/
  tests/
```

沿用原專案 ESP32-C3、Arduino ESP32 core 3.3.11、NimBLE-Arduino 2.5.1 與原分割表設定。使用編譯產生的 APP bin 進行既有 OTA/APP-only 更新。此包不附 .bin：原封包的所有舊編譯檔已排除，避免誤燒舊韌體。

## 優先級與功能行為

1. GPIO ISR 以事件時間擷取 RPM；主迴圈先處理事件及狀態機。
2. OLED 一輪最多送原本的一個 I2C 區塊，先於背景功能。
3. OTA、BBP、設定模組之間再次服務 RPM。三組服務共用每輪一次 Notify 嘗試額度，待處理輸入優先。
4. 正常歷史/設定保存不關閉事件捕捉，不清除保存途中排入的事件。
5. 低電/ADC 故障仍依原保護停止量測；使用者主動開始 OTA 時明確進入維護模式，量測暫停至取消、失敗、15秒接收逾時或更新完成重啟。

## 新增、修改與刪減

| 完整位置 | 調整 |
| --- | --- |
| BRD_BPX/brd_capture.cpp、brd_capture.h | 新增原生 ESP-IDF IRAM GPIO service、IRAM callback 及 esp_timer_get_time 時間路徑。取代 Arduino 非 IRAM wrapper；不需修改 core 的 CONFIG_ARDUINO_ISR_IRAM。 |
| BRD_BPX/brd_measurement.cpp、brd_measurement.h | 修改 HOLD 為顯示限制，新裝載可立即優先；尾段須新 LOAD 或連續安靜300ms重新武裝。新增 ISR/排入/丟棄/滿列/最後時間等固定 RAM 診斷。 |
| BRD_BPX/brd_measurement.cpp | 刪減正常 NVS 保存時的關閉擷取與恢復時清空 Queue；增加設定套用原子窗口、獨立 OTA 維護暫停及狀態一致性恢復。 |
| BRD_BPX/brd_config.h | RPM FIFO 128改1024格；每16筆釋放一次快照 critical section。新增事件世代避免跨遺失區段計算錯誤週期。 |
| BRD_BPX/brd_settings.cpp | Flash 成功後若已有新事件，先以舊設定處理量測，安全後再套用。相同模式保存保留 LOAD 去抖/READY；ADC式停止恢復不會使設定永久 PENDING。 |
| BRD_BPX/brd_bbp_session.cpp、brd_bbp_session.h | 新增8筆完成資料 FIFO，保留600ms發布延遲，下一輪不清除上一輪；發布完成資料不會中止新採樣。容量滿時先發布最舊完成記錄。 |
| BRD_BPX/brd_scheduler.cpp、brd_scheduler.h | 新增三組 BLE 服務共用的每輪傳送額度。 |
| BRD_BPX/BRD_BPX.ino | 修改執行順序；新增 service_rpm()，放在 loop() 之前。背景操作前後優先處理 RPM。BLE 初始化前先建立擷取。 |
| BRD_BPX/brd_io.cpp | GPIO 及 BLE 初始化早期先安裝 IRAM service。 |
| BRD_BPX/brd_bbp.cpp、brd_settings_ble.cpp、brd_ota.cpp | 使用共用 Notify 額度；OTA 使用獨立維護取得/釋放介面。 |
| BRD_BPX/brd_oled.cpp | 新增 IR ERR / IR LOSS 狀態；MAX 完整畫面回報不再關閉 RPM。 |

主程式以完整修正檔取代即可；原版第22行 include 區新增 scheduler，原版第31行 BLE 初始化前啟動擷取，原版第42行 loop 起改用新排程。各修改檔案均保留完整程式並有中文新增/修改/刪減註解。

## 與原行為的重要差異

- MAX 原有2.5秒 HOLD 保留作為顯示，但不阻擋下一次有效裝載。AUTO 必須先有300ms安靜，避免把同一發的尾段重複計次。
- `IR ERR`：原生擷取初始化失敗，不能將未建立的捕捉當成正常0 RPM；初始化成功後恢復。其他模組若先安裝 GPIO service，必須統一其 IRAM 屬性及擁有權。
- `IR LOSS`：已知事件遺失，該輪不發布不完整 MAX/歷史；週期基準重新同步，新有效量測可恢復。不能用缺了邊沿的長週期冒充低轉速。
- UUID、BBP 17-byte封包、設定20-byte回覆、OTA格式、GPIO、輸入上下拉、預設百分比與轉速門檻均沿用。最高有效範圍仍是原本60000 RPM，不另加入未經驗證的峰值濾波。
- 1024格實際可存1023筆，同極性30000 RPM約2.046秒；尚未鎖定參考時正負緣皆入列，容量時間較短。事件世代、較大快照及完成 FIFO 合計增加約23KB靜態RAM，應以實機編譯結果確認。

## 驗證

執行 `python3 tests/run_host_tests.py`。使用真正的韌體狀態機、Session、設定及主迴圈；替身僅用於 GPIO、時鐘、NVS、I2C及BLE stack等硬體邊界。

- 既有設定、CRC/NVS失敗、BBP、OTA MTU/MD5/映像/斷線/重連、OLED及DXF回歸測試。
- 保存期間收到RPM、新裝載取代HOLD、600ms內跨下一輪保留完成資料。
- NVS途中脈衝先用舊設定處理；相同模式保存後立即收到第一圈。
- OTA維護不能被一般 begin() 誤解除，明確釋放後恢復。
- 原生服務初始化失敗可見且恢復，ISR計數可辨別是否真的進入。
- Queue遺失不發布不可信MAX，尾段溢位不誤判安靜或重複計次。
- ADC式重啟不使保存交易卡住；嚴格BBP pending旗標至實際發布。
- 真實主迴圈順序、三服務每輪最多一次Notify。
- LOAD及AUTO各120分鐘、各7200發，共4小時虛擬時間、14400發；最高30000 RPM在前32點後仍正確更新，曲線保留前32筆。涵蓋micros有號/無號邊界與回繞。
- ASan/UBSan驗證記憶體及未定義行為。

這些是電腦端測試，不是實機連續4小時測試。目前環境沒有 ESP32 目標編譯工具鏈，未產生新 APP bin，也尚未確認實際 IRAM map、GPIO 中斷延遲、BLE射頻及感測器波形。不能據此宣稱所有使用者原先30分鐘故障已在實機根治。

## 建議實機驗收

1. 使用已知週期訊號源測試 GPIO3：2000us同極性週期應為30000 RPM；確認一圈一脈衝及實際感測反射條件。
2. 保持BLE連線與通知/設定查詢，在保存歷史時觀察RPM及GPIO波形；量測至少60分鐘。
3. 在HOLD內重新裝載、連續拉轉，確認新MAX及曲線；AUTO舊尾段不得增加第二筆。
4. OTA開始、取消、斷線、接收逾時及失敗後，確認恢復擷取；維護期間不進行RPM驗收。
5. 比較診斷 rpm_isr_entries、rpm_events_enqueued、rpm_events_dropped、rpm_capture_disabled、max_rpm_queue_depth、invalid_measurements，配合邏輯分析儀確認訊號是否真的到GPIO。正常保存不應增加 rpm_capture_disabled。
