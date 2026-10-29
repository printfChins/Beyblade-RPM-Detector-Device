# BRD BPX PRO — GPIO5 Button / SP GRAPH UI

## 功能規格

- GPIO5 = `INPUT_PULLUP`；LOW 為按下；40 ms 穩定按下觸發一次；長按不連發。
- 預設頁 1：MAIN；按一次顯示頁 2：SP GRAPH；再按返回 MAIN。
- OTA、低電、ADC Fault、BLE 指令指定 IR TEST 時，優先顯示這些頁面，忽略按鍵頁面選擇。
- 不需要修改 `BRD_BPX_PRO.ino`。
- 第二頁是**本機即時取樣曲線**，不是 BBP 標準封包的 32 點 Profile，不寫入 Flash。
- 每 100 ms 最多取 1 點，總計最多 50 點（約 5 秒）。顯示 LAST SHOT 直到下一次非 0 RPM 開始。
- 畫面 Y 軸依當次 PEAK 自動縮放；X 軸是 100 ms 取樣點序列（最長約 5 s）。

## 檔案責任

| 檔案 | 功能 |
|---|---|
| `brd_io.cpp/.h` | GPIO5 初始化及按鍵讀取（原有） |
| `brd_ui_input.cpp/.h` | 狀態判斷、去抖、頁面偏好 |
| `brd_ui_curve.cpp/.h` | RAM 曲線取樣及 frozen 結果 |
| `brd_ui_types.h` | Graph 頁面 ID / graph_selected 快照 |
| `brd_ui_core.cpp` | 路由優先順序及 Graph enter/update |
| `brd_ui_page_main.cpp` | 主頁 UI 外觀及 Dirty 狀態 |
| `brd_ui_page_graph.cpp` | 曲線頁全部 UI，局部繪圖 |
| `brd_ui_widgets.cpp/.h` | 共用電池／BLE 元件 |
| `brd_tft.cpp` | 系統狀態轉 UI 快照、按鍵／曲線輪詢 |

## 更新規則

1. 每次頁面切換：`brd_ui_core::enter()` 執行一次 `lcd().fillScreen()`，建立靜態 UI。
2. 圖表同頁新取樣：若 PEAK 不變，僅畫末端新增一段 `drawLine()`；若 PEAK 提高，僅重畫圖表矩形，Y 軸重新縮放。
3. 電池改變：只更新右上角 Widget；即時資料無變動則完全不觸碰 LCD。
4. 量測開始與結束由現有量測模組判斷，**本模組不改動 BLE、MAX、LOAD ADC 或 OTA**。
5. `brd_ui_page_graph.cpp` 不直接讀取 GPIO、ADC 或 BLE；所有繪圖皆走 Adafruit GFX 官方 API。

## 後續新增第三頁

- 在 `brd_ui_types.h` 加入第三頁 ID；於 `brd_ui_pages.h` 宣告新頁面 API。
- 建立獨立 `brd_ui_page_<name>.cpp` 實作 `enter()` 和 `update()`。
- 只在 `brd_ui_core.cpp` 加入註冊與頁面路由，其他頁面不需修改。
- 多頁輪播時，只修改 `brd_ui_input.cpp` 的導航策略。
- 新頁面資料來源放 `brd_tft.cpp` 快照或獨立 UI 資料模組。
- 加入主機模擬的 Dirty 更新測試與實機驗證。

## 實機驗證建議

1. 不按按鈕上電，確認主頁是否正常顯示。
2. 短按一次進 Graph；短按再回 MAIN。連續按住 3 秒僅切換一次。
3. AUTO 與 LOAD 都進行一發量測，確認曲線峰值與轉速變化，完成後保留 LAST SHOT。
4. 測試最長 5 秒後停止新增點；反覆切換頁面不應清除曲線。
5. OTA／低電／IR TEST 頁面優先性、BLE 操作與開機 CS 接地不受影響。

## 交付限制

目前只有主機端測試與 GFX 模擬渲染，尚未進行 ESP32-C3 Arduino 工具鏈完整編譯或實板驗證。
