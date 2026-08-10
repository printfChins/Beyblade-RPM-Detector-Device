BLE_RPM_V1.0 模組化專案

資料夾名稱必須與主程式檔名相同:
BLE_RPM_V1.0/
    BLE_RPM_V1.0.ino
    brd_config.h
    brd_types.h
    brd_context.h
    brd_context.cpp
    brd_utils.h
    brd_utils.cpp
    brd_io.h
    brd_io.cpp
    brd_measurement.h
    brd_measurement.cpp
    brd_ble.h
    brd_ble.cpp
    brd_log.h
    brd_log.cpp

模組用途:
1. BLE_RPM_V1.0.ino
   Arduino setup/loop 與任務執行順序。

2. brd_config.h
   GPIO、RPM、LED、BLE、LOG 等可調參數。

3. brd_types.h
   BRD 狀態列舉與 RPM 曲線 sample 型別。

4. brd_context.h / brd_context.cpp
   跨模組共用執行狀態。

5. brd_utils.h / brd_utils.cpp
   uint16_t 限制、Little Endian 寫入、狀態文字轉換。

6. brd_io.h / brd_io.cpp
   GPIO8 LED 與 GPIO10 充電偵測。

7. brd_measurement.h / brd_measurement.cpp
   GPIO0 RPM、GPIO1 裝載/發射、RPM 計算、狀態機、曲線記錄。

8. brd_ble.h / brd_ble.cpp
   NimBLE 初始化、callback 與 0xA1/0xA2/0xA3/0xB1/0xB2 封包。

9. brd_log.h / brd_log.cpp
   啟動訊息與週期 Runtime LOG。

使用方式:
1. 將整個 BLE_RPM_V1.0 資料夾放入 Arduino 專案目錄。
2. 使用 Arduino IDE 開啟 BLE_RPM_V1.0.ino。
3. 確認已安裝 NimBLE-Arduino。
4. 選擇 ESP32-C3 開發板後編譯上傳。
