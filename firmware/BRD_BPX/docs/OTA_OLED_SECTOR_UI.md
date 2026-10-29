# BRD BPX V1.2 OTA OLED 扇形進度 UI

## 修改檔案與貼放位置

- `BRD_BPX/brd_oled.cpp`：**完整檔案直接替換**；不需要在 `BRD_BPX.ino` 追加程式。
- `BRD_BPX/tests/render_boot.cpp`：更新主機模擬，檢查扇形隨進度增加且 100% 完全填滿。
- `BRD_BPX/tests/run_host_tests.py`：增加 0%、25%、50%、75%、100% 的 framebuffer 輸出。
- `BRD_BPX/docs/OLED_ota_sector_0_to_100_preview.png`：實際 OLED framebuffer 預覽。

## 128×32 OLED 版面

- 第一行 (`y=0`)：`OTA UPDATE`／`VERIFY`／`OTA DONE`；右上保留電量與充電圖示。
- 第二行 (`y=16..31`)：左側圓盤中心 (`13,23`)、半徑 8 像素，尺寸 17×17 像素。
- 第二行 (`y=16`)：`0%` 到 `100%`（字型放大兩倍），以 OLED 水平中心置中；右下保留 BLE 狀態顯示。
- 0%：只顯示圓框；1%~99%：從正上方順時針填充扇區；100%：整個圓盤內部填滿。

## 演算法

圓盤使用內建 `oled_set_pixel()`，採用整數象限與方向比值判定，不額外使用浮點數或動態配置，僅在開始新一幅 OLED 畫面時重畫。

## 維持既有行為

- 韌體版本維持 `V1.2`。
- OTA 傳輸、校驗、完成及重啟狀態機均不改動。
- 一般模式 RPM/MAX、電量與 BLE 狀態 UI 不改動。

## 驗證

於 `BRD_BPX` 目錄執行：`python tests/run_host_tests.py`。已執行八個主機模組測試，OLED 畫面與填充單調性測試通過。

**提醒**：沒有 ESP32-C3 Arduino 交叉編譯器及實機，不宣稱已生成可用的新韌體 BIN 或完成 OLED 實機驗證。交付 ZIP 已排除修改前舊編譯的 `.bin` / `build`，避免誤燒。
