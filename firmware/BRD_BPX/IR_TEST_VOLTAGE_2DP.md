# IR TEST 電壓顯示更新（BRD BPX V1.2）

修改檔案：`BRD_BPX/brd_oled.cpp`

- [新增] `oled_format_ir_voltage()`：直接讀用既有電池模組傳來的 mV，以四捨五入換算到 0.01V，無 BAT 前綴。
- [新增] `oled_draw_right_aligned_text()`：右上角顯示 `4.12V` 類型的數值，向右對齊。
- [修改] `oled_begin_ir_test_frame()`：顯示 `x.xxV`，取代上一版移除的 BAT 文字。
- [保留] OLED 左上 `IR TEST`、IR1 / IR2 間距 10px、DXF 圖形、觸發狀態、BLE 圖示。
- [保留] MAIN 頁電池圖示、ADC 讀取頻率及低電保護。

電壓範例：4021mV → `4.02V`；4124mV → `4.12V`；4125mV → `4.13V`。

更新測試：`tests/render_boot.cpp` 驗證精度、四捨五入及右上字型 framebuffer。

版本維持 V1.2；尚需 Arduino ESP32-C3 實機編譯與驗證。
