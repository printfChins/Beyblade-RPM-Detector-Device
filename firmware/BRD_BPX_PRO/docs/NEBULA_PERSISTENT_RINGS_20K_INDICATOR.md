# BRD BPX PRO V1.2 - NEBULA 常駐雙環與三風格 RPM 指示條

## 變更範圍

修改基準：`BRD_BPX_PRO_V1.2_3Styles_8MHz_20FPS.zip`

- [修改] NEBULA（風格索引 0）的藍色外圈與黃色內圈**底環**在 WAIT LOAD、LOADED、LIVE、MAX 期間都顯示。為避免與啟動動畫混淆，底環使用較低亮度的對應顏色。
- [保留] LOADED 後黃色亮色特效弧持續旋轉；發射進入 LIVE 後持續旋轉；MAX 完成時停止旋轉，暗黃色底環不消失。
- [修改] NEBULA 的亮青藍色 RPM 進度弧僅於有效 LIVE/最終 MAX 顯示，隨本次最高 RPM 增加，達到滿格後不再加長。
- [新增] 全部三風格共用 `BRD_UI_INDICATOR_FULL_SCALE_RPM`，預設 20000 RPM = 100%。
- [保留] DIAL 原指針與刻度配置、TORQUE 原 16 段柱、NEBULA 原圓環構圖。
- [保留] 8 MHz SPI / 50 ms 主畫面動畫間隔、4 頁切換、Dirty Tile。

## 設定方式

檔案：`BRD_BPX_PRO/brd_config.h`

```cpp
/* [新增] 三種主畫面 RPM 指示條滿格基準 */
#define BRD_UI_INDICATOR_FULL_SCALE_RPM   20000UL
```

可改為 `15000UL`、`25000UL`、`30000UL` 等正整數；每次修改後需要**重新編譯與燒錄**，不是 BLE 動態參數。

轉速對應：

| 實際最高 RPM | 滿格設 20000 時的指示比例 |
| ---: | ---: |
| 0 | 0% |
| 5000 | 25% |
| 10000 | 50% |
| 15000 | 75% |
| 20000 | 100% |
| 25000 | 100%（飽和） |

比例是 UI 顯示比例，不是轉速量測的上限：數字仍顯示例如 25000 RPM；曲線頁仍依曲線資料峰值獨立繪製；RPM 偵測、發射判定、BLE 資料、保存歷史都不變。

## 修改檔案

- `BRD_BPX_PRO/brd_config.h`：新增 CFG 滿格常數及非零檢查。
- `BRD_BPX_PRO/brd_ui_style_scene.cpp`：滿格比例由寫死 32000 改成 CFG；NEBULA 常駐雙環。
- `BRD_BPX_PRO/brd_ui_style_scene.h`：修正三風格檔頭註解，無 API 變更。
- `BRD_BPX_PRO/tests/test_nebula_three_phase.cpp`：修改雙環回歸斷言。
- `BRD_BPX_PRO/tests/test_ui_indicator_full_scale.cpp`：新增三風格比例及飽和測試。
- `BRD_BPX_PRO/tests/render_indicator_20k.cpp`：新增同源 GFX PNG/GIF 影格輸出測試程式。
- `BRD_BPX_PRO/tests/run_motion_tests.py`：納入新測試。

## 驗證限制

目前已執行主機端語法檢查、12 組量測/BLE/OTA 等功能回歸及 9 組 UI 測試，包含三風格指示條飽和測試。

此處尚無 ESP32-C3 Arduino Core 3.3.11 的實際編譯及 ST7735S 實機驗證。20 FPS 是 50 ms 的排程目標，非實測幀率。
