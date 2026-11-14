# IR_Test_OTA — ESP32-C3 BLE OTA 移植說明

## 1. 移植來源及功能

- **原始專案**：`IR_Test(1).zip` → `IR_Test/IR_Test.ino`。
- **OTA 參考**：`BRD_BPX(20261114-182744).zip` 的 `brd_ota.cpp/.h`、`brd_ota_protocol.cpp/.h` 與 OTA V1.2 文件。
- **此交付**：`IR_Test_OTA/` 完整 Arduino Sketch 專案，包含可直接在 Arduino IDE 開啟的 `IR_Test_OTA.ino` 及所有 OTA `.cpp/.h` 原始碼。
- **新增**：NimBLE BLE OTA、原 Web Flash Download Tool 相容的 Discovery/OTA GATT、OLED OTA 扇形進度、電壓低電防護、雙身分標記防止錯燒、協定/Adapter 主機端測試。
- **未移植**：原 BRD_BPX 的 BBP 通訊、轉速量測、歷史紀錄、設定 BLE、充電偵測與 NVS。原 IR1/IR2/OLED/ADC 運作不變。

### 使用的 OTA 協定

| 設定 | 數值 |
|---|---|
| OTA Service UUID | `c7570010-9040-4c7b-87b1-086ef858ace7` |
| Control (Write) | `c7570011-9040-4c7b-87b1-086ef858ace7` |
| Data (Write / Write NR) | `c7570012-9040-4c7b-87b1-086ef858ace7` |
| Status (Read / Notify) | `c7570013-9040-4c7b-87b1-086ef858ace7` |
| 原 Web 工具用的 Discovery Service UUID | `55c40000-f8eb-11ec-b939-0242ac120002` |
| BLE 廣播名稱 | `BEYBLADE_TOOL_BRD_XXXX`（XXXX 為 eFuse MAC 尾四碼） |
| 傳輸 Opcode | `INFO=0x01`、`HASH=0x02`、`BEGIN=0x03`、`END=0x04`、`ABORT=0x05`、`SYNC=0x06`、`DATA=0x10` |
| STATUS 回覆 | 固定 20 bytes，協定版本 1，包含 Offset、Total、Capacity、Max Data、Flags |
| 傳輸逾時 | 15 秒沒有有效資料則中止 |
| Flash Buffer | 4096 bytes |
| 完成後重啟 | 約 5 秒 |

**注意：** Discovery Service 只用來相容既有 Web 工具的掃描篩選，內含一個只讀 `IR_TEST_OTA_V1` 身分文字；**沒有**實作 BBP 轉速/設定指令，不能視作正式 BPX 裝置。

## 2. 完整目錄與檔案

```text
IR_Test_OTA/
├── IR_Test_OTA.ino          # [修改] 原 IR_Test.ino 的完整檔案；新增 OTA 接口、OLED 畫面
├── brd_ota_protocol.h      # [沿用] BRD_BPX V1.2 封包狀態機
├── brd_ota_protocol.cpp    # [沿用] Offset / ACK / 重傳 / 逾時 / 映像檢查
├── ir_test_ota.h           # [新增] OTA 主程式接口與 OLED 畫面狀態
├── ir_test_ota.cpp         # [新增] BLE GATT + Flash/MD5/雙分區更新
├── ir_test_ota_config.h    # [新增] UUID、BLE 名稱、低電門檻設定
├── ir_test_ota_guard.h     # [新增] IR_TEST BIN 身分識別
├── ir_test_ota_guard.cpp   # [新增] IR_TEST BIN 身分識別
├── partitions.csv          # [新增複製] 原 IR_Test 1.25 MiB × 2 分區，沒有擴容
├── tools/
│   └── verify_ota_bin.py   # [新增] 編譯完成後檢查 app BIN 類型、標記與大小
├── tests/
│   ├── run_host_tests.sh
│   ├── test_ota_protocol.cpp
│   ├── test_ir_test_guard.cpp
│   ├── test_ir_ota_runtime.cpp
│   └── stubs/               # ESP32/NimBLE/FreeRTOS 等測試替身
└── README_OTA.md           # [新增] 本操作與差異說明
```

### 原本的硬體 GPIO（完全保留）

| 功能 | GPIO | 修改範圍 |
|---|---|---|
| IR1 數位檢測 | GPIO3 | 未修改腳位、INPUT 模式、HIGH 觸發設定 |
| IR2 數位檢測 | GPIO1 | 未修改腳位、INPUT 模式、HIGH 觸發設定 |
| LED | GPIO8 | 未修改腳位；**僅在 OTA 期間暫停**狀態變化切換 |
| Battery ADC | GPIO0 | 未修改 470k/470k 換算、8+64 筆採樣；OTA 時降低取樣頻率 |
| OLED SDA / SCL | GPIO20 / GPIO21 | 未修改 I2C 400 kHz、128×32 SSD1306 |

