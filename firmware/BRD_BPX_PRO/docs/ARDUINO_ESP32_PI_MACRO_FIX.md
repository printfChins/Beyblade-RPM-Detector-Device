# Arduino ESP32 Core 3.3.11 編譯修正：PI 巨集衝突

## 問題與根因

- `Arduino.h` 中 `#define PI 3.1415926535897932384626433832795`。
- 原本 `brd_ui_warning_pages.cpp` 使用 `constexpr float PI`，`brd_ui_style_scene.cpp` 使用 `constexpr double PI`。
- C/C++ 預處理器先展開 `PI` 巨集，導致編譯器無法解析本地常數定義，報告 `expected unqualified-id before numeric constant`。

## 修改檔案與放置位置

專案根目錄：`BRD_BPX_PRO/`，在 Arduino IDE 開啟 `BRD_BPX_PRO.ino`。

1. `BRD_BPX_PRO/brd_ui_warning_pages.cpp`
   - [修改] 將 `constexpr float PI` 重新命名為 `constexpr float kBrdPi`。
   - [修改] 更新本檔所有角度換算使用的常數名稱。
2. `BRD_BPX_PRO/brd_ui_style_scene.cpp`
   - [修改] 將 `constexpr double PI` 重新命名為 `constexpr double kBrdPi`。
   - [修改] 更新本檔所有角度換算使用的常數名稱。
3. `BRD_BPX_PRO/tests/test_arduino_pi_compat.py`
   - [新增] 使用與 ESP32 Arduino 相同的 `PI` 巨集，檢查兩個檔案可通過 host g++ 語法編譯。
4. `BRD_BPX_PRO/tests/run_motion_tests.py`
   - [修改] 將此宏定義相容性檢查加入主機端驗證流程。
5. `BRD_BPX_PRO/CHANGELOG.md`
   - [新增] V1.2 編譯修正紀錄。

## 驗證方式與結果

已在主機端執行：

- `python3 tests/test_arduino_pi_compat.py`：PASS。
- `python3 tests/run_motion_tests.py`：11 組原有回歸、3 組新增畫面測試、韌體語法檢查及 PI 巨集相容性檢查：PASS。

**限制：沒有在本環境使用 Espressif Arduino-ESP32 3.3.11 的 RISC-V 交叉編譯器完整編譯，也未在實機測試。**

## 影響範圍

只更改兩個檔案的區域常數名稱，數值與計算公式維持不變。其餘 GPIO、SPI、TFT 畫面、Dirty Tile、BLE、OTA、ADC 與量測流程不變。
