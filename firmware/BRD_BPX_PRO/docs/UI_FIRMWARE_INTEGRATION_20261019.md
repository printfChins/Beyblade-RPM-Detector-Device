# BRD BPX PRO UI 模組版韌體整合（2026-10-19）

## 基準

- 基底原始碼：`BRD_BPX_PRO_IR_TEST_ADC_Integrated.zip`，韌體版本仍是 V1.2。
- UI 版面依照先前 18 頁預覽、SP 3 號字／MAX 2 號字與最終 `MAX SP` 靠左版本。
- MCU ESP32-C3-MINI-1-N4；LCD ST7735S 0.96 吋 160×80、橫向深色、CS GND、RESET 10 kΩ 上拉 + 1 µF 對地。
- LCD 顯示驅動只使用 Adafruit ST7735 / Adafruit GFX 官方函式庫。

## 修改與刪除位置

| 位置 | 修改內容 |
|---|---|
| `BRD_BPX_PRO/brd_ui_page_main.cpp` | [修改] 標題 2 號字、SP 數字 3 號字、MAX 標籤 2 號字；[刪除] MODE 與 BTN 文字；[保留] Dirty Region |
| `BRD_BPX_PRO/brd_ui_page_graph.cpp` | [修改] X=3..157／Y=24..68 曲線區；[刪除] LAST、PEAK；[新增] 底部左對齊 `MAX SP XXXXX`（標籤 X=4，數值 X=46） |
| `BRD_BPX_PRO/brd_ui_page_ir.cpp` | [修改] 2 號字 IR TEST、右上電池圖示 + 電壓、白色觸發形狀；[新增] LOAD 左、RPM 右 ADC 垂直條及原始數值；[刪除] `ADC 0-4095` 底部欄 |
| `BRD_BPX_PRO/brd_ui_page_ota.cpp` | [修改] OTA 標題、圓形進度條與大字百分比排列 |
| `BRD_BPX_PRO/brd_ui_page_alert.cpp` | [修改] 開機、低電、ADC 警告字級與排版 |
| `BRD_BPX_PRO/brd_ui_widgets.cpp` | [修改] 共用頁面標題統一 2 號字 |
| `BRD_BPX_PRO/tests/test_ui_ir_adc.cpp` | [修改] 更新 IR 元件 Dirty Region 預期範圍 |
| `BRD_BPX_PRO/tests/test_ui_layout.cpp` | [修改] 曲線 Y 範圍；[新增] `MAX SP` 標籤與數字靠左斷言 |
| `BRD_BPX_PRO/tests/test_ir_dxf_geometry.py` | [修改] 新版 IR 圖形原點；[保留] DXF 頂點與旋轉形狀驗證 |

主程式 `BRD_BPX_PRO.ino` 沒有修改，也未修改 BLE／量測／ADC／OTA 傳輸協議模組。

## 主要 UI 版面

- 主畫面：頂部 `LOADED`、`WAIT LOAD`、`HOLD`，右上 BLE、電池、充電閃電；中央 `SP` 與大型轉速。轉速最高常見 6 位使用 `setTextSize(3)`，更長數值自動降字級以避免超界。
- 曲線頁：頂部 `SP GRAPH`；中間 100 ms 取樣的曲線；底部左靠齊 `MAX SP XXXXX`，不再顯示 MODE、PEAK、LAST 與 BTN。
- IR TEST：GPIO1 LOAD ADC 左側，GPIO4 RPM ADC 右側；兩組 0～4095 原始 ADC 值獨立局部更新，讀取無效時顯示 `----`。不變更 GPIO3 的 RPM 脈衝中斷。
- OTA：增量式扇形進度，不因百分比刷新而清除全螢幕。
- 其他：開機、低電、ADC 錯誤畫面更新排版；保留頁面跳轉優先順序。

## 編譯與驗證

1. Arduino IDE 開啟 `BRD_BPX_PRO/BRD_BPX_PRO.ino`。
2. 安裝 `Adafruit ST7735 and ST7789 Library`、`Adafruit GFX Library`、`Adafruit BusIO`，以及原韌體所需的 ESP32 與 BLE 依賴；沿用原專案的 OTA partitions 設定。
3. 主機端回歸測試：在專案根目錄執行 `python3 tests/run_host_tests.py`，已通過 15 組測試與 18 種 UI 畫面輸出。
4. 此環境未提供 ESP32-C3 Arduino 編譯工具鏈，也未完成 LCD 真機測試；不得將主機端測試視為實機編譯成功。

## 硬體未決事項

- LOAD 電壓範圍 0.89～2.65 V；裝載時高低極性仍需實機確認，目前保留專案既有設定。
- LCD CS 直接接地，RESET 採 RC 上電復位；GPIO21 SCLK 的上電干擾風險仍須實機測試。
