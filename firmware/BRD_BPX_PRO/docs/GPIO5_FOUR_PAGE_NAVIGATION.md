# BRD BPX PRO — GPIO5 短按四頁切換

## 頁面順序

1. Main (測量結束時顯示 MAX)
2. SP Graph
3. History
4. IR TEST
5. 再回到 Main

## 硬體與操作

GPIO5 = `INPUT_PULLUP`，按下接地 = LOW。沿用 `brd_io.cpp` 設置 GPIO。

- 短按：按下超過 40 ms，放開再穩定 40 ms，且按住不超過 800 ms，切換一次。
- 長按（超過 800 ms）：不切頁、不連發。
- 除去抖動外不使用 `delay()`；每輪主迴圈呼叫 `input_update()`。
- OTA、低電量、ADC 故障與 BLE 指定的 IR TEST 頁面優先；遮蔽時不補觸發舊事件。
- 本地第四頁 IR TEST 使用 GPIO1 LOAD ADC 與 GPIO4 RPM ADC，包含首次切入即取樣。
- 不變更原本 GPIO3 RPM 中斷、BLE/OTA、歷史 NVS 資料格式與 Dirty Tile。

## 修改檔案

- `brd_config.h`：新增短按時間上限與本地頁數。
- `brd_ui_input.cpp/.h`：四頁短按狀態機。
- `brd_ui_navigation.h`（新增）：獨立頁面優先權路由。
- `brd_ui_core.cpp`：改用獨立路由函式。
- `brd_ui_types.h`：本地頁索引說明。
- `brd_tft.cpp`：本地進入 IR TEST 時即時取得兩組 ADC。
- `tests/test_ui_input_curve.cpp`：四頁按鍵與曲線資料回歸測試。
- `tests/test_ui_navigation.cpp`（新增）：獨立頁面優先權測試。
- `tests/run_motion_tests.py`：納入路由測試。

## 驗證注意

主機端測試不能取代 Arduino ESP32-C3 3.3.11 實際編譯及實機 GPIO5 消抖測試。
