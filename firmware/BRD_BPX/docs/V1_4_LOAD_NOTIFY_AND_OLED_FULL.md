# BRD_BPX V1.4 — LOAD 狀態 Notify 與 OLED FULL 顯示修正

## 修改範圍

- `BRD_BPX/brd_bbp.cpp`：恢復經既有量測模組去抖後 `display.loaded` 狀態的變化通知，使用原本 17-byte `0xA0` 封包、Byte 3 bit 2 作為 LOAD 狀態。不新增 UUID 或 Characteristic。
- `BRD_BPX/brd_oled.cpp`：主頁面在 FULL latch 有效時，在電池圖示左側 `x=80, y=0` 顯示 5x7 字型的 `FULL`，不顯示充電閃電；未 FULL 保留原充電圖示，且 FULL latch 改變會重繪。
- `BRD_BPX/tests/test_bbp_link.cpp`：更新 LOAD 正/反邊緣事件、30 秒 SOC 定期更新、連續切換、傳輸中延遲與 BLE Notify 失敗重試案例。
- `BRD_BPX/tests/test_priority_ble.cpp`：避免將 LOAD 新增 A0 誤判為曲線頁，檢查兩筆 B6 中繼資料。
- `BRD_BPX/tests/render_boot.cpp`：新增 `full.pgm`、`charging.pgm` 實際 OLED framebuffer 的測試。
- `BRD_BPX/tests/run_host_tests.py`：加入 14 組 BBP 連線情境與 FULL/charging 畫面渲染。

## 工作流程

1. 訂閱有效後原排程先送首次 A0（訂閱穩定時間 100 ms）。
2. 主迴圈取得已去抖的 `brd_measurement_get_display().loaded`，只有狀態改變且已連線/訂閱才設定待送旗標。中斷及 BLE callback 都不進行 Notify。
3. 既有 A0 封包 Byte 3 bit 2 = 1 表示 LOADED，= 0 表示 WAIT LOAD；電量 Byte 4 採用傳輸時最新 SOC。
4. 30 秒定期電量 A0 保留。LOAD A0 與 APP 主動 `0x51` 回覆同樣帶最新電量；每次成功送入 BLE stack 的 A0 會重計 30 秒，避免多餘封包及功耗。
5. 優先送完 12 頁 BBP 曲線，再排入待送 LOAD/A0；曲線期間多次 LOAD 切換以最新狀態合併一次回報，避免中途插入及佇列膨脹。
6. 沒有變更時不新增任何 BLE 輪詢。沿用 `brd_scheduler_take_notify_slot()`、10 ms 分頁間隔、100 ms Notify 失敗重試與斷線/取消訂閱停送邏輯。
7. OLED FULL 仍由原版 `brd_battery_is_full()` 決定（充電中 >= 4.15 V 持續 30 秒 latch）；本次不改電量演算法、低電鎖定與充電速率限制。

## 沒有修改

`BRD_BPX.ino`、`brd_measurement.*`、`brd_capture.*`、`brd_battery.*`、`brd_scheduler.*`、`brd_config.h`、低功耗 BLE 設定、SPI/I2C、各硬體 GPIO、量測 ISR。

## 測試與限制

- 主機端已執行 14 組 BBP 情境、OLED 實際像素比對、RPM 13 組優先權情境、主程式/BBP 語法檢查。
- 不是 ESP32-C3 實體編譯與手機實測；`notify()` 回傳 true 僅代表已接受發送，不保證 APP 收到。
- APP 必須訂閱原本 BBP Notify characteristic，並在每次收到 A0 時重新讀取 Byte 3 bit 2，而不是只在裝置連線時解析一次。
- 若 LOAD 翻轉速率超過 BLE 允許的傳輸速度，僅保留傳送前最新狀態；不保證重播每次瞬間切換。這是維持 RPM 與功耗優先的刻意設計。
