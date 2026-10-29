# Adafruit ST7735 官方函式庫 LCD 驗證流程

## 1. 最小 LCD 測試（優先執行）

1. Arduino IDE 程式庫管理員安裝 `Adafruit ST7735 and ST7789 Library`、`Adafruit GFX Library` 與 `Adafruit BusIO`（若未自動安裝）。
2. **單獨開啟** `BRD_BPX_PRO/examples/Adafruit_ST7735_PanelCheck/Adafruit_ST7735_PanelCheck.ino`，選擇與 PRO 板對應的 ESP32C3 Dev Module。
3. 連線要求 GPIO2=D/C、GPIO20=MOSI、GPIO21=SCLK、GPIO8=背光；CS 直連 GND；RESET 10 kΩ 上拉/1 µF 下拉。
4. 成功應看到：黑底、白色框線、左上紅、右上綠、左下藍、右下黃、`ST7735` 白字、`160x80 Adafruit GFX` 青字。
5. 連續測試多次冷啟動、ESP32 軟體重啟、快速斷電再上電。記錄哪種情況會錯位、白屏或文字傾斜。

## 2. 若最小程式也錯位

- 先測 `TFT` SCLK GPIO21 與 MOSI GPIO20 腳位是否接反或受到 UART0 活動干擾；開機 ROM 階段 GPIO21 可能輸出 UART0 TX，且 CS 硬接地讓 TFT 沒有隔離手段。
- 查看 3.3V、RESET（10 kΩ / 1 µF）、SCLK 與 D/C 上電波形；雙重初始化不代表 RESET 波形一定正確。
- 將測試程式的 `setSPISpeed(8000000)` 改成 `setSPISpeed(4000000)` 進行 A/B 測試；若時序受限可能改善。若未改善，不應持續盲目下降時脈。
- 若邊緣有固定座標位移，可比較 `INITR_MINI160x80_PLUGIN` 與 `INITR_MINI160x80`；**每次只變更一個參數**，並確認反相/顏色是否一起變動。
- CS 永久 GND 是實際硬體風險。如果對開機可靠度有要求，下一版 PCB 建議使用 MCU 控制 CS，或改良 LCD 上電 RESET 監督電路。

## 3. 若最小程式正常但 BRD 主專案異常

- 先檢查 GPIO20/GPIO21 是否仍被其他 UART/SPI 程式或除錯輸出占用。
- 查看主程式的畫面更新頻率以及上電後 BLE 啟動時序；排除外部擾動。
- 在新專案中不應再出現手動 `writePixels()`、RGB565 原始傳輸、`sendCommand()` 或自行設定 ST7735 寄存器。

**限制**：本次只有主機端語法和行為模擬測試，並未實際編譯 ESP32 Arduino 套件，亦未進行硬體驗證。
