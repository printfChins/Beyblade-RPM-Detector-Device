# BRD BPX PRO — 只保留風格三 TORQUE（2026-10-29）

## 變更項目

- [刪除] NEBULA（舊選單索引 0）以及 DIAL（舊選單索引 1）的 GFX 場景、主題色、警告畫面版型及對應預覽素材。
- [保留] TORQUE（舊選單索引 2／原始設計代碼 06_TORQUE），其所有主畫面與完整頁面配置不變。
- [修改] `brd_config.h` 的 `BRD_UI_MOTION_THEME=0`，且 `brd_ui_motion.h` 的 `THEME_COUNT=1`；新唯一索引 0 即 TORQUE。
- [修改] `brd_ui_motion.cpp` 的 LOADED／LIVE 動畫條件固定為 TORQUE 原本排程，避免索引 0 誤套用其他風格的無限動畫。
- [保留] `style_scene::draw()` 與 `warning_pages::draw()` 原 API，其他模組無需修改呼叫簽名。
- [保留] 原專案中 TORQUE 的 MAX 畫面、狀態提示、IR TEST 圖形、充電亮帶、BLE 顯示與指示條比例（滿刻度 20000 RPM）。此次**沒有改寫畫面樣式**。
- [未修改] `brd_measurement.*`、`brd_bbp.*`、`brd_settings.*`、`brd_battery.*`、`brd_ota.*` 與 `.ino`；不變更 RPM ISR／LOAD／AUTO／BLE／OTA 行為。

## 使用的正式檔案位置

```text
BRD_BPX_PRO/
├── BRD_BPX_PRO.ino          # 主程式，未修改
├── brd_config.h            # [修改] 只選 TORQUE
├── brd_ui_motion.cpp/.h    # [修改] 唯一風格與動畫判斷
├── brd_ui_style_scene.cpp/.h # [修改] 僅 TORQUE 場景
├── brd_ui_warning_pages.cpp/.h # [修改] 僅 TORQUE 警示
├── docs/TORQUE_ONLY/       # [新增] 靜態 PNG 與 GIF
└── tests/                  # [修改] 單風格回歸驗證
```

## 驗證方法

在 `BRD_BPX_PRO` 資料夾執行 `python3 tests/run_motion_tests.py`。
使用主機端 GFX mock 逐畫素比較新舊 TORQUE 的 Main／MAX／Graph／History／IR Test／Low Battery／ADC Fault，結果完全相同。
本次未在實體 ESP32-C3 開發板進行 Arduino 韌體編譯、功耗、SPI 及 RPM 精度實測。

## 預覽

- `TORQUE_ONLY/01_WAIT_LOAD.png` ～ `12_ADC_FAULT.png`：各畫面的原始 160×80 單獨 PNG。
- `TORQUE_ONLY/TORQUE_ALL_12_SCREENS.png`：12 畫面總覽。
- `TORQUE_ONLY/TORQUE_WAIT_LOADED_LIVE_MAX_25FPS.gif`：正式場景 40 ms／幀的狀態預覽。
- `TORQUE_ONLY/TORQUE_4PAGE_TRANSITION.gif`：保留原專案 TORQUE 四頁切換 GIF。
