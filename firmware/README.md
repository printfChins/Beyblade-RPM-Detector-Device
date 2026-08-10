# Firmware

本資料夾包含 **Beyblade RPM Detector (BRD)** 的韌體程式與硬體功能測試程式。

## 目錄結構

```text
firmware/
├── README.md
├── BLE_RPM/
│   └── BLE_RPM_V1.1/
│       ├── BLE_RPM_V1.1.ino
│       ├── brd_ble.cpp
│       ├── brd_ble.h
│       ├── brd_config.h
│       ├── brd_context.cpp
│       ├── brd_context.h
│       ├── brd_io.cpp
│       ├── brd_io.h
│       ├── brd_log.cpp
│       ├── brd_log.h
│       ├── brd_measurement.cpp
│       ├── brd_measurement.h
│       ├── brd_types.h
│       ├── brd_utils.cpp
│       └── brd_utils.h
└── IR_Test/
    └── IR_Test.ino
```

## BLE_RPM

`BLE_RPM` 為 BRD 的主要韌體專案。

目前版本：`BLE_RPM_V1.1`

主要功能：

- 使用 ESP32-C3 執行韌體。
- GPIO0 讀取 RPM IR 感測訊號。
- GPIO1 讀取陀螺裝載與發射狀態。
- 計算即時 RPM、最大 RPM 與發射瞬間 RPM。
- 記錄發射前後的 RPM 曲線資料。
- 使用 BLE 傳送即時 RPM、發射事件與曲線資料。
- 使用 GPIO8 作為狀態 LED。
- 支援充電狀態偵測。
- BLE 裝置名稱使用 MCU ID 後四碼建立唯一名稱。

### 開發環境

- MCU：ESP32-C3
- Framework：Arduino
- IDE：Arduino IDE 2.x
- BLE Library：NimBLE-Arduino

### Arduino IDE 設定

目前主程式所使用的設定：

```text
Board: ESP32C3 Dev Module
Upload Speed: 921600
USB CDC On Boot: Enabled
CPU Frequency: 40MHz
Flash Frequency: 80MHz
Flash Mode: DIO
Flash Size: 4MB (32Mb)
Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
Core Debug Level: None
Erase All Flash Before Sketch Upload: Disabled
JTAG Adapter: Integrated USB JTAG
Zigbee Mode: Disabled
```

### 編譯方式

1. 安裝 Arduino IDE 2.x。
2. 安裝 ESP32 Arduino Core。
3. 安裝 `NimBLE-Arduino` Library。
4. 開啟：

   ```text
   firmware/BLE_RPM/BLE_RPM_V1.1/BLE_RPM_V1.1.ino
   ```

5. 選擇 `ESP32C3 Dev Module`。
6. 依照上方 Arduino IDE 設定完成編譯與燒錄。

## IR_Test

`IR_Test` 為 IR 感測器功能測試程式，用於硬體開發、GPIO 訊號確認與感測器除錯。

主程式：

```text
firmware/IR_Test/IR_Test.ino
```

此程式主要供開發與硬體驗證使用，不代表正式產品韌體。

## 版本管理建議

正式韌體以版本資料夾區分，例如：

```text
BLE_RPM/
├── BLE_RPM_V1.0/
├── BLE_RPM_V1.1/
└── BLE_RPM_V1.2/
```

每個版本資料夾應保留可獨立編譯所需的完整程式碼，避免不同版本之間共用會被修改的原始檔案。
