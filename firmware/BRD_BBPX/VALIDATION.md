# BRD_BBP API_V1.1_R4 驗證紀錄

日期：2026-09-21。本次為 OLED 藍牙圖示替換，正式程式僅變更 `brd_oled.cpp`。

## 實際執行

1. 直接讀取使用者附件 `BRD_BLE_OLED(3).zip` 的 `BRD_BLE_OLED/brd_oled.cpp`，移入原生 `oled_draw_bluetooth_icon()`。
2. 比對來源與目標的圖示函式，點陣及座標一致。
3. 執行來源圖示函式與本次 OLED 畫面繪圖函式，輸出 framebuffer。連線／斷線差異與來源圖示逐點一致。
4. 檢視 LOADED READY／RPM 60000 及 HOLD／MAX 60000 預覽。新圖示亮點邊界 x119..125、y17..29；五位數最右像素 x105，互不重疊。電池及充電圖示位於第一行，無遮擋。
5. 逐檔確認其餘正式 `.cpp/.h` 及 `.ino` 與 R3 相同。

預覽：`docs/OLED_BLE_R4_Preview.png`，由實際繪圖程式產生，不是實板照片。

## ESP32-C3 編譯

- Arduino CLI 1.5.1；Arduino-ESP32 3.3.11；NimBLE-Arduino 2.5.1。
- 使用新的 build 目錄、`--warnings all --jobs 2`，退出碼 0，無警告。
- CPU 80 MHz、Flash 4 MB／DIO／80 MHz、Default Partition。
- Flash：602037／1310720 bytes，45%。
- 靜態 RAM：29628／327680 bytes，9%；剩餘 298052 bytes。
- App：602192 bytes；完整映像：4194304 bytes。
- 合併映像的 bootloader（0x000000）、partition（0x008000）、app（0x010000）均與本次編譯輸出逐段一致。

```text
esp32:esp32:esp32c3:JTAGAdapter=builtin,CDCOnBoot=default,PartitionScheme=default,CPUFreq=80,FlashMode=dio,FlashFreq=80,FlashSize=4M,UploadSpeed=921600,DebugLevel=none,EraseFlash=none,ZigbeeMode=default
```

韌體 SHA-256：`6c5953747c473c90616b4701c4e40afad8894f20fa62d21932e378b6aeed3b12`。日誌在 `docs/compile.log`；全檔校驗在 `docs/SHA256SUMS.txt`。

## 驗證範圍

本次完成附件圖示比對、實際繪圖輸出、位置檢視及 ESP32-C3 編譯；未執行實板燒錄或拍攝。本次沒有新增或重跑通訊／曲線測試；`docs/host_tests.log` 與 `docs/VALIDATION_R3.md` 保留 R3 的 7 組通過紀錄，不當成本次新增驗證。

完整 4 MB 映像會覆寫 NVS；保留歷史時使用 Arduino IDE 一般專案上傳並停用全 Flash 擦除。
