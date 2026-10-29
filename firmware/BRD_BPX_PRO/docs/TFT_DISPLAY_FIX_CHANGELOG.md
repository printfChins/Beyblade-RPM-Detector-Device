# TFT LCD 顯示修正記錄（160x80 ST7735）

## 2026-10-20 - 現場故障照片對應修正版

- `[修改] brd_config.h` 預設 8MHz → 4MHz，LCD 偏移 profile 由 MINI_PLUGIN → MINI，對應反相由 true → false。不同廠牌面板可能需要使用回原 profile；可透過獨立診斷程式驗證。
- `[修改] brd_ui_dirty_presenter.cpp` 原本 `setAddrWindow()` + `writePixels()` 一次傳輸整塊區域，改為 Adafruit 官方 `drawRGBBitmap()`，函式庫自行處理 RGB565 傳輸與每列資料。
- `[保留]` GFXcanvas16 單一 25.6KB 畫布、8×16 Dirty Tile、相鄰區塊合併、像素差異 hash、非阻塞動畫。
- `[新增] examples/ST7735_Offset_SPI_Diagnostic/ST7735_Offset_SPI_Diagnostic.ino`：三階段實機自測，包含 profile A/B 比較。
- `[新增] tests/test_tile_dirty.cpp`：多區域與畫面首尾像素完整性檢查。
- `[保留]` BLE、OTA、RPM 中斷、ADC、20 套 UI 配色及頁面內容。

## 已知限制
- 不能只根據一張照片就確認硬體偏移或 SPI 傳輸為唯一根因；改完仍須燒錄診斷程式及實機驗證。
- Adafruit ST77xx 官方庫的 initR 呼叫 begin() 時預設為 32MHz，setSPISpeed(4MHz) 放在 initR 後只適用於後續圖像傳輸。CS 接地＋RESET RC 在快速重上電時不一定能重置面板。
- Arduino ESP32 Core 3.3.11 真實編譯環境尚未建立，這版僅完成主機端與 stubs 的 C++ API、邏輯測試，沒有 .bin。
