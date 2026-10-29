# 專案驗證紀錄

## V1.3 版本更新 — 2026-10-25

- [修改] `brd_config.h`：韌體版本 `V1.3`，派生名稱 `BPX V1.3`、OLED 開機文字 `FW VER V1.3`。
- [修正] `tests/render_boot.cpp`：保存並還原真正開機 framebuffer，避免 OTA 扇形驗證覆蓋開機預覽；韌體顯示流程沒有因此變更。
- [驗證] 重新執行 `python3 tests/run_host_tests.py`，12 個 host executables、13 個 RPM 優先案例、六個 BLE 連線案例、主程式語法檢查全部通過，ASan/UBSan 啟用；完整輸出見 `docs/V1_3_TEST_RESULTS.txt`。
- [驗證] 重新產生並目視檢查 `docs/OLED_boot_preview.png`，上方 BRD BPX、下方 FW VER V1.3 完整顯示。
- [限制] 尚未以 ESP32-C3 工具鏈編譯或實機燒錄，不附新版 BIN。下方紀錄保留當時歷史版本。

---

## V1.1 充電關閉 BLE 預設停用 — 2026-10-07

- [修改] `brd_config.h` 的 `BLE_OFF_WHILE_CHARGING` 改為 `0U`。
- [驗證] 既有 `test_runtime.cpp` 確認沒有 NVS 設定時，執行值為 `0`，充電時允許 BLE；測試再明確啟用功能，確認充電停用，並停用確認充電保留 BLE。
- [驗證] 重新執行 `python3 tests/run_host_tests.py`，五個主機測試與 BBP、I/O、主程式語法檢查全部通過。
- [保留] `brd_settings.cpp` 的 Flash 載入流程未修改，已保存的設定仍優先於編譯預設值；API 參數 `04=0` 後 SAVE 可套用到已有裝置。
- [限制] 未執行 ESP32 target 編譯或實機燒錄。

## V1.1 BLE / OFF 兩行小字 — 2026-10-07

- [驗證] 重新執行 `python3 tests/run_host_tests.py`，五個主機測試與 BBP、I/O、主程式語法檢查全部通過。
- [驗證] 重新渲染八種 OLED 狀態，只有充電停用 BLE 的正常／MAX 畫面更換為兩行小字，其餘畫面與上一版一致。
- [驗證] 小字範圍 x111..127、y16..30；`BLE` 與 `OFF` 完整顯示，五位數 RPM／MAX 與文字保持間距。
- [修改] `docs/OLED_charging_ble_off_preview.png` 更新為兩行小字的實際 framebuffer。
- [限制] 未執行 ESP32 target 編譯或實機燒錄。

## V1.1 充電關閉 BLE 圖示 — 2026-10-07

- [驗證] 重新執行 `python3 tests/run_host_tests.py`，五個既有主機測試執行檔及 BBP、I/O、主程式語法檢查全部通過。
- [驗證] 使用真正 `brd_oled.cpp` framebuffer 渲染八種狀態：未連線、已連線、充電保存回覆期間已連線／未連線、充電關閉 BLE、充電 MAX、停用充電關閉設定、拔除充電後已連線。
- [驗證] 模擬時間固定在 2500 ms，僅切換 BLE 已停止狀態時新圖示即出現；切換充電關閉設定為停用時即清除。確認不依賴等待 100 ms 更新週期。
- [驗證] 原圖示區 29 個亮點，新停用圖示區 32 個亮點；其他狀態與原程式 framebuffer 一致。MAX=30000 文字未與新圖示重疊。
- [修改] 新增 `docs/OLED_charging_ble_off_preview.png`，以最近鄰放大實際 128×32 畫面供排版檢查。
- [限制] BLE 啟動／連線、充電輸入及 I2C 使用主機替身；未執行 ESP32 target 編譯或實機充電切換驗證，未產生新 BIN。

## V1.1 版本更新 — 2026-10-07