**原始註解修正：** 原始碼部分電池分壓說明誤寫 GPIO4，實際 `ADC_GPIO` 一直是 GPIO0；這次只更正註解，不更動任何接腳或倍率。

## 3. Arduino IDE 編譯與首次 USB 安裝

1. 將整個 `IR_Test_OTA` 資料夾完整解壓縮，**不要只取出 `.ino`**。
2. 在 Arduino IDE 開啟 `IR_Test_OTA/IR_Test_OTA.ino`。
3. 使用參考專案相容的 **Arduino-ESP32 Core 3.3.11** 與 **NimBLE-Arduino 2.5.1（2.x）**。
4. 開發板選 **ESP32C3 Dev Module**，CPU Frequency **80 MHz**，Flash Size **4 MB**，Flash Mode **DIO**，Flash Frequency **80 MHz**。
5. 選用 `partitions.csv` 中與原始 IR_Test 相同的 **1.25 MB × 2 OTA** 分區；ESP32 Core 3.3.11 的預設分割配置符合本表，但應再次檢查編譯輸出的 `partitions.csv`。**不要選 Huge APP / No OTA**。
6. 以 Arduino IDE 的 USB 上傳（或 Export Compiled Binary 後使用 USB 完整映像）首次安裝新韌體。**舊版 IR_Test 沒有 OTA 接收服務，無法直接藉由 BLE 更新。**
7. 建置後預期出現 `IR_Test_OTA.ino.bin`（**供 BLE OTA**）及 `IR_Test_OTA.ino.merged.bin`（**供 USB 整機燒錄**，通常 4 MB）。不得混用兩者。

**舊版原始 ZIP 的 `.bin`／`build/` 並非本次更新的輸出，已刻意不納入本交付 ZIP。** 本交付尚未使用 Arduino ESP32-C3 工具鏈完成原生交叉編譯，亦未產出新的可燒錄 `.bin`。

### Flash 分割區（原版沿用）

| 分區 | Offset | Size |
|---|---:|---:|
| NVS | `0x9000` | `0x5000` |
| OTA Data | `0xE000` | `0x2000` |
| APP0 | `0x10000` | `0x140000`（1280 KiB） |
| APP1 | `0x150000` | `0x140000`（1280 KiB） |
| SPIFFS | `0x290000` | `0x160000` |
| Core Dump | `0x3F0000` | `0x10000` |

**警告：** BRD_BPX 參考專案的 APP 分區已擴容至 1984 KiB。**IR_Test_OTA 仍沿用原版的 1280 KiB。** 不可直接沿用參考專案的 `partitions.csv` 或不經確認燒入大於 1280 KiB 的 app BIN。只有 USB 可以改變分割表，BLE OTA 無法改 Partition Table。

## 4. BLE OTA 更新步驟

1. 第一次以 USB 安裝本專案後重新開機。
2. 用電腦 **Chrome/Edge** 或支援 Web Bluetooth 的 Android Chrome 開啟 `https://printfchins.github.io/Web-Flash-Download-Tool/`。
3. 切到 **藍牙 OTA**，選擇新版編譯的 **`IR_Test_OTA.ino.bin`（app-only）**。不要選舊 `IR_Test.ino.bin`、`BRD_BPX.ino.bin`、`merged.bin`、`bootloader.bin`、`partitions.bin`。
4. 電池須有接上且量測值在 **3.65–4.40 V**。因 IR_Test 沒有可靠的充電狀態/SOC 邏輯，這裡不是「SOC ≥ 20%」判定；可在 `ir_test_ota_config.h` 調整閾值。
5. 點選「連接藍牙並開始更新」，選取 `BEYBLADE_TOOL_BRD_XXXX`。本專案廣播參考 Web 工具需要的 Discovery Service UUID，且 OTA Service 授權方式不變。
6. 依照 Web 顯示進度。OLED 出現 `OTA UPDATE`，左下扇形圓盤 0–99% 與中央百分比；校驗階段顯示 `VERIFY`；**只有整個映像通過檢查並切換 APP 分區才顯示 `OTA DONE`／100%**。
7. 大約 5 秒後自動重新啟動。若斷線、低電、更新失敗或逾時，未提交的 APP 不會被設為新的開機分區，重新連線可從頭更新。

**手機限制：** iOS Safari 一般不支援直接 Web Bluetooth，請使用支援 Web Bluetooth 的電腦或 Android 瀏覽器。

## 5. 雙標記安全檢查

