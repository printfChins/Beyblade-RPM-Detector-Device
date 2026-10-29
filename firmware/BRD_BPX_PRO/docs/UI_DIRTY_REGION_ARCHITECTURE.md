# BRD BPX PRO TFT UI 模組化與 Dirty Region 更新架構

更新日期：2026-10-18

## 目標

- 僅在顯示資料改變時，對「對應元件的矩形區域」繪圖；資料不變時不產生任何 GFX 繪圖寫入。
- 換頁時才呼叫一次 `fillScreen()`，初始化整個新頁面；同頁更新不得全畫面清除。
- RPM 在字元數及字級不變時僅以 Adafruit GFX 不透明文字模式覆寫變動字元。換位數或顏色時只清除數值區。
- OTA 百分比正常遞增時只補畫「增加的扇形」，不反覆清空整個圓圈；進度倒退才清除進度圈區域。
- IR1、IR2、電池、電壓、BLE／MODE 狀態都有獨立重畫區域。
- LCD 驅動**僅使用 Adafruit ST7735 / Adafruit GFX 官方函式庫**。

## 資料流

`BRD_BPX_PRO.ino` → `brd_tft_update()` → `Model` 快照 → `brd_ui::render()` → `page_xxx::update()` → Widget 快取 → Adafruit GFX API → ST7735 SPI。

`brd_tft.cpp` 只是相容上層原有入口的資料轉接層，不包含 UI 排版、SPI 或繪圖指令。

## 檔案與責任

| 檔案 | 職責 |
|---|---|
| `brd_ui_types.h` | `Page` 頁面 ID、`Model` 畫面快照資料 |
| `brd_ui_screen.h/.cpp` | LCD 唯一實例、初始化、背光、受限矩形清除、文字工具 |
| `brd_ui_core.h/.cpp` | 頁面優先順序、換頁偵測、執行各頁 enter/update |
| `brd_ui_widgets.h/.cpp` | 共用電池、充電、BLE、模式等小元件及其變動快取 |
| `brd_ui_pages.h` | 各個頁面的 `enter()` / `update()` 統一介面 |
| `brd_ui_page_main.cpp` | CURRENT SP / MAX SP / STATUS 主頁 |
| `brd_ui_page_ir.cpp` | IR TEST、電壓、IR1/IR2 |
| `brd_ui_page_ota.cpp` | OTA 進度與狀態 |
| `brd_ui_page_alert.cpp` | 開機、低電、ADC 故障等靜態警告頁 |
| `brd_tft.h/.cpp` | 接回既有測量、BLE、OTA 程式的相容入口 |

## Dirty 判定規則

1. 頁面改變：在 `brd_ui_core.cpp` 的 `enter()` 內，先清整頁，呼叫新頁 `enter()` 畫固定背景，再執行新頁 `update()` 畫動態資料。
2. 頁面未改變：只執行該頁 `update()`；每個元件先比較上一筆已呈現資料的快取，只有變化才清除自己的 `Rect` 後繪圖。
3. 靜態元件不在 `update()` 內重畫；例如頁首文字、分隔線、面板邊框。
4. 每個 `Rect` 必須包住其文字縮放後的完整筆畫，同時**不可碰到鄰近元件的固定圖形**；有重疊層級時需把有關元素改成同一個 Widget 整組重畫。
5. 比較的是已呈現的值，不是每次從量測模組讀到的上一個值；頁面重進時必須將所有 Widget 快取標為無效。
6. 變更顯示顏色、字級、位數時，需清除整個原數字元件區域；等寬且同色的字元可以逐字元覆寫。
7. OTA 等非純文字圖像應優先做差分繪圖，降低動態重繪面積。
8. 畫面重試/面板重初始化後，`valid_page=false`，下次重新進頁，不沿用前一次舊像素快取。

## 新增 UI 頁面步驟

1. 在 `brd_ui_types.h` 的 `Page` 列舉增加新頁面 ID；如需新資料欄位，加到 `Model`。
2. 新建 `BRD_BPX_PRO/brd_ui_page_<name>.cpp`；在 `brd_ui_pages.h` 加入 `enter()` 和 `update(const Model&)` 宣告。
3. `enter()` 只繪製固定元件、重置每個 Widget 的變化快取。
4. `update()` 先比較快取，變化時呼叫 `clear_region(Rect)` 清理局部區域，透過 `lcd().drawXXX()` / `label()` 重新畫該區，最後更新快取。
5. 在 `brd_ui_core.cpp` 的 `choose()` 加入切頁規則，並在 `enter()` / `render()` switch 註冊新頁面。
6. 外部操作/量測資料一律由 `brd_tft.cpp` 的 `snapshot()` 提供；新增 GPIO、BLE 操作不應直接寫進 UI 繪圖檔。
7. 在 `tests/test_ui_dirty.cpp` 加入新頁面「同資料完全不重畫、單一欄位變更不清全頁、換頁只清一次」測試。

## 既有介面與相容性

- 保留 `brd_tft_begin()`、`brd_tft_update()`；主程式 `BRD_BPX_PRO.ino` 不需修改。
- AUTO/LOAD、MAX generation ACK、IR TEST、BLE、OTA 等原有資料來源和流程不變。
- LCD CS 永久接地、RESET 為 10 kΩ 上拉 + 1 µF 對地；啟動時序仍需實機確認。
- GPIO2(D/C)、GPIO20(MOSI)、GPIO21(SCLK)、GPIO8(背光) 與 160x80 橫向、8 MHz 不變。
- 沒有建立全畫面自製 framebuffer；使用 Adafruit GFX 官方 API。

## 測試與限制

- `python3 tests/run_host_tests.py` 可驗證主機端邏輯及 ST7735 模擬器，`test_ui_dirty.cpp` 可檢測繪圖次數與不必要的全螢幕清除。
- `tests/bin/` 是主機端測試產物，不屬於燒錄韌體，交付 ZIP 會排除。
- 主機端通過不能替代 Arduino-ESP32 真正編譯、實機 SPI 驗證或 BLE/OTA 壓力測試。
- 這是**元件/矩形區域級**的 Dirty 更新，不是逐像素 framebuffer 差分；對於完全重疊或動態透明圖層，應建立合併元件或追加區域裁剪策略。
