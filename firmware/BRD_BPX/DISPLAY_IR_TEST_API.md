# IR TEST：BLE 頁面 API（BRD_BPX V1.2）

## 顯示配置

- OLED：128 x 32（沿用原 SSD1306 字型與 BLE 狀態圖示；IR TEST 頁不顯示 BAT 文字或電池圖示）
- MAIN：原轉速主頁，不修改計算功能。
- IR TEST：左上 `IR TEST`，右上角留白；左側 IR2（GPIO1 / LOAD，縱向六邊形）、右側 IR1（GPIO3 / RPM，DXF 六頂點梯形），不顯示 IR1/IR2 文字。
- IR 腳位觸發狀態 `HIGH`：實心；`LOW`：空心；與 `IR_Test.ino` 的觸發判定一致。
- IR2 / IR1 使用 `gpio_get_level()` 讀取既有輸入，沒有重設 GPIO / attachInterrupt；原電池 ADC、低電保護持續正常運作，IR TEST 頁不繪製電壓文字。
- OTA、低電、ADC 錯誤畫面始終優先於 IR TEST；量測、BLE BBP 通知、OTA 與 NVS 保存行為均保持原樣。

## GATT 規格

BLE 裝置名稱與廣播維持原本 V1.2；服務 UUID 維持原設定服務：

- Service: `c7570001-9040-4c7b-87b1-086ef858ace7`
- Display Page Characteristic: `c7570004-9040-4c7b-87b1-086ef858ace7`
- Properties: `READ` + `WRITE`（Write With Response）
- 資料長度：1 byte。
- `0x00` = MAIN；`0x01` = IR TEST。
- 其它值或長度：忽略，保持上一個合法頁面。`READ` 會取得當前有效值。
- 連線中可以立即切換，**不需要 SET/SAVE**，也**不寫入 Flash**。
- BLE 斷線或停止後自動恢復 MAIN；重新連線預設 MAIN，避免診斷畫面長時間占用螢幕。
- 舊 `C1 ...` 設定請求 / `C2 ...` 20 bytes 回覆完全維持不變；只增加 1 個 Characteristic。

## App 使用順序

1. BLE 連上 `BEYBLADE_TOOL_BRD_XXXX`。
2. 發現 Settings Service 與 Display Page Characteristic。
3. Write With Response：`01`，切換 IR TEST。
4. Read：確認回覆 `01`。
5. Write With Response：`00`，切換 MAIN；斷線也會自動恢復 MAIN。

## 原始碼修改位置

- `brd_config.h`：新增 UUID / Page 常數 / IR 觸發電平。
- `brd_io.h`, `brd_io.cpp`：新增不干涉 ISR 的 IR1 / IR2 唯讀介面。
- `brd_settings_ble.h`, `brd_settings_ble.cpp`：增加 1-byte GATT，主頁狀態存 RAM；BLE 斷線復原。
- `brd_oled.cpp`：增加 DXF 六頂點圖形、動態包圍框填色、鏡射對稱與 IR TEST 頁面排版；詳見 `docs/IR_TEST_DXF_CORRECTION.md`。
- `tests/test_ble.cpp`, `tests/render_boot.cpp`：增加頁面 GATT 及 OLED 版面測試。
- `BRD_BPX.ino`、量測與電池底層：**完全保留原版，不需增減程式碼**。
