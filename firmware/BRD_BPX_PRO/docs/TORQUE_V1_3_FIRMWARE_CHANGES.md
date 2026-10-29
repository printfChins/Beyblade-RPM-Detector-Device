# BRD_BPX_PRO V1.3 — TORQUE V3 韌體修改記錄

日期：2026-10-29。基準：`BRD_BPX_PRO_TORQUE_ONLY_20261029.zip`。所有修改檔均放在 `BRD_BPX_PRO/` 與主程式相同目錄。**不是 AI 生圖；附圖來自正式 `brd_ui_style_scene.cpp` C++ 渲染與主機端 GFX stub。**

## 核心行為

1. `brd_config.h`：版本號 V1.3。`LOAD_ADC_ACTIVE_RAW_THRESHOLD=4000`；`UI_CURVE_SAMPLE_MS=50`、`UI_CURVE_POINTS=64`；TORQUE 計量條維持 `BRD_UI_INDICATOR_FULL_SCALE_RPM=20000`。
2. `brd_load_adc.cpp/.h`：將電壓 mV 施密特分類刪除，改為 **raw<4000 裝載／raw>=4000 未裝載**；保留 ADC 失效超時安全邏輯、取樣間隔、量測狀態機 100ms 去抖。
3. `brd_measurement.cpp`：保留 CHANGE ISR + 固定大小 RAM Queue；每個邊緣帶時間與極性，主 loop 各自用前一次**同極性**的時間差計算 60,000,000 / period_us。`LOW` 與 `HIGH` 週期互不干擾；異常期間、Queue overflow 等只重新同步期間基準，不於 ISR 執行 BLE/ADC。MAX 取兩者全程最高。
4. `brd_bbp_session.cpp/.h`、`brd_bbp.cpp/.h`：兩組各自保存前 32 個有效同極性週期，額外以 `edge_max_[0/1]` 保存不受長度限制的最高值。發射結束後選擇 MAX 較高的組；同分優先資料較多者，再同分選正緣。原 17-byte*12 BBP 封包、181-byte NVS Storage、歷史 50 筆格式均維持。
5. `brd_ui_curve.cpp/.h`：本機曲線從事件流取得（不是 LCD 渲染時間取樣），最低 50ms 間隔、最多 64 筆，發布後凍結；每個有效取樣點繪製圓點並連線。**UI 64點 / BLE 32點為不同資料路徑，BLE 不被 UI 降採樣**。
6. `brd_ui_style_scene.cpp`：TORQUE 主頁 SP/MAX 字體 GFX 3×、固定 x60；移除 MAX 左下 PEAK；右側原 W1 呼吸與 L2 收束維持。HISTORY x3/x83 兩欄各四筆，移除橫條，`TOP` 顯示整個歷史最高（可能不在最近 8 筆），該最高值高亮，其他白色。OTA x1。BOOT 8 格以 tick/2 漸亮。
7. `brd_ui_motion.cpp`、`brd_ui_core.cpp/.h`、`brd_tft.cpp`：啟用 BOOT 場景 40ms 排程，在原本 1500ms 啟動等待中播放；Graph 動畫延長至 64 個節點可完全繪出；歷史 8 筆及 TOP 納入更新簽章。UI 不自行呼叫原先 `curve_observe()` 進行二次假取樣。
8. 主程式 `BRD_BPX_PRO.ino` 只有**頂部註解同步**，沒有更動 `setup()` / `loop()` 執行順序。

## GPIO 和重要限制

- GPIO1 ADC 原始值 `<4000` 接近滿刻度（4095），**務必以未裝載／已裝載波形確認電路與 ADC 特性**；100ms 去抖不保證能解決長時間偏低時的誤裝載。
- 正緣與負緣有兩套完整一圈估算，但只有在感測器每圈各有一個有效上升／下降事件時才近似半圈時間交錯；**沒有用半圈的時間直接推 RPM**。
- 本機畫面最多 64 點，50ms 間隔；低於 50ms 的有效事件仍會更新真實 SP、MAX 與 BLE 原始樣本。曲線超過 64 點後不新增，直到下次裝載／AUTO 重設。
- MAX、Ble/OTA/NVS/計數與用戶端資料結構仍沿用 V1.2；不更動 UUID、分割表、低電保護、充電控制與 GPIO5 功能。
- 本包已通過 C++17 主機 mock 編譯與回歸測試；**未使用 Arduino CLI/ESP32 Core 編譯真實 MCU，也未完成實機燒錄及長時間量測**。

## 路徑結構

```text
BRD_BPX_PRO/
├── BRD_BPX_PRO.ino         # 入口（邏輯保持）
├── brd_config.h            # [修改] ADC 門檻、曲線點數、版本
├── brd_load_adc.cpp/.h     # [修改] GPIO1 raw 比較
├── brd_measurement.cpp     # [修改] 兩極性完整一圈
├── brd_bbp.cpp/.h          # [修改] 雙組資料傳輸與 8 筆歷史 API
├── brd_bbp_session.cpp/.h  # [修改] 依 MAX 選擇極性組
├── brd_ui_curve.cpp/.h     # [修改] UI 64 點資料來源
├── brd_ui_types.h          # [修改] HISTORY 8 筆與 TOP
├── brd_ui_style_scene.cpp  # [修改] TORQUE 畫面
├── brd_ui_motion.cpp       # [修改] BOOT/Graph 幀數
├── brd_ui_core.cpp/.h      # [修改] BOOT 實際繪製
├── brd_tft.cpp             # [修改] 8 筆歷史快照 / 開機動畫
├── tests/                 # 主機端測試，**不可直接部署到 MCU**
└── docs/TORQUE_V1_3_FIRMWARE_CHANGES.md
```

## 主機測試

於 `BRD_BPX_PRO/` 執行 `python3 tests/run_motion_tests.py`，其中包含：

- 3999/4000 RAW 邊界、LOAD 超時失效保護、CPU 時間與 GPIO IRQ 模擬；
- 交錯正緣/負緣計算兩組完整一圈 RPM；
- 兩組 BLE Profile 的最優 MAX 選擇及超過 32 點的峰值；
- BBP 封包、歷史與 NVS 編解碼相容性；
- 開機 0→8 格／40ms Frame，W1/L2、25FPS、HISTORY/Graph、OTA 等既有回歸。
