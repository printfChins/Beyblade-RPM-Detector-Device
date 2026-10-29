## 2026-10-19 TFT 主畫面 / Graph 滿版改版回歸

- `python3 tests/run_host_tests.py`：13 組主機端可執行檔全部通過，另含 GFX API / DXF 幾何檢查。
- 新增 `tests/test_ui_layout.cpp` 驗證藍牙圖示位置、充電閃電 Dirty 範圍、裝載狀態、SP 大字及 LAST SHOT 折線終點 x=157。
- 主畫面 SP 單一字元更新、模式變動、電池變動及 Graph 曲線縮放均不觸發全畫面清除。
- 產生 `docs/TFT_main_layout.png`、`docs/TFT_graph_layout.png`、`docs/TFT_max_layout.png`、`docs/TFT_waitcharge_layout.png` 主機端 160x80 畫面模擬預覽。
- 未執行 ESP32-C3 Arduino 工具鏈交叉編譯，也未完成實板驗證。

## 2026-10-19 GPIO5 / SP GRAPH UI 回歸

- 已執行 `python3 tests/run_host_tests.py`，含 12 組主機端測試（新 `ui_input_curve`、`ui_dirty` Graph 測試）。
- [新增] GPIO5 按鍵 40 ms 去抖、長按不重複、遮蔽頁面不切換。
- [新增] RAM 曲線取樣、Peak、完成凍結、下次有效 RPM 才重新開始的邏輯測試。
- [新增] SP GRAPH 進入頁面及增量繪圖，僅切頁會清除全螢幕。
- [新增] MAIN / GRAPH 160x80 Adafruit GFX 主機端預覽 `docs/TFT_main_BUTTON_UI.png`、`docs/TFT_graph_BUTTON_UI.png`。
- [限制] 未完成 Arduino-ESP32 實際編譯、TFT 或 GPIO5 實機測試。

## LOAD ADC 驗證紀錄（2026-10-19）

- 已通過 11 個主機端執行檔與 Dirty Region、BLE、OTA、AUTO/LOAD 狀態機測試。
- 已加入 0.89V/2.65V、遲滯邊界、ADC 失敗超時、共用 ADC1 unit 的測試。
- 無 Arduino ESP32-C3 實際完整編譯及板上 ADC 實測，不能宣稱可直接燒錄通過。
- 使用者尚未確認裝載狀態的電壓極性，目前韌體暫定低有效。

## 2026-10-18 Modular Dirty Region Refactor

- [主機測試] `tests/test_ui_dirty.cpp`：在同頁資料不變時繪圖數量不增加；RPM 一字元、電池、模式、IR、電壓及 OTA 進度的更新不發出全螢幕清除。
- [主機測試] `tests/render_boot.cpp`：以正式 UI Core 模組編譯並生成 Boot/Main/MAX/Low/ADC/OTA/IR 的模擬畫面。
- [靜態檢查] `tests/test_adafruit_api.py`：確認只有 `brd_ui_core.cpp` 的換頁路徑呼叫 `fillScreen()`；其他模組不手寫 LCD SPI 傳輸。
- [限制] 未進行 Arduino-ESP32 實機編譯、LCD 顯示器開機時序及長時間運行驗證。

# BRD_BPX_PRO 驗證紀錄 2026-10-18

## 已完成（主機模擬 / 原始碼檢查）

- `python3 tests/run_host_tests.py`：8 個主機測試可執行程式全部通過。
- 設定模型、LOAD/AUTO RPM 取樣、100% 嚴格百分比終止邏輯、回繞/overflow 等原功能回歸通過。
- BBP GATT 設定、世代隔離、OTA power/safe/MD5/partition guard、未發布歷史快照阻擋 OTA 開始等測試通過。
- `brd_io.cpp`, `brd_tft.cpp`, 主 `.ino` 主機 C++ 語法檢查通過。
- 顯示畫面：RGB565 160x80 開機、主畫面、MAX、LOW、ADC ERR、OTA 0/25/50/75/100%、VERIFY、DONE、IR TEST 4 狀態渲染成功。
- 更新策略：常態變化畫面間隔至少 100 ms，關鍵狀態轉換即時送出，靜止時不重送整幅。
- OTA 扇形 0% 圓心黑、100% 圓心非黑、25% 間隔填滿像素數嚴格遞增。
- IR TEST 六頂點來源與 DXF 左轉 90 度幾何一致。

## 未執行

- Arduino-ESP32 交叉工具鏈實際編譯、產生 `BRD_BPX_PRO.ino.bin`。
- ESP32-C3-MINI-1 實板上電/開機 Strapping、GPIO6 電源自鎖與 GPIO7 IR 開關極性。
- ST7735S 實際玻璃視窗偏移、色序、背光電流、傳輸電氣完整性。
- 實機最高 RPM 轉速 / SPI 同時更新畫面之中斷延遲、BLE OTA 穩定性。

**請勿把主機模擬成功解讀為硬體測試或已產生可燒錄 BIN。**

- [PRO 修改] ST7735 畫面由 Adafruit library 實際負責 SPI 傳輸。主機回歸只 mock 其公開 API；不是對 Adafruit 真實庫的 ESP32 交叉編譯驗證。

## 2026-10-18 橫向深色版驗證補充

- [修改] 橫向 `160×80` 與 `rotation=1` 的初始化參數及亮屏順序已使用 mock 檢查。
- [修改] IR TEST 六頂點 DXF 幾何保持不變，左右位置改為 160×80 畫面，水平距離 10px。
- [限制] RESET 10kΩ / 1µF RC、GPIO21 ROM UART 波形、CS 接地時開機偶發失效，只能透過示波器與實板循環重啟測試確認。


## ST7735 官方 GFX 版本
- `tests/test_adafruit_api.py`：確認只有官方 ST7735/GFX 公開 API，不再走自製像素緩衝/傳輸。
- `tests/render_boot.cpp`：以主機端 GFX 介面替身驗證主畫面及 OTA 填充百分比。
- 新增官方函式庫 LCD 獨立測試草稿 `examples/Adafruit_ST7735_PanelCheck/`（實機未驗證）。
- Arduino-ESP32 真實工具鏈及實機螢幕均尚未驗證。


### IR TEST ADC UI 變更 (2026-10-19)
- PASS：`python3 tests/run_host_tests.py`，15 組主機端 C++ 測試及語法、DXF 幾何、Adafruit-only API 檢查。
- PASS：GPIO0/GPIO1/GPIO4 使用同一個 ADC1 oneshot unit；GPIO4 raw 取樣與資料失效。
- PASS：IR TEST 藍圖/電壓/左右 ADC/中央 IR 獨立 Dirty Region。
- NOT VERIFIED：ESP32-C3 Arduino 工具鏈編譯與實板 ADC、SPI 顯示。


## 2026-10-19 UI 正式整合回歸
- 主機端 15 組測試通過；含新版 Dirty Region、IR ADC、MAX SP 左對齊與 18 主要畫面渲染。
- 未實際以 Arduino ESP32-C3 編譯，也未實機燒錄或顯示測試。
