# BRD_BPX V1.4 — OLED 靜態電池圖示

## 原始碼與修改範圍

- `BRD_BPX/brd_oled.cpp`：[刪除] 黑色掃描線動畫函式與計時常數；[修改] 充電、放電及 FULL 時都依 SOC 畫靜態電池填充。
- `BRD_BPX/tests/render_boot.cpp`：[修改] 使用 13 個不同時點測試充電畫面不變；保留 OLED 主頁與其他頁面 framebuffer 驗證。
- `BRD_BPX/tests/run_host_tests.py`：[修改] 測試說明。
- `BRD_BPX/BRD_BPX.ino`、RPM、BLE、電池演算法、設定檔完全保留。

## OLED 128×32 主畫面

| 顯示項目 | 行為 |
|---|---|
| 狀態文字 | `WAIT LOAD`／`LOADED`；AUTO 模式保留原顯示 |
| 數值 | `SP 15800`／`MAX 18800`，原本 2 倍字型 |
| 電池 | x105～124、y0～8，依 SOC 靜態填充，不繪製百分比數字 |
| 充電閃電 | x96～100、y1～7，充電中、未 FULL 時顯示 |
| FULL | 靜態滿格電池，閃電熄滅，不顯示 FULL 文字 |
| BLE | 保留右下角原有圖示及原本停用顯示 |

## 充電動畫移除

- [刪除] 每 160 ms 移動 1px 的黑線、13 格動畫循環與相關計時常數。
- [修改] 充電中直接呼叫 `oled_draw_battery_icon()`，使用 `brd_battery_get_percent()` 回傳的 SOC 顯示填充程度。
- [保留] 充電閃電顯示條件與 FULL 判定，不改任何實際電量計算或低電鎖定條件。
- [保留] OLED 既有更新排程；裝載、轉速、電池電量變更仍會觸發畫面更新。

注意：右上角不顯示 SOC 數字，但電池圖示仍依 SOC 以靜態填充表示電量；BLE 仍可傳送電池數值。
