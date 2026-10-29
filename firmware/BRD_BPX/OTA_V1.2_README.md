# BRD BPX V1.2 使用與修改說明

## 檔案放置

解壓縮後保留 BRD_BPX 資料夾，Arduino IDE 開啟 BRD_BPX/BRD_BPX.ino。同層保留全部 .cpp、.h 與 partitions.csv；不把模組分散貼入主程式。

[2026-10-15 V1.2 修改] OTA OLED 左側扇形進度與中央百分比畫面使用 `brd_oled.cpp`，不修改 `.ino`。
本次輸出 ZIP 是原始碼交付；不包含輸入 ZIP 中修改前的 BIN/build 舊編譯結果。編譯後才能輸出更新的 app-only BIN。

新增完整模組：brd_ota.h、brd_ota.cpp、brd_ota_protocol.h、brd_ota_protocol.cpp。修改檔案：brd_config.h、brd_bbp.h/.cpp、brd_bbp_session.h、brd_oled.cpp、BRD_BPX.ino。每項修改以 [V1.2 新增]、[V1.2 修改] 或 [修正] 註解標示，刪減原版本字串與原本無 OTA 的執行路徑。

資料夾結構：

- BRD_BPX/BRD_BPX.ino：完整主程式。
- BRD_BPX/brd_ota*.h/.cpp：OTA 模組。
- BRD_BPX/brd_*.h/.cpp：保留並整合原量測、BLE、OLED、電池及設定模組。
- BRD_BPX/partitions.csv：4 MB 雙 OTA app 分區，沿用 NVS 位址。
- BRD_BPX/docs/OTA_V1.2_DESIGN.md：兩端協議。
- BRD_BPX/tests/：主機驗證程式；不屬於 MCU 燒錄映像。

## Arduino 建置

沿用原專案的 Arduino-ESP32 3.3.11 與 NimBLE-Arduino 2.x 環境。最低 ESP-IDF 5.3，選 ESP32C3 Dev Module、CPU 80 MHz、Flash 4 MB、DIO、Flash 80 MHz。保留支援兩個 app 分區的配置，本版自訂 partitions.csv：App 0 / App 1 各 0x1F0000 bytes（1984 KiB），單一程式不可超過此容量。首次安裝新分割表需 USB 更新；詳細步驟見 FLASH_APP_1984_README.md。

[V1.2 修改] 本資料夾 partitions.csv 已移除 SPIFFS，將 App 0 / App 1 各擴大為 1984 KiB，與舊裝置分割表不同。新韌體需自行用 Arduino IDE 編譯及 Export Compiled Binary；本交付不包含舊版 bin，也未冒稱產出已在 MCU 編譯的新 bin。

## 第一次安裝

V1.1 沒有 OTA 接收功能，首次將 1280 KiB App 分割表擴為 1984 KiB 時，必須使用 USB 安裝新的 Partition Table 與 V1.2 App。網站選 USB 燒錄，使用 BRD_BPX.ino.merged.bin，大小須為 4 MB。目前 Web Flash Download Tool 的 USB merged.bin 模式會全擦除，造成設定與歷史紀錄遺失。要保留 NVS，請依 FLASH_APP_1984_README.md 使用不擦除 NVS 的 USB 分區燒錄方式。

## 後續 BLE OTA

1. 在電腦 Chrome/Edge 或 Android Chrome，開啟燒錄網站 HTTPS 頁面。
2. 選藍牙 OTA，上傳 BRD_BPX.ino.bin 這類純 app BIN。不可使用 merged.bin、bootloader.bin 或 partitions.bin。
3. 解除裝載、停止轉動並等待最後一筆歷史保存。SOC 至少 20%，或插上充電且電池讀取正常。
4. 點連接藍牙並開始更新，選 BEYBLADE_TOOL_BRD_XXXX 裝置。
5. 網頁依裝置回覆顯示進度，校驗前最多 99%。裝置驗證 Flash 讀回 MD5、映像與交易連線後，才提交開機分區並回覆完成。
6. OLED 上方顯示 OTA UPDATE、VERIFY、OTA DONE；下方左側顯示扇形填充圓盤，中央顯示 0~100%。成功後約 5 秒自動重啟。

OTA 期間暫停量測，禁止其他 Flash 保存與清除歷史。既有設定 GET 仍可讀取，其餘修改回覆 BUSY。充電過程中已開始的 OTA 保留 BLE，結束後沿用原充電設定。

如果已設定充電關閉 BLE 且尚未連線，請先停止充電或關閉該設定，讓 BLE 能啟動。iPhone/iPad 一般瀏覽器不支援直接 Web Bluetooth。

## 取消與錯誤

傳輸中可取消；等待目前 GATT 操作結束後送 ABORT。校驗/提交階段取消按鈕停用。Notify 遺失會先讀取回覆，必要時重送完全相同的一包；不重複寫入 Flash。

在提交原子檢查前斷線、低電、ADC 故障、Flash 寫入/讀取失敗、MD5 或映像不合格，會中止未提交的更新。取得提交 ticket 後，切換開機分區屬不可取消階段；若這時斷線，網頁不會在沒有完成回覆的情況下顯示成功。

15 秒無有效資料會中止。斷線後不續傳，重新連線從頭更新。雙分區保護傳輸期間的原 app，未新增開機自檢回退、簽章、Wi-Fi 或差分更新。

## 已執行驗證與限制

- 八組 C++ 主機測試，保留原五組並新增 OTA 協議、GATT/Flash 邊界、最後一筆歷史保存。
- AddressSanitizer 與 UndefinedBehaviorSanitizer。主機 tracing 環境不允許 LeakSanitizer，沿用專案 detect_leaks=0。
- 真實 OpenSSL MD5 對應 MCU mbedTLS 邊界；注入錯誤雜湊、讀回損壞/失敗、驗證期斷線/重連及 Flash API 失敗，檢查不切換開機分區。
- 實際 OLED framebuffer 產出 V1.2 開機、傳輸、VERIFY、DONE 圖。
- 網頁十項 Node 協議測試，包含 MTU=23、ACK 遺失、END 回覆讀取、取消及 GATT 不重疊。
- 網頁語法檢查。Playwright 操作測試腳本已提供，但執行環境缺少瀏覽器，未完成瀏覽器操作測試。
- 缺少 ESP32-C3 Arduino 工具鏈及實機，尚未完成 MCU 編譯或硬體 BLE/OTA 實測。

## 主程式貼放位置

使用完整交付專案時，不需另外貼程式。若手動移植，以下為 V1.2 完整主程式位置；配合所有完整模組及 BBP/OLED 修改一起使用：
- BRD_BPX/BRD_BPX.ino 第 22 行：`#include "brd_ota.h"`。
- BRD_BPX/BRD_BPX.ino 第 56 行：`if (!brd_ota_is_busy()) {`。
- BRD_BPX/BRD_BPX.ino 第 61 行：`brd_ota_update();`。
- BRD_BPX/BRD_BPX.ino 第 63 行：`bool ota_busy = brd_ota_is_busy();`。
- BRD_BPX/BRD_BPX.ino 第 64 行：`bool storage_allowed = !ota_busy && brd_measurement_storage_allowed();`。
- BRD_BPX/BRD_BPX.ino 第 76 行：`if (!ota_busy) { brd_settings_update(); }`。
