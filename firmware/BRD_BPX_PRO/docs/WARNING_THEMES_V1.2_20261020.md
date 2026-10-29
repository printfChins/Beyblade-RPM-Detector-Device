# BRD BPX PRO - 20 種主題專屬警告頁（2026-10-20）

## 目的與基準

- [修改] LOW BATTERY / ADC FAULT 不再使用全主題相同版型。
- [保留] 選定的 01 NEBULA ～ 20 STRIKE 原始 20 套主畫面風格、字型、配色。
- [新增] 警告頁獨立模組 `brd_ui_warning_pages.cpp/.h`；僅使用 Adafruit GFX API。
- [保留] `brd_ui_style_scene.cpp` 只是轉交 palette/theme，動畫排程與 SPI Dirty Tile 顯示層保持不變。
- [保留] 電池低電壓判定、ADC 故障判定與所有量測/BLE/OTA/NVS 邏輯，不修改任何閾值。

## 每個風格的警告頁設計

| 編號 | 主題 | 保留於警告頁的主要風格 |
|---|---|---|
| 01 | NEBULA | 雙環軌道與青色強調 |
| 02 | DIAL | 圓弧刻度／指針儀表 |
| 03 | WAVE | 水平掃描與折線波形 |
| 04 | HYPER | 速度隧道透視 |
| 05 | SCOPE | 示波器格線與儀表框 |
| 06 | TORQUE | 左側工業色塊 |
| 07 | ORBIT | 輻射狀刻度與核心點 |
| 08 | ZEN | 最少的水平基線與大量留白 |
| 09 | MATRIX | 矩陣柱狀訊號 |
| 10 | SPECTRUM | 橫向頻譜條與短直條 |
| 11 | VECTOR | 左右工程資訊框 |
| 12 | HORIZON | 透視地平線 |
| 13 | RAPTOR | 傾斜速度線與分段條 |
| 14 | NOVA | 三角向量線 |
| 15 | CIRCUIT | PCB 電路走線與資訊框 |
| 16 | PULSE | ECG 脈衝線 |
| 17 | ECLIPSE | 雙圓環儀表 |
| 18 | FLUX | 分段能量條 |
| 19 | ION | 微粒點狀訊號 |
| 20 | STRIKE | 高速斜向軌跡與競速線 |

## 功能視覺規範

- LOW BATTERY：每套都必須有電池外框、凸起正極、極少量紅色電量、驚嘆號以及充電文字。
- ADC FAULT：每套都必須有感測 IC 的外框、兩側接點、斷線 X，以及 CHECK INPUT / CHECK IR 文字。
- 警告優先於裝飾：字體所在矩形僅遮去底下裝飾線，避免線條穿越字元。
- 顏色使用現有 Theme 背景、前景、accent、muted、warn、red；主題一致但仍保留高辨識警告色。
- 非阻塞：原本 `brd_ui_motion.cpp` 的警告幀更新間隔為 500 ms；只有 1px 的狀態指示點輕微切換。
- 160×80 RGB565，不配置第二張全畫面緩衝。畫面由單張 GFXcanvas16 繪製，由 8×16 Dirty Tile 合併區域傳輸。
- 不在 MCU 上讀取 GIF，GIF 僅用於 PC 預覽。沒有自製 ST7735 底層 SPI 驅動。

## 修改的原始碼

- **[新增]** `BRD_BPX_PRO/brd_ui_warning_pages.h`：主題色與 `Kind`，繪製模組宣告。
- **[新增]** `BRD_BPX_PRO/brd_ui_warning_pages.cpp`：20 套主題的警告頁排版與圖示。
- **[修改]** `BRD_BPX_PRO/brd_ui_style_scene.cpp`：引入新模組，刪除共用的 LOW / ADC 警告畫面程式，轉交選定主題。
- **[修改]** `BRD_BPX_PRO/tests/test_style_20.cpp`：不再假設固定電池座標，驗證每套圖示與小區域動畫。
- **[修改]** `BRD_BPX_PRO/tests/run_motion_tests.py`：把新模組納入測試編譯。
- **[新增]** `BRD_BPX_PRO/tests/export_warning_previews.py`：正式 C++ 模組與 Dirty Tile 輸出同來源 GIF 和 PNG。
- **[保留]** 主程式 `BRD_BPX_PRO.ino`、`brd_ui_motion.cpp`、`brd_ui_dirty_presenter.cpp`、BLE、OTA、ADC、轉速測量、GPIO5 按鍵。

## 預覽

- `docs/animated_previews/ALL_LOW_BATTERY.gif`
- `docs/animated_previews/ALL_ADC_FAULT.gif`
- `docs/animated_previews/ALL_LOW_BATTERY_20_STYLES.png`
- `docs/animated_previews/ALL_ADC_FAULT_20_STYLES.png`

## 驗證

- 11 項既有主機端測試 + Dirty Tile 與 20 風格測試通過。
- 編譯檢查使用桌面 C++ 模擬版 Adafruit GFX，**不等於 Arduino-ESP32 實機編譯**。
- 20 幀傳輸量統計含首次全螢幕刷新，不能當作 MCU CPU 占用率或實體 SPI 時間。
- 尚未燒錄 ESP32-C3，不宣稱實機運作已驗證。
