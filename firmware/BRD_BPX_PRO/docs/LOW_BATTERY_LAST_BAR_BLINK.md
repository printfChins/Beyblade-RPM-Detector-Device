# BRD BPX PRO — 低電量最後一格紅色閃爍（V1.2）

## 修改目的

移除 20 套低電頁面電池內的驚嘆號，改由最後一格紅色電量閃爍表示低電。

## 完整檔案路徑與更動

- `BRD_BPX_PRO/brd_ui_warning_pages.cpp`
  - `[刪除]` 電池內驚嘆號（垂直線與底部小點）。
  - `[刪除]` 電池外側隨動畫交替變色的小狀態燈。
  - `[新增]` 最後一格紅色電量於偶數幀顯示；奇數幀隱藏。
  - `[保留]` NEBULA～STRIKE 的原有圖形、文字與主題配色。
- `BRD_BPX_PRO/brd_ui_motion.cpp`
  - `[修改]` 低電量頁在低電狀態存續期間持續每 500 ms 交替亮滅，不會於 4 秒後停止。
  - `[保留]` ADC 異常頁原本的 500 ms 動畫與停止策略。
  - `[保留]` 原本非阻塞 `millis()` 調度與 8×16 Dirty Tile 更新。
- `BRD_BPX_PRO/brd_ui_warning_pages.h`
  - `[修改]` 更新 `frame` 參數的註解。
- `BRD_BPX_PRO/tests/test_style_20.cpp`
  - `[修改]` 驗證 20 個風格只有電池最後一格改變，且兩次亮幀內容一致。
- `BRD_BPX_PRO/tests/test_low_battery_blink.cpp`
  - `[新增]` 使用同一份正式動畫 C++ 模組模擬 12 秒（25 次取樣）；確認 500 ms 亮滅、非閃爍時不重傳、每次更新量不超過 1024 bytes。
- `BRD_BPX_PRO/tests/run_motion_tests.py`
  - `[修改]` 納入上述測試。

## 動畫與局部更新

- 低電量頁正常進入：只在第一次送出 160×80 畫面（25,600 bytes，RGB565 不含協議開銷）。
- 之後每 500 ms 交替顯示／隱藏電池中的最後一格紅色條。
- 不使用 `delay()`；控制函式在主迴圈中執行，不從 GPIO ISR 傳輸 SPI。
- 紅色條在單一電池符號中寬 5 px，實際上下範圍隨 20 種佈局變動。Dirty Tile 每次只送出涵蓋該區域的區塊，LCD 的背景、電池輪廓與文字不閃爍。
- 低電狀態解除或進入其他頁面，立刻停止低電頁動畫，維持原有頁面路由。
- **ADC 異常、電池 ADC 的低電門檻、電源保護、BLE、RPM 和 OTA 未修改。**

## 驗證

`python3 tests/run_motion_tests.py` 主機端 14 組測試通過（11 原有回歸 + Dirty Tile + 20 主題 + 低電閃爍）。

`python3 tests/export_warning_previews.py --out <output_dir>` 產生由正式 C++ 繪圖模組輸出的 20 主題 PNG 和 `ALL_LOW_BATTERY.gif`。

注意：主機端測試不是 ESP32-C3 實機編譯或效能驗證。Arduino-ESP32 及面板實測尚待執行。