- 為維持原 Web 工具的 `validateFirmware()` 格式檢查，保留 `BRD_BPX_OTA_IMAGE_V1_C3`（來自參考 `brd_ota_protocol.cpp`）。
- 此專案另外嵌入 `BRD_IR_TEST_OTA_IMAGE_V1_C3`，**裝置端在提交前必須同時驗證到兩者**。一般 BRD_BPX 映像缺少 IR_TEST 標記，因此會被裝置拒絕。
- 使用 `python tools/verify_ota_bin.py IR_Test_OTA.ino.bin` 預先確認編譯完成的 app BIN 具有兩組標記、正確 ESP32-C3 header 及大小限制。
- **此機制是產品型號防呆，不是加密簽章、身份驗證或防竄改。** BLE 連線沒有驗證身份；若要作為量產產品，需另行規劃簽章、安全開機、配對權限或實體 OTA 啟動條件。

## 6. 檔案修改位置（便於自行比對）

| 檔案 | 位置 | 修改內容 |
|---|---|---|
| `IR_Test_OTA.ino` | include 區（約第 7 行） | `[OTA 新增]` include `ir_test_ota.h` |
| `IR_Test_OTA.ino` | `oledGetGlyph()`（約第 200 行） | `[OTA 新增]` OTA 畫面所需 glyph |
| `IR_Test_OTA.ino` | `oledDraw...()`（約第 270、500 行） | `[OTA 新增]` 扇形圓盤、2x 百分比、OTA 專用畫面 |
| `IR_Test_OTA.ino` | `batteryUpdate()`（約第 481 行） | `[OTA 新增]` 電池取樣與畫面解耦；OTA 時 1 秒取樣 |
| `IR_Test_OTA.ino` | `setup()`（約第 585 行） | `[OTA 新增]` ADC 初次取樣後呼叫 `irTestOtaBegin()` |
| `IR_Test_OTA.ino` | `loop()`（約第 611 行） | `[OTA 新增]` `batteryUpdate()`、`irTestOtaUpdate()`；忙碌期間停 IR LED 切換及調整 delay |
| `ir_test_ota.cpp/.h` | 整份新增 | BLE Server、指令隊列、Flash Sink、安全檢查、重啟 |
| `ir_test_ota_guard.cpp/.h` | 整份新增 | 專屬 IR_TEST firmware tag 辨識 |
| `brd_ota_protocol.cpp/.h` | 整份移植 | 原 BPX V1.2  OTA protocol，核心邏輯不變 |

## 7. 已完成測試／尚未完成項目

在具有 g++ / OpenSSL 的 Linux 測試環境執行：

```bash
cd IR_Test_OTA
bash tests/run_host_tests.sh
```

測試涵蓋：

- 原參考 OTA V1.2 狀態機（封包長度、Session、Offset、Retry、SYNC、Timeout、錯誤映像、電力門檻）。
- IR_TEST 專屬標記橫跨多個 BLE 封包的辨識，拒絕一般 BPX 映像。
- GATT → queue → Flash Adapter 的低電阻止、錯誤 MD5、Flash 讀回損壞、斷線、4096 bytes 分塊及成功分區切換。
- `IR_Test_OTA.ino` 和 OTA 模組在測試 stub 下的 C++ 語法檢查。

**未完成**：Arduino-ESP32 + NimBLE 的 MCU 真正交叉編譯、燒錄板上的 BLE 連線／OTA、OLED 硬體顯示和電池 ADC 校準。請在實機安裝前先完成這些驗證，不應把主機替身測試視為量產驗收。

## V1.2.1 編譯相容性修正（2026-11-15）

- **修改檔案**：`IR_Test_OTA/ir_test_ota.cpp`，`IrTestRequestCallbacks::onWrite()`，原約第 325 行。
- **刪除**：`value.empty()`；**新增**：`value.size() == 0U`。原因：NimBLE-Arduino 2.x `NimBLEAttValue` 沒有 `empty()` 成員，但支援 `size()`。
- **測試加強**：`tests/stubs/NimBLEDevice.h` 改為使用不提供 `empty()` 的 `NimBLEAttValue` 測試替身；`tests/test_ir_ota_runtime.cpp` 新增空封包測試。
- **未修改**：GPIO、IR/OLED/ADC 邏輯、OTA UUID/指令、雙分區及主程式。
- **安裝**：如你的 Arduino 專案放在 `firmware/IR_Test/`，將此 ZIP 的 `IR_Test_OTA/ir_test_ota.cpp` 複製覆蓋到 `firmware/IR_Test/ir_test_ota.cpp` 即可，不需要更動 `.ino` 檔名。
- **驗證狀態**：已通過 Linux g++ 主機測試及語法檢查；未在 Windows Arduino IDE 或 ESP32-C3 實機編譯／燒錄。
