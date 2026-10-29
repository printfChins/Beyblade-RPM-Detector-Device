# BRD BPX PRO V1.2 — 三種保留 UI 風格

## [修改] 風格索引

| 新設定 `BRD_UI_MOTION_THEME` | 既有九風格排列位置 | 原始風格編號 | 名稱 |
|---|---|---|---|
| 0 | 第 1 種 | 01 | NEBULA |
| 1 | 第 2 種 | 02 | DIAL |
| 2 | 第 4 種 | 06 | TORQUE |

保留 160x80 ST7735 TFT、原本的 Adafruit GFX 字型、MAX 在 MAIN 內顯示、GPIO5 四頁切換（MAIN / GRAPH / HISTORY / IR TEST）、藍牙與電池符號、低電量最後一格紅色閃爍、OTA、Boot 及 8x16 Dirty Tile。

[刪減] WAVE / ORBIT / VECTOR / RAPTOR / ECLIPSE / FLUX 的場景及各自的主題色定義。

[修改] `brd_ui_style_scene.cpp` 風格映射 0 / 1 / 5（舊內部編號），新版設定連續 0～2；`brd_ui_motion.h` 主題總數為 3。

[保留] `Page::Max` 型別供相容使用，但量測結束後不會由 MAIN 自動跳轉至 MAX。

**注意：** 三風格中的 TORQUE 對應原始編號 06，而非先前已移除的 04 HYPER。

**實機驗證：** 目前僅執行主機端模擬/語法測試，尚未在 ESP32-C3 Arduino Core 3.3.11 和 ST7735 實機完成驗證。
