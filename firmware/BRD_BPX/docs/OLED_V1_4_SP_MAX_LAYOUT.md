# BRD_BPX V1.4 — OLED 主頁 SP/MAX 與電池排版

## 修改位置

- `BRD_BPX/brd_oled.cpp`：唯一改動的韌體來源檔；`oled_begin_frame()`。
- `BRD_BPX/tests/render_boot.cpp`：用真正的 `brd_oled.cpp` 生成 framebuffer，新增六種主頁情境逐像素測試。
- `BRD_BPX/tests/run_host_tests.py`：將主頁六情境納入 OLED 回歸。
- `BRD_BPX/docs/OLED_V1_4_SP_MAX_BATTERY_ACTUAL.png`：實際韌體 framebuffer 排版預覽。

## 主頁版面（SSD1306 128×32）

| 元件 | 座標 | 樣式 |
|---|---|---|
| 左上 LOAD 狀態 | x=0, y=0 | 1 倍：`WAIT LOAD` / `LOADED` |
| 充電閃電 | x=72..76, y=1..7 | 充電中且未確認 FULL 顯示；滿電熄滅 |
| 電池框及端子 | x=80..99, y=0..8 | 依現有 SOC 填充 |
| 電量數字 | 右對齊到 x=126，y=1 | `0%`～`100%`，1 倍，**不顯示 FULL 文字** |
| 第二行 | x=0, y=16 | `SP 數值` 或 `MAX 數值`，整行 **2 倍字體**，字串 `MAX 30000` 在 x=0..106 |
| BLE / BLE OFF | 沿用原始右下配置 | 不變更 |

### [修改] LOAD 狀態

LOAD 模式主頁移除 `LOADED READY` 與 `HOLD` 的文字呈現，改為 `WAIT LOAD` / `LOADED`。此項為**文字呈現**更動，不修改內部 `hold_active`、MAX 自鎖時長、發射事件、量測流程、APP LOAD 狀態與 Notify。AUTO 模式原有 `AUTO READY`、`IR ERR` / `IR LOSS`、OTA 與 IR TEST 畫面保持原行為。

### [修改] 第二行

非 MAX 從 `RPM` 改為 `SP`；MAX 保留為 `MAX`（**不是** `MAX SP`）。沿用 `oled_draw_text(0U, 16U, value_line, 2U)`，不縮小字體。

### [修改] 電池區

- 右上順序為 `閃電 → 電池 → 百分比`。
- 充電未滿電保留閃電，顯示 SOC（原充電限速 72 秒每 1%、未 FULL 封頂 99%）。
- `brd_battery_is_full()` 確認時顯示 100%、熄滅閃電；不繪製 FULL 字樣。
- 仍保留 `g_frame_full` 快取及變化重繪，以確保充滿當下更新閃電狀態。

## 保留不修改的部分

`BRD_BPX.ino`、`brd_measurement.*`、`brd_capture.*`、`brd_bbp.*`、`brd_battery.*`、`brd_config.h`、`brd_scheduler.*`，包含 RPM ISR/計算、LOAD 去抖、MAX 自鎖、BLE A0（LOAD 切換 + 30 秒電量 Notify）、SOC 21 點曲線、充電 72 秒上升限制、低電 3.46/3.42 V、FULL 4.15 V 判斷等，皆未修改。

## 驗證及限制

- 已以真實 `brd_oled.cpp` 渲染 `wait`、`loaded`、`max`、`hold`、`charging`、`full` 六種情境，並對照預期字型、位置、電池、閃電、數值、BLE 圖示逐像素比較。
- OTA 進度、VERIFY/DONE、IR TEST 既有畫面測試與主程式、BLE、I/O 主機端語法檢查通過。
- BBP 14 種場景、RPM priority 13 種場景，使用實際韌體模組及主機端硬體替身測試通過。
- **尚未進行 ESP32-C3 實機編譯、燒錄、硬體 OLED 或手機 APP 測試，不附新版 BIN。**
