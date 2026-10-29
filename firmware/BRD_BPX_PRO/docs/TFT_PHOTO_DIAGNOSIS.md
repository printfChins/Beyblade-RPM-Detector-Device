# BRD BPX PRO 實機 LCD 白底、文字錯位診斷（照片 2026-10-18）

## 確認與推斷

- 照片可確認：畫面顯示反色（白底）且文字與線條錯位。
- 程式確定性問題：Adafruit ST7735 `INITR_MINI160x80_PLUGIN` profile 交換 invertOnCommand/invertOffCommand，上一版呼叫 `invertDisplay(false)` 會送出物理 `INVON`；故本版使用 `invertDisplay(true)` 送出 `INVOFF`。這是解決白底的優先修正。
- 文字錯位可能與 SPI 時序、大筆 writePixels 資料流、CS 持續接地造成命令干擾有關，照片不足以確定唯一原因，不宣稱已在實機完全修復。
- TFT 原有 RESET=10kΩ 上拉 + 1µF 接地，不修改 RC。

## 韌體修正

1. brd_config.h: TFT_INVERSION_ON 0->1，保留 PLUGIN profile 的 X/Y 位移。
2. brd_config.h: SPI 16 MHz -> 8 MHz；新增 TFT_TRANSFER_ROWS=8。
3. brd_tft.cpp: 每次全幀發送拆成 10 個連續 8 行的 writePixels()。
4. 保留 CS=-1（硬體 GND）、GPIO2 D/C、GPIO20 MOSI、GPIO21 SCLK、GPIO8 背光。

## 實機 A/B 測試

1. 燒錄本版，在正常上電後確認黑底、字體方向及是否仍有斜線錯位。
2. 關機等待至少 10 秒，再做多次冷啟動；另外測試按 MCU EN 重啟（LCD 仍供電）與快速重上電。
3. 若斜線/錯位仍存在，改 brd_config.h 中的 `TFT_SPI_FREQUENCY_HZ` 為 `4000000UL`、`TFT_TRANSFER_ROWS` 為 `1U`，同時將 `tests/stubs/Adafruit_ST7735.h` 的預期數字隨之改動或只進行實機燒錄。
4. 若 4 MHz / 1 行仍亂碼，優先使用示波器量 TFT SCL(GPIO21)、SDA(GPIO20)、D/C(GPIO2)、RESET，確認 SPI Mode 0 時序/訊號過衝、啟動時 D/C 是否漂移。
5. 若出現不固定開機白屏，保持 SYS_POWER 供電與 MCU EN 重啟分開測；CS 接地無法在 ESP32-C3 GPIO21 UART ROM 輸出期隔離 TFT。

## 限制

本版僅做主機端 stub 測試；沒有實機 TFT / ESP32-C3 Arduino core，因此不能保證實際 LCD 畫面已修復，也沒有提供 BIN。
