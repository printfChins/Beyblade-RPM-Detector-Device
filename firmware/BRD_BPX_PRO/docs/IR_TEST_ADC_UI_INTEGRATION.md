# IR TEST ADC UI 整合紀錄

## 檔案位置與變更
- `brd_ui_page_ir.cpp`：更新右上角電池符號 + 電壓、感測器白色啟動效果、左右 0–4095 垂直條；Dirty Region 分離。
- `brd_ui_types.h`：新增 LOAD/RPM 原始 ADC 數值和有效狀態。
- `brd_load_adc.cpp/.h`：沿用 GPIO1 ADC1_CH1 轉換結果，新增 raw getter。
- `brd_rpm_adc.cpp/.h`：新增 GPIO4 ADC1_CH4 原始值模組；50 ms 取樣，300 ms 無有效樣本視為無效。
- `brd_adc_shared.cpp/.h`：擴充 ADC1 唯一 owner 至 GPIO0、GPIO1、GPIO4，RPM ADC 無須校正即可讀 raw。
- `brd_tft.cpp`：僅於 IR TEST 且沒有低電/故障/OTA 覆蓋時取得 RPM ADC，畫面每 100 ms 最多更新一次。
- `brd_ui_page_main.cpp`、`brd_ui_page_graph.cpp`、`brd_ui_widgets.cpp`：同步之前確認的預覽修改，移除 MODE、BTN，縮小頂部 WAIT LOAD/LOADED/HOLD 至 TextSize 1。
- `BRD_BPX_PRO.ino`：**不修改**；沿用 `brd_tft_begin()` / `brd_tft_update()`。

## 硬體對照
- GPIO0：BATTERY_ADC，GPIO1：LOAD_ADC，GPIO4：RPM_ADC（類比量）；共用 ADC1。
- GPIO3：RPM 數位脈衝（保留原中斷與測量演算法）。
- ADC 數值為 12-bit raw（0～4095），不是校正過的 mV。GPIO4 的類比電壓要保持在 ESP32-C3 ADC 安全範圍內。
- GPIO1 LOAD_ACTIVE_LOW 目前仍是假設，0.89 V/2.65 V 已知但裝載極性尚需實機確認。

## UI 區域 160x80
- 顶部（y=0..18）：IR TEST；右上 x=105..159：電池圖示 + 4.13V；頂部 4.13V 為示例，實際顯示 GPIO0 推算的電池電壓。
- 左側：LOAD ADC 由下往上填充，顯示 4 位數字；中間：左 LOAD（六角形），右 RPM（梯形）白色圖示；右側 RPM ADC 由下往上填充。
- 任一 ADC 讀取無效時，文字為 `----`、長條為空；原始 ADC 數值不保證表示 RPM 轉速。
- 目前沒有 ADC 平均濾波；變動迅速可能看見短時間數字跳動。
- 只在切換頁面時全螢幕清除，同頁只更新變更的元件矩形。
