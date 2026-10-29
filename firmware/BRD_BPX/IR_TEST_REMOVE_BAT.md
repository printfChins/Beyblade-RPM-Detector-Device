# IR TEST 移除 BAT 顯示（V1.2）

## 原始碼變更

檔案：`BRD_BPX/brd_oled.cpp`

- [刪減] `oled_begin_ir_test_frame()` 的 `BAT x.xxxV` 字串與右上文字繪圖呼叫。
- [刪減] 未再使用的 `oled_draw_right_aligned_text()`。
- [保留] 左上 `IR TEST`、10 px IR 圖示間距、右下 BLE 狀態、DXF 圖形與觸發邏輯。
- [保留] 電池 ADC、電池電壓 API、低電 / ADC ERR 提示、一般 MAIN 頁電池圖示。

## 測試

`tests/render_boot.cpp` 新增檢查 IR TEST 畫面頂部右側空白。

韌體版本維持 V1.2。尚未做 ESP32-C3 實機編譯／燒錄驗證，請重新編譯原始碼後再燒錄。
