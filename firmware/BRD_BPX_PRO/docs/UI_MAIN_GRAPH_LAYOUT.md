# BRD BPX PRO TFT 160x80 UI 版面調整（2026-10-19）

## 組件版面

| 組件 | 座標 / 區域 | 更新策略 | 來源檔案 |
|---|---|---|---|
| 裝載狀態 | x=0..111, y=0..17 | LOAD / MODE / HOLD / MAX 變更才更新 | `brd_ui_page_main.cpp` |
| Bluetooth 圖示 | x=112..125, y=1..16 | BLE 連線 / 停用狀態變更 | `brd_ui_widgets.cpp` |
| 電池圖示 | x=128..149, y=2..15 | 電量百分比變更 | `brd_ui_widgets.cpp` |
| 充電閃電 | x=151..159, y=1..16 | 充電狀態變更 | `brd_ui_widgets.cpp` |
| SP 標籤 | x=0..30, y=20..64 | LIVE/MAX 切換才更新 | `brd_ui_page_main.cpp` |
| 轉速數字 | x=32..159, y=20..65 | 同位數只更新變動字元 | `brd_ui_page_main.cpp` |
| MODE | x=0..89, y=69..79 | AUTO/LOAD 變更才更新 | `brd_ui_widgets.cpp` |
| BTN:GRAPH | x=105..159, y=69..79 | 切換到 MAIN 頁面時繪一次 | `brd_ui_page_main.cpp` |
| SP GRAPH 圖表 | x=0..159, y=19..67；折線 x=3..157, y=21..65 | 新增線段、PEAK 縮放或 LAST SHOT 完成時更新 | `brd_ui_page_graph.cpp` |
| 曲線頁 PEAK | x=0..97, y=69..79 | 曲線資料更新才更新 | `brd_ui_page_graph.cpp` |

## 裝載狀態標題

- LOAD，未裝載：`WAIT LOAD`。
- LOAD，已裝載：`LOADED`。
- AUTO 模式：`READY`。
- 顯示 MAX 時：`SAVED`。
- MAX 自鎖（HOLD）時優先顯示 `HOLD`。
- 優先順序：`HOLD` > `SAVED` > `READY` > `LOADED`/`WAIT LOAD`。
- BLE 已連線顯示青色符號，等待連線顯示灰色，BLE 停用顯示紅色；不再顯示 `BLE WAIT` 文字。
- 充電狀態為 true 才顯示電池右側閃電符號，無充電時保留背景。

## 曲線滿版邏輯

- LIVE 曲線仍保留 50 個固定時間槽、每 100 ms 最多一點；只畫新增線段。
- LAST SHOT 曲線凍結時，根據實際點數，將第 1 點對齊 x=3、最後一點對齊 x=157。原始 RPM 點數和時間序列不變。
- Y 軸為畫面顯示峰值的動態縮放，x 軸滿版不影響原 BLE 曲線協議、歷史資料。
- 只有峰值變更、重新開始新曲線或由 LIVE 轉 LAST SHOT 時清除「圖表矩形」，不清除整個 TFT。

## 新增頁面規範

1. 每個新頁面單獨實作 `brd_ui_page_<name>.cpp` 的 `enter()`/`update()`。
2. 共用圖示透過 `brd_ui_widgets.cpp/.h` 管理，不讓頁面直接自行實作 BLE/電池圖示。
3. 宣告固定矩形的 Dirty 更新邊界，確保不同 Widgets 的更新矩形不重疊。
4. `brd_ui_core.cpp` 負責路由及切頁全清，頁面更新禁止 `fillScreen()`。
5. 主程式 `BRD_BPX_PRO.ino`、ADC、BLE、OTA、測量邏輯不因 UI 改版而修改。

## 回歸驗證

- `python3 tests/run_host_tests.py`：13 組主機端可執行測試。
- `tests/test_ui_layout.cpp`：確認 BLE、充電、LOAD、SP 及最後一筆曲線滿寬邊界。
- `tests/test_ui_dirty.cpp`：相同數值零繪圖、局部更新、只有換頁才全畫面清除。
- `docs/TFT_main_layout.png`、`docs/TFT_graph_layout.png`、`docs/TFT_waitcharge_layout.png` 為主機端模擬畫面，非實機照片。
- 尚未用 Arduino-ESP32 工具鏈編譯或於 LCD 真實硬體驗證。
