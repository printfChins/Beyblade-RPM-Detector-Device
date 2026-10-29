# OTA 百分比字型修正（V1.2）

## 修改原因

OLED OTA 進度數字 `0%`～`100%` 原先使用 `oled_get_glyph()` 的 5x7 `%` 字元，再以 `oled_draw_char()` 放大 2 倍。上下圓圈由 2x2 像素轉為實心的 4x4 方塊，視覺上近似 `./.`，而不是空心圓圈。

## 變更位置

- `BRD_BPX/brd_oled.cpp`
  - [新增] `oled_draw_char()` 遇到 `c == '%' && scale == 2U` 時使用獨立 10x14 點陣。
  - [修正] 上方與下方為各自封閉、有黑色中心的空心圓圈，中間是斜線。
  - [保留] 其他字型、OTA 左側扇形進度圓盤、IR TEST 的 10px 圖示間距與 `x.xxV`、BLE／OTA／量測邏輯。
- `BRD_BPX/tests/render_boot.cpp`
  - [新增] 對 OTA 0%、25%、50%、75%、100% 的 `%` 點陣做封閉圓圈、黑色中心、斜線位置的 framebuffer 像素檢查。

## 驗證

`python3 tests/run_host_tests.py`：八組主機端測試與其他附加檢查通過。OLED 測試圖片直接從 `render_boot.cpp` 畫出的 128x32 framebuffer 生成。

本包保留編譯版本字串 V1.2，不附修改前的 `.bin` 或 host 測試可執行檔。需在實際 ESP32-C3 編譯後驗證螢幕。
