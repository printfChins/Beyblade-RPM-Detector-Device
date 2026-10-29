# BRD BPX PRO TFT CS 接地／RC RESET 開機可靠度檢查

## 已確認的電路

- TFT：GMT096-04-WG，ST7735S，原生 80×160，韌體顯示 160×80 橫向。
- CS：直接接地，ST7735S 始終被選取，韌體不控制 CS。
- RESET：10kΩ 上拉 3.3V、1µF 接地，RC 時間常數 10ms。
- D/C：GPIO2，具 10kΩ 上拉，ESP32-C3 strapping。
- SCLK：GPIO21，與 ESP32-C3 ROM 的 UART0 TX 預設功能重疊。
- MOSI：GPIO20。
- 背光：GPIO8，ESP32-C3 strapping。

## 本次韌體處理

1. `brd_tft_begin()` 啟動時，先等 `TFT_POWER_STABLE_WAIT_MS=180ms`。
2. 設定背光 GPIO8 為關閉狀態。
3. 啟動 SPI（SCK=21、MOSI=20），進行第一次 `Adafruit_ST7735::initR(INITR_MINI160x80_PLUGIN)`。
4. 間隔 `TFT_REINIT_GAP_MS=80ms` 後，第二次呼叫 `initR`。
5. 設定 SPI 16MHz、旋轉=1（橫向）及 inversion 狀態。
6. 將首次完整畫面寫入 GRAM 後才開啟背光。

注意：雙重軟復位與等待可能降低干擾，但 LCD 無 MISO，也沒有由 MCU 控制的 RESET 腳；韌體無法知道 LCD 是否真的收到命令。

## 建議實板確認次序

1. 先觀察「畫面空白時 BLE 是否仍能連線」，區分 MCU 開機失敗與 LCD 單獨失效。
2. 示波器同時量測 TFT VDD 與 TFT RESET：冷開機、MCU EN 軟重置（LCD VDD 不斷電）、快速關機再上電、長時間關機再上電。若 RESET 在快速循環時未拉低，RC 復位未重新發生。
3. 示波器量測 SCLK(GPIO21) 與 D/C(GPIO2)，確認 3.3V 供電穩定前／TFT 初始化前有沒有 UART0 TX 波形進入面板。
4. 選擇每種重啟模式至少 30 次，逐次記錄是否顯示開機動畫、有否偏移／白屏／亂碼。
5. 若韌體初始化後仍偶發不正常，考慮先實測降低 SPI 時脈、改善 RC 放電與上電時序。單改軟體不能切斷 CS 接地時的非預期時鐘。
6. 下一版 PCB 若可調整硬體：優先考慮使 LCD CS 可控制、或加獨立 reset supervisor／MCU 可控的 RESET；在不增加新的 GPIO 前勿直接改造現有 pinout。

## 重要限制

- ST7735S RESET 是低有效；10kΩ/1µF 只提供類比上電延遲，不能視為精準 10ms 數位復位脈衝。
- ESP32-C3 GPIO2 / GPIO8 的 strapping 是上電採樣；軟體延遲無法修復開機時外部電路拉錯電位。
- Adafruit `initR()` 自己會執行其初始化流程；`setSPISpeed(16000000)` 在初始化後設定，主要控制一般畫面更新速率，不代表初始化所有命令必定都在 16MHz 下傳送。
- 本次沒有 ESP32-C3 實機、示波器資料，也未進行 Arduino Core 真實交叉編譯。
