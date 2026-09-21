# BRD_BBP API_V1.1_R4 修改紀錄

日期：2026-09-21。本次依附件 `BRD_BLE_OLED(3).zip` 替換 OLED 藍牙圖示。

## 完整檔案與放置位置

完整專案位置：`/workspace/scratch/858468988b82/oled_reference/working/BRD_BBP/`。

| 檔案 | 位置 | 標記 | 內容 |
|---|---|---|---|
| `BRD_BBP/brd_oled.cpp` | 第 439 行起 | 新增 | 移入附件的 `oled_draw_bluetooth_icon()`，點陣與繪製座標一致。 |
| `BRD_BBP/brd_oled.cpp` | 第 472 行起 | 修改／刪減 | 已連線時改呼叫附件圖示，刪除 R3 點陣與放大迴圈。 |

提供完整檔案；以本包 `BRD_BBP/` 替換舊專案，根目錄全部 `.cpp/.h` 與 `BRD_BBP.ino` 放在同一層。若只替換原始碼，將完整 `BRD_BBP/brd_oled.cpp` 覆蓋同名檔案即可。

## 主程式

`BRD_BBP/BRD_BBP.ino` 沒有新增程式，不需貼入片段。Arduino IDE 開啟本包主程式即可。

## 圖示來源與差異

來源：附件 `BRD_BLE_OLED(3).zip` 中 `BRD_BLE_OLED/brd_oled.cpp` 第 445 行的函式。原生 7×13，x119..125、y17..29；本次不是將 R3 圖示縮放，而是使用附件的原始點陣與函式。

[刪減] R3 7×7 點陣及每點 2×2 放大方式。

[新增] `docs/BRD_BBP_OLED_ICON_R4.patch`、`docs/OLED_BLE_R4_Preview.png`；R3 報告與驗證分別保留於 `docs/BRD_BBP_API_V1.1_R3_Review.md`、`docs/VALIDATION_R3.md`。

[修改] README、VALIDATION、目前比對報告、檔案清單、編譯日誌、SHA-256、燒錄說明及本次重新編譯的 4 MB 映像。

註解：已逐檔確認正式程式只有 `brd_oled.cpp` 改變；曲線、GPIO、BLE 通訊與主程式沿用 R3。所有歷史 patch／報告中的版本描述僅適用當時版本。
