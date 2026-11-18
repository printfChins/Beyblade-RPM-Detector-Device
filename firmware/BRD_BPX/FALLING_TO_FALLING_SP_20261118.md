# BRD_BPX — Falling → Falling SP 取樣與 OLED 顯示修改

修改日期：2026-11-18

## 修改來源

- 原始專案：`BRD_BPX(20261118-052251).zip`
- 專案路徑：`BRD_BPX/`
- 韌體版本：核心 Falling → Falling／OLED SP 修改完成後，依後續需求從 `V1.4.4` 更新為 `V1.5`；BLE 封包格式未變。

## 修改檔案與位置

1. `BRD_BPX/brd_config.h`：`RPM_IR_GPIO` 下方取樣常數改用 `FALLING`，並以 `static_assert` 固定檢查；LOAD 仍維持 `CHANGE`。
2. `BRD_BPX/brd_measurement.cpp`：`rpm_ir_isr()` 改為只接收 GPIO 硬體下降緣中斷，不再讀取事後 GPIO 電位或選擇第一個邊沿極性；`process_rpm_edge()` 固定以下降緣（LOW）事件計算週期；移除已不需要的極性鎖定變數與重置路徑。
3. `BRD_BPX/brd_oled.cpp`：`oled_begin_frame()` 主頁面非 OTA 分支，將即時顯示標籤從 `RPM` 改為 `SP`；完成發射後 `MAX` 保持不變。
4. `BRD_BPX/BRD_BPX.ino`：僅更新主程式檔頭的取樣規格註解，無需修改 `setup()` 或 `loop()`。
5. `BRD_BPX/tests/stubs/Arduino.h`：補齊主機端 `FALLING` 常數。
6. `BRD_BPX/tests/test_auto_baseline.cpp`、`test_hold_dynamic.cpp`、`test_hold_guard.cpp`、`test_load_hold_v123.cpp`、`test_runtime.cpp`、`test_measurement_fixes.cpp`、`run_fix_regressions.py`：將舊的上升緣／自動選極性測試更新為固定下降緣波形，並新增只有上升緣時不得觸發量測的回歸情境。

## 保留項目

- 量測公式：`RPM = 60,000,000 / (相鄰兩次下降緣之微秒間隔) / PULSES_PER_REV`，目前每圈脈衝數 `PULSES_PER_REV=1`。
- 5 筆週期中位數確認、MAX、32 點 Profile、LOAD/AUTO 模式、HOLD、結束判定、OLED 字型／座標、BLE 與 OTA 均不變。
- 「SP」為 OLED 上的文字標籤，本次不加入 RPM → 官方 SP 單位換算。

## 驗證結果

- Host C++ 語法檢查：`brd_measurement.cpp`、`brd_oled.cpp`、`BRD_BPX.ino` 通過。
- 量測相關 Host 執行測試：AUTO、LOAD、HOLD、重裝載、時間回繞、32 點後 MAX、佇列溢位通過。
- `tests/run_fix_regressions.py`：80 個 ASan/UBSan 回歸情境通過，含上升緣忽略、噪訊、假峰值、低轉提早結算、歷史／BLE 恢復。
- 原專案所附測試的其餘 BLE／OTA／電池相關測試亦完成 Host 測試；**尚未在實際 ESP32-C3 硬體上編譯及量測波形**。

## 編譯與燒錄

- Arduino IDE 開啟 `BRD_BPX/BRD_BPX.ino`，其餘 `.cpp/.h` 請維持同資料夾。
- 請使用原專案 Arduino-ESP32 與 ESP-IDF 相容版本重新編譯。
- 為避免誤用原壓縮檔的舊韌體，本次交付包**排除**舊的 `build/` 與 `*.bin`、`*.elf`、`*.map`、`tests/bin` 暫存輸出。
- 因未取得可用 ESP32-C3 編譯工具鏈，未產生新的燒錄 `.bin`。

## 既有歷史文件

`OLED_SP_TO_RPM_CHANGELOG.md` 是上一版的修改紀錄，保留作為歷史文件；本次修改以此文件為準。
