> [注意] 本文件保留過往 20 風格的設計紀錄，非當前可選主題清單。
> 最新正式風格僅 3 套（NEBULA / DIAL / TORQUE）；請以 `UI_3_STYLES.md` 為準。

# BRD BPX PRO TFT 動畫：20 原始風格／7 頁／Tile Dirty Region

## 原則

- MCU：ESP32-C3-MINI-1-N4；ST7735S 160×80 橫向；8 MHz SPI；CS 永久接地、RESET 外部 RC。
- **保留 Adafruit ST7735 / Adafruit GFX 官方函式庫**。此版本沒有重寫 ST7735 指令、SPI 時序或 reset 驅動。
- 用新 `brd_ui_style_scene.cpp` 移植原始 01 NEBULA ～ 20 STRIKE 的二十種**獨立場景**，不是用 5 種模板換顏色。
- `brd_ui_dirty_presenter.cpp` 管理 160×80 RGB565 GFXcanvas16 及 8×16 Dirty Tile。
- `brd_ui_motion.cpp` 使用 millis() 限速，不在 RPM ISR 內處理顯示，不會解碼 GIF。
- `brd_ui_core.cpp` 已整合頁面路由；GPIO5 Main/Graph/History；OTA 與警告頁優先。
- 參數 BRD_UI_MOTION_THEME：0=01 NEBULA、1=02 DIAL、…、19=20 STRIKE。

## 檔案與呼叫方向

`BRD_BPX_PRO.ino -> brd_tft_update() -> brd_ui_core::render()`

`-> brd_ui_motion::update() -> brd_ui_style_scene::draw(GFXcanvas16)`

`-> brd_ui_dirty_presenter::flush(Adafruit_ST7735)`

`-> Adafruit_SPITFT::setAddrWindow()/writePixels()`

- `[新增] brd_ui_style_scene.cpp/.h`：二十套原始場景／7 頁，不含任何 SPI 函式。
- `[新增] brd_ui_dirty_presenter.cpp/.h`：tile hash、區塊合併、局部傳輸，唯一使用 writePixels 的程式。
- `[重寫] brd_ui_motion.cpp/.h`：非阻塞更新排程、Model/Curve 快照變化檢測。
- `[修改] brd_ui_core.cpp`：初始化時確認 GFXcanvas16 記憶體建立成功，失敗不啟動動畫。
- `[保留] brd_ui_screen.cpp/.h`：官方 ST7735 初始化與顯示物件。
- `[保留] 其他 BPX 模組：量測、BLE、ADC、OTA、NVS 不改資料協議。

## SPI/記憶體預算

- RGB565 畫布：160×80×2=25,600 bytes。
- 16 行傳輸緩衝：160×16×2=5,120 bytes。
- 20×5 個 Dirty tile hash：100×4=400 bytes，加上 valid[] 約 100 bytes。
- 不使用第二張完整 framebuffer，合計約 31.2 KB 加上函式庫/物件等配置開銷。
- Dirty tile 8×16：每 tile 128 pixels=256 bytes。
- 相鄰 tile 同行合併成一個 SPI Window。
- 相同畫面：CPU 不重新呼叫場景 render、SPI 0 bytes（入口頁面以外）。
- 有動畫的主畫面約 120 ms 一次（上限約 8.3 FPS）；HISTORY 約 110 ms，警告 500 ms。
- 8 MHz SPI 實際傳輸速度取決於 Arduino/線材/硬體；全畫面 25.6 KB 純資料傳輸理論下限 25.6 ms，**不是實測時間**。
- 當大量像素連續移動時 Dirty 覆蓋仍可能接近整頁；需實機測試系統負載及 NCS/NimBLE 互斥時序。

## 七頁資料

- 主畫面：原始二十種外觀、即時 SP、裝載狀態、電池圖示與 %。
- MAX：峰值強調，短暫動畫後停止。
- HISTORY：讀最近四筆 NVS/BBP RAM 歷史唯讀快照，進場逐筆顯示。
- GRAPH：使用現有 RAM 50 點曲線與實際 peak，逐段揭示後停在完整曲線，BLE 32 點資料不變。
- IR TEST：原始兩組 IR 頂點原樣，LOAD GPIO1／RPM GPIO4 ADC；主題色**靜態填滿**，觸發獨立。
- LOW BATTERY／ADC FAULT：低頻文字強調動畫。
- Boot／OTA 保留舊模組，未納入這次七頁的動畫最佳化。

## Arduino IDE

1. 開啟 `BRD_BPX_PRO/BRD_BPX_PRO.ino`。
2. 安裝 `Adafruit ST7735 and ST7789 Library`、`Adafruit GFX Library`、`Adafruit BusIO`、`NimBLE-Arduino`。
3. 使用 ESP32 Arduino Core 與原有 BPX OTA 分割表；設定 ESP32C3 Dev Module、4 MB Flash。
4. 編輯 `BRD_BPX_PRO/brd_config.h` 的 `BRD_UI_MOTION_THEME`（0～19）。
5. 不需要修改 `.ino` 或把預覽 GIF 上傳 MCU。

## 驗證層級

- `python tests/run_motion_tests.py`：11 組舊模組回歸 + 2 組新增 Dirty/風格測試 + 主程式語法檢查。
- 每張 ALL GIF 由上面的同一份 `brd_ui_style_scene.cpp` + `brd_ui_dirty_presenter.cpp` 搭配主機端 GFX mock 產生。
- **沒有 ESP32-C3 工具鏈實際編譯、沒有實板 SPI 波形/功耗/IRQ 延遲測試，暫不附 BIN。**
- 此硬體 CS 常低、RESET 10kΩ＋1µF 仍需測試冷開機和快速重新上電；GPIO21 SCLK 與 UART0 TX 可能相互干擾。

## 主機端傳輸量比較（僅基準測試）

測試為 20 主題×7 頁，每頁 20 個固定影格，比較「每幀完整傳送 25,600 bytes」與本版 Tile Dirty 的累積 SPI payload。**不是對上一版韌體的效能量測，也不是 MCU CPU 占用率實測。**

| 頁面 | 20 套平均 payload 減少量 |
|---|---:|
| Main | 80.2% |
| Max | 88.0% |
| History | 91.7% |
| Graph | 93.0% |
| IR TEST | 86.2% |
| Low Battery | 85.9% |
| ADC Fault | 86.6% |

每個主題完整結果在 `docs/spi_transfer_report.csv`。複雜動態背景有可能產生大面積 Dirty，傳輸量會高於靜態風格。CPU 因為要在 GFXcanvas16 重繪場景、運算 Hash，仍需要在實板量測。若想進一步壓低 MCU 占用，下一階段可在頁面模組加上元件級失效標記，減少對 GFXcanvas16 的重畫次數。
