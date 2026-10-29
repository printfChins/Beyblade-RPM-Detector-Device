# BRD_BPX V1.2 + IR TEST 頁面整合

此專案由使用者提供的 `BRD_BPX(9).zip` 開始修改。保留韌體版本字串 `V1.2`；主程式、量測、OTA、電池 ADC、分割表及原 BLE Settings 協議不變。

## 本次修改

- [新增] `brd_config.h`：`SETTINGS_DISPLAY_PAGE_UUID`、`BRD_DISPLAY_PAGE_*`、`IR_TEST_TRIGGER_LEVEL`。
- [新增] `brd_settings_ble.cpp/.h`：Settings Service 增加 1-byte `READ/WRITE`，頁面狀態僅使用 RAM；斷線與 BLE 停止恢復 MAIN，不寫 NVS。
- [新增] `brd_io.cpp/.h`：GPIO3 = IR1（右側梯形）；GPIO1 = IR2（左側六邊形），唯讀 GPIO，**不重新初始化 ISR**。
- [修改] `brd_oled.cpp`：加入 IR TEST 版面、5x7 `S` 字型、填滿/輪廓多邊形；本次依 DXF 修正兩個六頂點圖形與對稱性，移除 BAT 電壓文字與 IR1/IR2 標籤；保留 OLED 分段 I2C 與 OTA / 電池告警優先順序。
- [新增] `DISPLAY_IR_TEST_API.md`：App 用法、GATT UUID 與檔案說明。
- [新增] `tests/test_ble.cpp`, `tests/render_boot.cpp`, `tests/run_host_tests.py`：頁面切換與實際 framebuffer 預覽測試。
- [刪減] 附件原有的 `BRD_BPX.ino.esp32c3.bin`、`build/`：它們是 **修改前的 V1.2 產物**，不能直接用來燒錄本次新增 IR TEST 的韌體。

## 目錄

```text
BRD_BPX/
  BRD_BPX.ino             # 主程式保持原版，不需要修改
    brd_config.h            # 顯示頁面常數與 UUID
    brd_settings_ble.h/.cpp # Display Page Characteristic
    brd_io.h/.cpp           # IR1 / IR2 讀值
    brd_oled.cpp            # IR TEST OLED UI
    brd_battery.h/.cpp      # 原電池電壓 API（未修改）
    brd_ota.*               # 原 OTA（未修改）
    brd_measurement.*       # 原轉速量測（未修改）
    DISPLAY_IR_TEST_API.md  # 新 BLE API 文件
    docs/IR_TEST_*.png      # 4 種 OLED 狀態預覽
    docs/IR_shape_reference.dxf    # 設計圖 DXF 原件
    docs/IR_TEST_DXF_CORRECTION.md # 本次修正驗證紀錄
    tests/                  # 主機端測試
    ...                     # 其他原版檔案
```

## 開發與燒錄

使用 Arduino IDE 打開 `BRD_BPX/BRD_BPX.ino`，按照原 V1.2 所需 ESP32-C3、NimBLE-Arduino、partition 設定重新編譯；**這個 ZIP 不附任何新編譯的 ESP32-C3 .bin**。

- 不需要改 `BRD_BPX.ino`。
- 原 8-byte Settings Request / 20-byte Response 無變化；App 僅需新增 `Display Page` characteristic 操作。
- 畫面控制詳見 `DISPLAY_IR_TEST_API.md`。

## 驗證

使用 `python3 tests/run_host_tests.py`（Linux g++ 與測試 stubs）執行 8 組主機端可執行測試與原本語法檢查，新增 GATT 和 OLED 的測試也通過。**尚未在實際 ESP32-C3 上重新編譯、燒錄或實機測試**。

## 最新圖形校正

- [修改] 移除 IR1 / IR2 文字；右上角不再顯示 BAT 電壓或電池圖示。
- [修正] 依 `docs/IR_shape_reference.dxf` 的 **兩個 6 頂點封閉多邊形** 重新繪製；非五邊形。
- [修正] 填色依輪廓真正高度處理，並校正左右鏡射取整誤差。
- [驗證] `tests/render_boot.cpp` 執行 4 種觸發狀態的像素對稱測試。
