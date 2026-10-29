# V1.2 — U8g2 官方字型取代手工字形

前一版自訂字形方案已淘汰。本版採用現成的 **U8g2_for_Adafruit_GFX**
Arduino 函式庫，不附帶字型檔，也不維護 glyph/bitmap 點陣資料。

## 新增函式庫相依

- 類型：Arduino 第三方函式庫 `U8g2_for_Adafruit_GFX`（維護者 olikraus）。
- 官方來源：https://github.com/olikraus/U8g2_for_Adafruit_GFX
- 已公布版本：`1.8.0`，https://www.arduinolibraries.info/libraries/u8g2_for_adafruit_gfx
- Arduino IDE 1.8.19：**草稿 / Sketch > 匯入程式庫 / Include Library > 管理程式庫 / Manage Libraries**，搜尋 `U8g2_for_Adafruit_GFX` 安裝。
- 若程式庫管理員找不到，從官方 GitHub 下載 ZIP，再用「草稿 > 匯入程式庫 > 加入 .ZIP 程式庫」。
- 本專案原有 `Adafruit GFX Library`、`Adafruit ST7735 and ST7789 Library`、`Adafruit BusIO` 保留。

## 選定的 U8g2 原始字型（無自製字元）

| 用途 | 函式庫內建名稱 | 說明 |
|---|---|---|
| 狀態、數字、單位 | `u8g2_font_7x13_tr` | 7×13 常規清晰字型 |
| 歷史與 OTA 狹窄欄位 | `u8g2_font_6x10_tr` | 6×10 緊湊字型 |
| 大字 / 主 RPM | 原有 Adafruit GFX 內建字型 | 尺寸不變 |

`_tr` 為透明背景子集；正式繪製使用 `font.setFontMode(1)`。
單字字距與 UTF-8 字串寬度由 `getUTF8Width()` 取得。
因 U8g2 的游標採基線定位，`brd_ui_legible_text.cpp` 會加上 `getFontAscent()`，保留原有 UI 左上角定位座標。

## 修改清單

- [刪減] `brd_ui_legible_text.h`：刪除全部手工 ASCII glyph/bitmap 表、最近鄰字形縮放演算法；只保留函式宣告。
- [新增] `brd_ui_legible_text.cpp`：以 U8g2 原廠字型進行文字繪製與寬度計算。
- [修改] `brd_ui_style_scene.cpp`：`width()` 改從真正字型庫取得實際寬度，底部文字 y 座標略上移避免裁切。
- [修改] `brd_ui_warning_pages.cpp`：警告頁同樣使用新文字 API。
- [修改] `tests/stubs/U8g2_for_Adafruit_GFX.h`：僅供 Linux 主機端 CI 的 API 替身，**不是正式字型，也不能用於 Arduino**。
- [修改] `tests/test_legible_text.cpp`、`tests/run_motion_tests.py`：更新測試。
- [保留] 3 種 UI、SPI 8 MHz、50 ms 目標 20 FPS、20,000 RPM 指示條上限、Peak Hold、OTA、BLE、Dirty Tile。

## 驗證限制

Linux 環境沒有 Arduino ESP32 Core 3.3.11 與正式 U8g2 函式庫。
已使用 U8g2 相同 API 的主機端替身測試路由、排程、GFXcanvas16 整合和字串佈局；
**沒有編譯正式 U8g2 字型內容，也沒有實機驗證 7×13 字型逐像素位置**。
建議實機編譯完成後檢查 160×80 畫面的電池百分比、IR TEST 右下 4 位 ADC、MAX、曲線底部數值。
