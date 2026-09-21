BRD_BBP API_V1.1_R4 — ESP32-C3 完整 4 MB 映像

[修改] OLED 藍牙圖示改用 BRD_BLE_OLED(3).zip 的原生 7x13 符號。
點陣與位置一致：x119..125、y17..29，已連線顯示、斷線清除。
本次由 R4 原始碼重新編譯。

檔名：BRD_BBP_ESP32C3_4MB_0x000000.bin
位址：0x000000
映像：4194304 bytes，4 MB
CPU：80 MHz；Flash：DIO、80 MHz
Arduino-ESP32：3.3.11；NimBLE-Arduino：2.5.1
SHA-256：6c5953747c473c90616b4701c4e40afad8894f20fa62d21932e378b6aeed3b12

含 bootloader、partition、app 及填補區；app 位於 0x010000。
完整燒錄會覆寫 NVS。保留歷史請使用 Arduino IDE 一般專案上傳，
Erase All Flash Before Sketch Upload 設為 Disabled。

完整原始碼：上一層 BRD_BBP.ino 與全部 .cpp/.h。
修改位置：../CHANGELOG.md；驗證：../VALIDATION.md。
