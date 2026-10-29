# BRD BPX PRO V1.2：NEBULA 三階段雙環與主畫面峰值保持

## 需求

- LIVE 主畫面數值只能向上增加，最終值與本次 MAX 相同。
- 01 NEBULA WAIT LOAD：黃色內圈、藍色外圈都不繪製。
- 01 NEBULA LOADED：啟動黃色內圈，20 幀旋轉一圈。
- 01 NEBULA 已發射：藍色外圈按本次最高 RPM 延伸，黃色圈仍持續旋轉。
- 測量完成：保持外圈最終角度與 MAX 數字，黃色圈停止旋轉。
- 02 DIAL、06 TORQUE 主畫面也採峰值顯示；原始造型不修改。

## 重要：量測真實流程

目前 LOAD 模式中的發射視覺條件，使用 `measurement.loaded` 由 true 轉為 false（GPIO1 ADC 的穩定裝載判斷），且已有有效 RPM。
AUTO 模式不依賴 LOAD，首次有效 RPM 即可啟動藍圈。
`brd_measurement.cpp` 的內部發射事件（`check_launch_threshold`）仍採轉速下降門檻；該邏輯沒有修改。
原始 `measurement.value` 仍然是即時 RPM，**不會**被峰值覆蓋，確保曲線、BLE 與結束判斷保留正確下降資料。

## 程式檔案

1. `[修改] brd_measurement.h`：`brd_display_t` 末端增加預設初始化的 `peak_value`，保留原五欄位 aggregate 初始化相容性。
2. `[修改] brd_measurement.cpp`：`brd_measurement_get_display()` 提供 `g_max_rpm`；量測完成提供 `g_display_max`。
3. `[修改] brd_ui_style_scene.cpp`：三種主畫面顯示峰值；NEBULA 內外圈依裝載、發射及 MAX 條件分別控制。
4. `[修改] brd_ui_motion.cpp`：READY 時黃色內圈照常動畫，峰值／generation 變化觸發更新；未裝載時停止動畫。
5. `[新增] tests/test_nebula_three_phase.cpp`：驗證待機／裝載／發射／MAX、轉速回落、其他主題峰值不下降。
6. `[新增] tests/render_nebula_phases.cpp`：正式 GFX 場景同源影格導出，主機端工具。
7. `[修改] tests/run_motion_tests.py`：將新測試納入整體回歸。

`BRD_BPX_PRO.ino` **不需要修改**。版本仍為 V1.2。

## 刷新效能

內圈每約 120 ms 更新 18°，總計 20 步／圈（約 2.4 秒／圈），由非阻塞排程驅動；SPI 仍透過 8×16 Dirty Tile 區塊差異傳輸。LCD 不解碼 GIF。

## 驗證界線

已完成 C++ 主機端畫面測試與功能回歸；尚未使用 ESP32-C3 Arduino Core 3.3.11 真正編譯／量測 ST7735 上的幀率與 CPU 使用率。