- [修改] 韌體版本更新為 `V1.1`，OLED 開機字串同步為 `FW VER V1.1`。
- [驗證] 使用 C++17 編譯期斷言確認 `PROJECT_FIRMWARE_VERSION`、`PROJECT_VERSION`、`PROJECT_BOOT_VERSION` 分別為 `V1.1`、`BPX V1.1`、`FW VER V1.1`，檢查通過。
- [驗證] 重新執行 `python3 tests/run_host_tests.py`，五個主機測試執行檔及 BBP、I/O、主程式語法檢查全部通過。
- [修改] 使用實際 OLED framebuffer 重新產生 `docs/OLED_boot_preview.png`，顯示更新後的版本。
- [保留] 下方 2026-10-05 的驗證紀錄保留當時的 V1.0 字串；本次未執行 ESP32 target 編譯或實機燒錄。

## 功能驗證 — 2026-10-05

在專案目錄執行 `python3 tests/run_host_tests.py`。g++ C++17 使用 `-Wall -Wextra -Werror`、AddressSanitizer / UndefinedBehaviorSanitizer；五個測試執行檔及 BBP、I/O、主程式語法檢查全部通過。

| 測試 | 真正參與的程式／驗證結果 |
|---|---|
| `test_model.cpp` | 參數範圍、草稿與執行值分離、SAVE 凍結、失敗不套用、重試、相同值不重寫、CRC 損壞、小端序及 BUSY / GET |
| `test_runtime.cpp` | 實際量測、設定 NVS adapter、Session、封包模組；AUTO / LOAD 共用嚴格百分比、相等不結束、AUTO 資格與零脈衝備援 |
| 同上 | 閒置接受 SAVE 後旋轉必須延後寫入，當次仍用舊設定；一秒間隔；模式切換更新 LOAD IRQ，保留 MAX / HOLD 及待發布曲線 |
| 同上 | Flash 失敗維持執行值、重啟載入保存值、失敗後斷線草稿隔離、充電 BLE 開關 |
| 同上 | 30000 RPM、負緣第一參考、非參考邊沿忽略、micros 回繞、最早 32 點保留、32 點後 MAX 繼續更新、ISR queue overflow 重新同步 |
| `test_ble.cpp` | 實際設定 BLE 排程及設定模型；量測中 BUSY / GET、callback 到 main 間再檢查、Notify 失敗後 Read、持續訂閱失敗超時後仍可處理 GET |
| 同上 | 舊連線命令隔離、未 SAVE 草稿丟棄、dequeue 邊界重連不接受舊 SAVE、新連線立即 SAVE 不繼承舊草稿 |
| `test_auto_baseline.cpp` | 以實際量測模組驗證 AUTO 12000 → 3000 RPM 立即結束，最終 MAX=12000 |
| `render_boot.cpp` | 實際 OLED 程式產生 128×32 framebuffer，確認 BRD BPX 大字及 FW VER V1.0 小字 |
| 語法檢查 | `brd_bbp.cpp`、`brd_io.cpp` 及 `BRD_BPX.ino`；使用主機硬體替身，並非 ESP32 target 編譯 |

開機預覽 `docs/OLED_boot_preview.png` 由實際 framebuffer 最近鄰放大至 1024×256，已檢查文字不截斷、上下行不重疊。這是程式渲染結果，非實板照片。

審查抓到的通知卡住、舊世代 SAVE 與斷線後失敗草稿問題，均先以回歸測試重現失敗，再修改至通過。設定接受與連線世代驗證置於短 RAM critical section；Flash / NimBLE 操作不在該區執行。

## 驗證界線

GPIO、中斷、時間、NVS 裝置及 NimBLE stack 使用主機替身；測試並未量測 ESP32 上的 ISR 延遲、實際 FreeRTOS 排程、Flash 停頓、射頻或 App 相容性。30000 RPM 是合成脈衝輸入，不能宣稱實機在此轉速完全無漏脈衝。

執行環境未安裝 Arduino-ESP32 交叉工具鏈，未產生本次 ESP32 BIN，也未完成燒錄、實機 OLED / IR、充電切換、斷電保存或手機無線驗證。ZIP 排除原先的 build / BIN，避免把舊韌體當成本次修改。

測試執行器因部分執行追蹤環境限制，設定 `ASAN_OPTIONS=detect_leaks=0`。AddressSanitizer 與 UBSan 仍啟用；本次未聲稱 LeakSanitizer 檢查通過。
