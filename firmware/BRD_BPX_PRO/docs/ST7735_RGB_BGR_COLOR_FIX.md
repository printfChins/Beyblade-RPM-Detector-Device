# BRD BPX PRO V1.2 / ST7735 RGB-BGR 色彩反轉診斷

## 症狀

01 NEBULA 繪圖原始碼指定內圈為黃色 (`rgb(255,211,61)`) 與外圈青藍色 (`rgb(75,227,228)`)。若實機顏色相反，通常是面板 RGB/BGR 通道設定不同，並非主題常數寫反。

## 修改檔案

- `BRD_BPX_PRO/brd_config.h`：新增 `TFT_PANEL_BGR_ORDER`，預設 `1U`，可改 `0U` 對照測試。
- `BRD_BPX_PRO/brd_ui_screen.cpp`：在 `setRotation(TFT_ROTATION)` 後，使用官方 `display.sendCommand()` 只覆寫 MADCTL 的 BGR 位元，保留原角度方向位元。
- `BRD_BPX_PRO/tests/stubs/Adafruit_ST7735.h`：主機模擬增加 MADCTL 驗證。
- `BRD_BPX_PRO/tests/test_color_order.cpp`：新增 panel_begin 色彩指令測試。
- `BRD_BPX_PRO/tests/test_adafruit_api.py`：僅放寬 `brd_ui_screen.cpp` 的官方色序設定指令，不放寬其他驅動直接傳輸行為。

## 注意

1. 這項校正修復的是**紅色、藍色對調**。若實機黑白也反色，應另檢查 `TFT_INVERSION_ON`，兩種問題不要混在一起修改。
2. 不更換 `INITR_MINI160x80`，避免 `INITR_MINI160x80_PLUGIN` 的 GRAM 起點不同導致上下左右像素偏移。
3. 此修正不改 RGB565 顏色常數、GPIO、轉速、BLE、OTA、字型或動畫時間。
4. ESP32-C3 Arduino Core 3.3.11 與真實 ST7735S 的結果仍需實機確認。

## 對照方式

1. 燒錄後先觀察 NEBULA 黃色內圈與青藍外圈：應是內黃、外藍。
2. 若仍相反，將 `TFT_PANEL_BGR_ORDER 1U` 改成 `0U` 再燒錄一次。
3. 若不只內外圈，電池藍色或警告紅色也偏色，表示是整體面板色序，不應逐頁交換主題顏色。
