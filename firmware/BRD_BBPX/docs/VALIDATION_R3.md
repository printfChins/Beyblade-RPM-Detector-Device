# BRD_BBP API_V1.1_R3 驗證紀錄

日期：2026-09-21。預設模式 `BBP_COMPAT_AUTONOTIFY=1`，僅記錄本次實際驗證。

## 曲線回歸

[新增／修改] 對 R2 先執行新規則測試，得到 24 個斷言失敗、退出碼 1，證實舊版仍把兩類重疊週期寫入 Profile。失敗輸出在 `docs/single_curve_red.log`。修改正式程式後通過；既有 32 點案例再以兩段不同轉速核對確實涵蓋 32 個完整週期。

最終執行 `python3 tests/run_host_tests.py`，退出碼 0：

```text
PASS dual_edge
PASS conformance
PASS protocol
PASS session
PASS measurement
PASS transport
PASS subscriber
PASS 7 host test binaries
```

g++ C++17、`-Wall -Wextra -Werror -fsanitize=address,undefined`；`ASAN_OPTIONS=detect_leaks=0`，未宣稱 LeakSanitizer 覆蓋。

| 測試組 | 本次核對內容 |
|---|---|
| dual_edge | 15 案例：兩類整圈 RPM、窄脈波、未裝載、timeout、前 32 圈、批次電位、回繞、雜訊、重載、溢位恢復；另驗證負緣先到、非參考 MAX、首觸發而非首有效值、重載選相反參考、歸零保留本發參考。 |
| conformance | 前 32 點、裝載後起錄／發射保留、獨立代表值、空曲線 NVS 與回繞。 |
| protocol | 17 bytes、LE、raw、ring、B7 checksum、Counter、CRC／範圍／零尾端。 |
| session | 凍結、600 ms 下限、命令、清除阻擋、下一發及回繞。 |
| measurement | 正式狀態機、LOAD 去抖、Capture 串接、前段保留及溢位。 |
| transport | 舊模式 0、NVS 故障／防覆寫、命令與通知重試。 |
| subscriber | 預設通知、頁序／checksum、重訂閱／清除；正式 wrapper 的非參考值提高 MAX 至 15000，但 Profile 仍只有 raw750、1500。 |

硬體邊界使用替身，量測、Session、協定或 BLE 排程執行正式程式。獨立唯讀審查重新執行全部 7 組通過，Critical／Important／Minor 均無發現。

## OLED 繪圖核對

[修改] 連線圖示以每點 2×2 放大，區域由 7×7 改為 14×14，原點 (114,16)。從本次與 R2 實際繪圖函式輸出 framebuffer，查看 LOADED READY／RPM 60000、HOLD／MAX 60000 兩個場景。

新圖示實際亮點邊界 x114..125、y16..29；舊圖示為 x120..125、y24..30。五位數最右像素 x105，第一行電池／充電／HOLD 與圖示不重疊；連線與斷線畫面差異只在該圖示區。

預覽在 `docs/OLED_BLE_R3_Preview.png`。這是繪圖函式輸出，並非 OLED 實板照片。

## ESP32-C3 編譯與映像

- Arduino CLI 1.5.1；Arduino-ESP32 3.3.11；NimBLE-Arduino 2.5.1。
- `--clean --warnings all --jobs 2`，退出碼 0，無編譯警告。
- Flash 602127／1310720 bytes，45%。
- 靜態 RAM 29628／327680 bytes，9%；剩餘 298052 bytes。
- App 602272 bytes；合併映像 4194304 bytes。
- bootloader：0x000000、18688 bytes；partition：0x008000、3072 bytes；app：0x010000。各區均與本次編譯輸出逐段一致。

```text
esp32:esp32:esp32c3:JTAGAdapter=builtin,CDCOnBoot=default,PartitionScheme=default,CPUFreq=80,FlashMode=dio,FlashFreq=80,FlashSize=4M,UploadSpeed=921600,DebugLevel=none,EraseFlash=none,ZigbeeMode=default
```

韌體 SHA-256：`48bd9b49b813e71946a86fa421ff9c97b0b80247d786d5870bbd6cad9c6df471`。日誌在 `docs/host_tests.log`、`docs/compile.log`；全檔校驗在 `docs/SHA256SUMS.txt`。

## 結論範圍

本次需求已通過宿主行為驗證、目標編譯與 OLED 繪圖核對。原 API 30 項表為 29 符合、0 部分符合、1 不符合；剩餘差異是使用者指定的裝載後起錄，而非原建議發射後起錄。

未執行實板燒錄、IR 光學波形與中斷延遲、真實 BLE／官方 App、NVS 斷電試驗。既有 R2 NVS 曲線不重算，格式沒有取樣模式標記；新 Shot 才使用單圈規則。完整 4 MB 燒錄會覆寫歷史；保留歷史應使用一般專案上傳並停用全 Flash 擦除。
