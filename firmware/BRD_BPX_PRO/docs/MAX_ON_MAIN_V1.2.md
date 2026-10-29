# BRD BPX PRO V1.2：MAX SP 改為主畫面內呈現

## 需求及行為

- [修改] 九種 UI 在 `measurement.show_max == true` 時，都保留原有的 `Page::Main` 版面，不跳轉獨立 MAX 頁。
- [修改] 顯示文字使用 `MAX SP` 或相同意義的局部標記，數值直接使用 `measurement.value`（量測模組原本已於 `show_max` 時提供最高轉速）。
- [修改] `show_max` 上升／下降緣會立即更新 MAIN 內容，進入 MAX 狀態時短暫播放原風格儀表動畫後停止，不重新呼叫整頁切換動畫。
- [保留] BLE 與電池、低電紅格閃爍、OTA、IR TEST、轉速曲線、歷史紀錄，GPIO5 4 頁切換，以及 8×16 Dirty Tile 局部 SPI 更新。
- [保留] `Page::Max` 型別及舊場景函式只為相容舊測試／外部程式存在；正式 `choose_display_page()` 不會選到該頁。

## GPIO5

MAIN（即時 SP 或 MAX SP）→ GRAPH → HISTORY → IR TEST → MAIN。

## 完整檔案位置及修改內容

- `BRD_BPX_PRO/brd_ui_navigation.h`：[修改] 首頁始終傳回 `Page::Main`，不依 `show_max` 切換 `Page::Max`。
- `BRD_BPX_PRO/brd_ui_style_scene.cpp`：[修改] 九個 MAIN 場景內，顯示 `MAX SP`、保留各風格構圖及主題色、突顯峰值數字；不新增整頁警告。
- `BRD_BPX_PRO/brd_ui_motion.cpp`：[修改] 即時／MAX 狀態變化時重新開始場景影格；MAX 強調動畫最多 9 幀後停止。
- `BRD_BPX_PRO/brd_tft.cpp`：[修改] 移除不可達 MAX 頁的 ACK 分支，但保留原本 generation ACK 條件與其他頁面處理。
- `BRD_BPX_PRO/tests/test_ui_navigation.cpp`：[修改] 驗證 `show_max = true` 時，路由仍為 MAIN。
- `BRD_BPX_PRO/tests/render_main_max.cpp`：[新增] 使用交付的 C++ 主畫面程式，輸出九套風格 LIVE／MAX RGB565 原始像素、逐幀 MAX 場景；驗證頁面路由、右上電池／BLE、回到 LIVE 像素一致性。
- `BRD_BPX_PRO/tests/run_motion_tests.py`：[修改] 加入 MAX 主畫面回歸測試。
- `BRD_BPX_PRO/docs/max_on_main_previews/`：[新增] PNG 與 GIF 預覽（各畫面由 `brd_ui_style_scene.cpp` 產生，不使用 AI 生圖）。
- `BRD_BPX_PRO/BRD_BPX_PRO.ino`：[保留] 完全不需要更改。

## 主機端驗證

執行 `python tests/run_motion_tests.py`（在 `BRD_BPX_PRO` 目錄）結果：

- 12 組量測／ADC／BLE／OTA／導覽回歸測試通過。
- 6 組 UI／Dirty Tile／BLE 圖示／低電動畫／切頁與九風格主頁測試通過。
- 測試九套風格 `show_max` 進出時，頁面皆仍為 MAIN，回到 LIVE 原始像素可重現。
- MAX 畫面右上 BLE 與電池像素未被覆蓋。

注意：主機端 C++ 編譯／模擬通過並不代表 Arduino ESP32 Core 3.3.11 與 ST7735 實板測試完成。尚需以裝置確認實際 SPI 時序、MAX 切換更新延遲及 RTC/OTA 共存情形。
