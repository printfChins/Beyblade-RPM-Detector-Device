# BRD BPX PRO 160x80 TFT 異常圖形診斷

## 對應症狀
使用者實機照片顯示：左邊與上邊有彩色亂點、文字和圓弧斜移變形。不能在未測試的情況下斷定是程式或硬體故障。

## 已修改
1. `brd_ui_dirty_presenter.cpp`：[修改] 仍然使用 GFXcanvas16 + 8x16 Dirty Tile + hash 比對，但不直接呼叫大塊 `writePixels`；改為呼叫 Adafruit 官方 `drawRGBBitmap()`，將像素列分段交給官方驅動處理。
2. `brd_config.h`：[修改] 一般 SPI 傳輸暫降 4MHz，以排查線路時序。初始 profile 改 `INITR_MINI160x80`、反相 false。原 `INITR_MINI160x80_PLUGIN` 檔案仍可切回，對應反相 true。
3. 新增 `examples/ST7735_Offset_SPI_Diagnostic/ST7735_Offset_SPI_Diagnostic.ino`，會輪播：直接繪圖、Tile RGB565、10x10 格線。
4. `tests/test_tile_dirty.cpp`：[新增] 校驗首尾像素與多 tile 色彩資料的正確性。

## 先測官方直接繪圖，再測 Dirty Tile
- 使用 Arduino IDE 另外開啟 `examples/ST7735_Offset_SPI_Diagnostic/ST7735_Offset_SPI_Diagnostic.ino` 編譯燒錄（注意會暫時覆蓋 BRD 主程式）。
- 第一個畫面是 DIRECT GFX：檢查四角色塊、最上、最左兩條 1px 標記與白色邊框是否完整。
- 第二個畫面是 TILE BITMAP：檢查全部 8x16 彩色區塊是否規則排列、有無色條移位。
- 第三個畫面是 GRID 10x10：檢查水平方向和垂直方向是否保持正交。
- 預設 `TEST_PANEL_PROFILE 0`（MINI）；如果出現裁切、顏色不正常、邊界錯位，改為 `1`（PLUGIN）對照再測。

### 判讀
- DIRECT 與 GRID 也壞：優先查 ST7735 面板 profile、SPI 訊號、接地、CS、RESET。
- DIRECT 正常、TILE 異常：優先查區塊傳輸／RGB565 資料順序。
- 三個測試都正常、完整 BRD 仍壞：優先查多任務 SPI 使用、UI Presenter 更新排程與中斷期間的線路問題。
- 只有接近上、左 1～2px 的條帶異常：比較 MINI 與 PLUGIN 的 GRAM 偏移。

## 硬體現況與限制
- LCD CS 直接接地，無法在其他 SPI 事件或 ESP32 ROM 開機 UART 波形時隔離面板。
- GPIO21 同時是 ESP32-C3 UART0 TX 與目前 LCD SCLK，開機階段可能輸出非 SPI 時鐘波形。
- LCD RESET = 10k 拉 3.3V + 1uF 接地：冷開機會產生 RC reset，但快速重開機電容不一定充分放電。
- Adafruit ST77xx `initR()` 會從 `commonInit()` 呼叫 `begin()`，函式庫預設頻率為 32MHz；程式的 `setSPISpeed(4MHz)` 在 initR 之後，僅降低後續像素傳輸時脈。若初始化本身偶發失敗，仍需 CS/RESET/線路或現成驅動的進一步處理。

## 實機優先順序
1. 完全斷電數秒再上電；先燒錄獨立 LCD 診斷程式，錄下三個測試畫面。
2. 再切換 MINI / PLUGIN 初始化對照。
3. 比較 4MHz 與 2MHz；若速率下降改善，量測 SCLK/MOSI/D-C 的邊緣、過衝和接地品質。
4. 若三項測試始終異常，建議 LCD CS 不要直接接地；須由 GPIO 控制，或至少修改硬體讓 RESET 可被 MCU 主動控制。這是硬體建議，不保證能只靠韌體完全解決。

## 驗證限制
主機端回歸測試可以驗證畫布與測試 LCD 取得相同像素，不能保證實體 ST7735 GRAM 偏移正確、SPI 線路訊號品質、實際 CPU 負載。新的 profile 與傳輸路徑需實板驗證。
