## V1.2.3 — 2026-10-31：LOAD 五步驟、MAX 等號、HOLD 3 秒

- [修改] LOAD 的結束條件由 `<` 改為 `<=`：MAX=10000 / 30% 時，3000 即結束。
- [新增] 獨立 `BRD_STATE_HOLD`，由 `finish_measurement()` 先停止 ISR 擷取，再凍結 MAX 與 BBP 曲線。
- [修改] LOAD 模式 OLED MAX 首次完整顯示後鎖定 3000 ms（AUTO 保留原 2500 ms）；HOLD 期間不處理 LOAD / SP、不允許新一輪量測或修改設定。
- [修改] HOLD 到期先回 WAIT LOAD，保留 MAX；必須實體 LOW 去抖後新的 HIGH 去抖，才 LOADED 並清零。
- [修改] OLED LOAD 顯示使用狀態機，避免 HOLD 剛結束 GPIO 還 HIGH 造成假 LOADED。
- [保留] V1.2.1 BLE 斷線修正、放電曲線、充電補償、OTA、V1.2 的 RPM 取樣機制；不加 FULL。
- [測試] `tests/test_load_hold_v123.cpp` 與原有測試已包含於 `tests/run_host_tests.py`。

# V1.2.2 — HOLD／LOAD 重新武裝修正（2026-10-31）

- [修正] HOLD 2.5 秒後不得將 LOAD 穩定狀態強行設為 LOW，避免 GPIO1 持續 HIGH 被當成新裝載。
- [新增] HOLD 結束後真實 LOW 去抖 50ms、再 HIGH 去抖 50ms，才允許清零 MAX 與下一發。
- [保留] 2.5 秒 HOLD 期間任何 LOAD/RPM 彈跳不可清零 MAX；AUTO 模式原流程不變。
- [保留] V1.2 的 RPM ISR/公式、V1.2.1 的 BLE 斷線修正、電池曲線/充電補償；不加入 FULL。
- [測試] 真實量測與 BBP Session 專項、完整主機端回歸測試通過；未執行 ESP32-C3 實機驗證。

# V1.2.2 — HOLD／LOAD 重新武裝修正（2026-10-31）

- [修正] HOLD 2.5 秒後不得將 LOAD 穩定狀態強行設為 LOW，避免 GPIO1 持續 HIGH 被當成新裝載。
- [新增] HOLD 結束後真實 LOW 去抖 50ms、再 HIGH 去抖 50ms，才允許清零 MAX 與下一發。
- [保留] 2.5 秒 HOLD 期間任何 LOAD/RPM 彈跳不可清零 MAX；AUTO 模式原流程不變。
- [保留] V1.2 的 RPM ISR/公式、V1.2.1 的 BLE 斷線修正、電池曲線/充電補償；不加入 FULL。
- [測試] 真實量測與 BBP Session 專項、完整主機端回歸測試通過；未執行 ESP32-C3 實機驗證。

<!-- [V1.2.1 新增] V1.2 BLE 主動斷線問題修正；只移植 21 點放電曲線 / 72s 充電百分比補償，不移植 FULL。 -->
## V1.2.1 — V1.2 基礎的 BLE 斷線與電池 SOC 修改（2026-10-31）

- [保留] V1.2 RPM 擷取、發射完成暫停、MAX 自鎖、LOAD/ AUTO、OLED 與 OTA 原始碼不變。
- [修正] 命令 Queue 滿、Notify 失敗 1.5s 不再主動 `disconnect`，100ms retry 保留同頁。
- [新增] 21 點放電曲線（0%=3460mV）、200mAh/100mA 充電 SOC 每 72 秒最多 +1%。
- [不移植] FULL 偵測、FULL 畫面及 V1.4 電壓式低電鎖定；維持 V1.2 百分比鎖定規則。
- [測試] 既有 V1.2 與新增 BLE／電池回歸測試；尚待 ESP32-C3 實機驗證。
- [文件] `V1.2.1_MIGRATION_README.md`。

## 2026-10-15 — OTA % 空心圓圈字型修正（維持 V1.2）

- [修正] OTA `%` 不再以放大的 5x7 實心點呈現，改用 10x14 專用點陣畫出兩個空心圓圈與斜線。
- [新增] OTA 百分比 framebuffer 點陣檢查，覆蓋 0%、25%、50%、75%、100%。
- [保留] OTA 左側扇形圓盤、IR TEST、BLE API 與其他韌體功能。

## V1.2 - IR TEST DXF 90-degree correction

- [修改] IR1 / IR2 圖示維持向左旋轉 90 度，重新依原始 DXF 六頂點等比例繪製。
- [修正] 右側 IR1 旋轉後只具有上下對稱性，不再強制左右鏡射，修正梯形變形。
- [修改] 圖示左右分區重新定位，避開頂部 BAT 與右下 BLE 顯示。
- [新增] 對照原 DXF 的座標單元測試及四種 OLED framebuffer 畫面驗證。
- [保留] BLE API、OTA、量測、版本字串 V1.2 不變。

## 2026-10-15 — IR TEST DXF 輪廓修正（維持 V1.2）

- [修正] 左側 IR2 使用 DXF 縱向 6 頂點六邊形；右側 IR1 使用 DXF 6 頂點梯形，不再誤畫五邊形。
- [修正] OLED 128x32 兩個輪廓各自以相同的 4 px / DXF 單位轉換，並修正鏡射對稱性與垂直置中。
- [修正] 實心填色改為依包圍框整數叉積判斷，避免只填到 y=12 的顯示缺陷。
- [修改] 移除 IR1、IR2 文字；右上 BAT 電壓取代電池圖示。
- [新增] `docs/IR_shape_reference.dxf`、`docs/IR_TEST_DXF_CORRECTION.md` 與 4 種狀態的真實 framebuffer 預覽。
- [保留] 版本 V1.2、量測、BLE API、OTA 及 Flash 分割表不變。

## 2026-10-15 — V1.2 OTA OLED 扇形進度 UI（版本不變）

- [V1.2 修改] `brd_oled.cpp`：將下方 `0~100 PCT` 改為左側填充式扇形進度圓盤，及中央 `0~100%` 大字。
- [V1.2 新增] `%` 的 5×7 字型。圓盤從 12 點方向順時針填滿，0% 空心、100% 實心。
- [保留] 上方 OTA UPDATE/VERIFY/OTA DONE、電量與 BLE 狀態圖示；原本量測 RPM/MAX 畫面不變。
- [V1.2 新增] `tests/render_boot.cpp` 新增 0/25/50/75/100% 進度畫面與圓盤填充測試；同步更新 OLED 模擬預覽。
- [注意] 本交付為完整專案原始碼。輸入 ZIP 原有 `.bin`、`build/` 屬修改前舊編譯結果，為避免誤燒已從交付 ZIP 移除。需重新以 Arduino IDE 編譯並匯出 app-only `.bin` 才能藍牙 OTA。

# V1.2

## 2026-10-14 — App 容量擴充交付確認

- [修改] 維持 4 MB Flash、雙 OTA App 各 1984 KiB，保留原 NVS 與 Core Dump 位址。
- [新增] `FLASH_APP_1984_README.md`：首次 USB 更新分割表、保留 NVS、建置驗證與回復說明。
- [修改] `README.md`、`OTA_V1.2_README.md` 修正舊 1280 KiB 分割表敘述。
- [保留] V1.2 功能與版本字串；本次沒有修改 RPM、BLE 命令或 NVS 格式。


## 2026-10-14 — 同版本 BLE OTA 批次加速與 App 擴容

- [修改] 本機偏好 MTU 改 247；網頁只採用實際 GATT 交換結果，不強制主機 MTU。
- [新增] 可選 SYNC (0x06) 開啟累計 ACK；最多連續 8 包資料，再回報可重傳位移。
- [新增] 4 KiB RAM 緩衝寫入 Flash，END 必須寫入尾塊後再進行 MD5 / Flash 讀回校驗。
- [修改] OTA 接收佇列 8 -> 32；支援較高吞吐與不覆寫式滿列丟棄重傳。
- [保留] 原本逐包 ACK / 協議版本 1，用於舊網頁或舊韌體相容。
- [修改] partitions.csv 改為 App 0/1 各 1984 KiB；[刪減] 未使用 SPIFFS。
- [注意] 分割表更新需 USB 完整更新；BLE OTA 不會改變現有設備分割表。
- [注意] 附件原本的編譯 BIN 為舊程式，交付 ZIP 不包含舊 BIN，需重新編譯。

- [新增] 自有 BLE OTA 服務、逐包確認、重傳、逾時與取消。
- [新增] 備用 app 分區寫入、串流及 Flash 讀回 MD5、ESP-IDF 映像校驗。
- [新增] OTA 量測 pause、NVS 隔離、OLED 進度與完成重啟。
- [修正] 最後一筆歷史未發布/保存時禁止開始 OTA；驗證期斷線不能提交。
- [修改] 版本字串 V1.2；[刪減] 交付中的舊 V1.1 建置映像。

# 變更紀錄

## 2026-10-07 — V1.1 充電關閉 BLE 預設停用

- [修改] `brd_config.h` 的 `BLE_OFF_WHILE_CHARGING` 改為 `0U`，預設充電時保留 BLE。
- [刪減] 原預設啟用值 `1U`。
- [修改] README、設定 API 與修改說明同步更新；原已保存的 Flash 設定仍優先。
- [修改] 既有主機測試確認首次預設保留 BLE，並明確啟用後驗證充電停用，再停用驗證恢復。

## 2026-10-07 — V1.1 充電停用提示改為 BLE / OFF 兩行小字

- [修改] OLED 右下角改用既有 5×7 字型、1 倍大小，上行 `BLE`、下行 `OFF`。
- [刪減] 原斜線藍牙停用圖示；一般連線圖示保留。
- [修改] 完整修改位置說明、README 與實際 framebuffer 預覽同步更新。

## 2026-10-07 — V1.1 充電關閉 BLE 圖示

- [新增] 充電導致 BLE 停用時，OLED 右下角顯示帶斜線的藍牙圖示。
- [新增] 唯讀 BLE 啟動狀態介面，確認已停止後才提示，保留設定保存的 200 ms 回覆期間。
- [修改] 充電停用狀態變化觸發下一幅畫面，即使充電與連線狀態未變也能更新。
- [刪減] 原本只在已連線時繪圖、充電關閉後完全沒有提示的單一判斷。
- [新增] 完整修改位置說明與實際 OLED framebuffer 預覽。

## 2026-10-07 — V1.1 版本更新

- [修改] `brd_config.h` 的 `PROJECT_FIRMWARE_VERSION` 由 `V1.0` 更新為 `V1.1`。
- [修改] 共用版本設定產生 `BPX V1.1` 與 OLED 開機字串 `FW VER V1.1`。
- [修改] README 與由實際 OLED framebuffer 產生的開機預覽同步更新。
- [刪減] 目前版本設定及 README 開機說明中的 `V1.0`；下方歷史紀錄保留原版本。

## 2026-10-05 — BLE 設定、Flash 保存、共同百分比與開機畫面

- [新增] 獨立 BRD BLE 設定服務，提供四項參數的 GET / SET / SAVE / DEFAULTS / DISCARD。
- [新增] RAM 草稿、凍結保存交易、參數驗證、NVS version / CRC32、讀回驗證、一秒寫入間隔及狀態回報。
- [修改] 模式、AUTO 最低 RPM、共同結束百分比、充電 BLE 開關改由執行時設定供應，config.h 保留預設值。
- [修改] AUTO 保留達最低 RPM 的有效結果資格，完成條件改用與 LOAD 相同的嚴格 MAX 剩餘百分比。
- [刪減] AUTO 等待 0 RPM 滿 1000 ms 的獨立發射條件。
- [修改] 量測中保留查詢與通知；設定修改及 BBP 清除操作拒絕，callback / main loop 雙重檢查。
- [新增] Flash 保存前確認非量測、空 GPIO 佇列並暫停捕捉；成功後套用且保留已完成 MAX / HOLD / 曲線。
- [新增] 已接受 SAVE 可跨斷線完成；未 SAVE 草稿在斷線時丟棄；舊連線請求不得在新連線執行。
- [新增] 可選充電時停用 BLE，本機歷史及量測持續運作。
- [修改] 開機上方大字 BRD BPX，下方小字 FW VER V1.0；正常 RPM/MAX 畫面保留。
- [新增] API 文件、主機測試及由實際 OLED framebuffer 產生的預覽。
- [交付] 完整原始碼；排除舊 build / BIN 及主機測試執行檔。

本次以實際 brd_config.h 為準：LOAD 去抖 50 ms、預設結束百分比 30%、MAX HOLD 2500 ms。原 ZIP 的舊說明曾記載 100 ms / 20%，本次文件已校正。

### IR TEST 圖示間距微調（V1.2，不升版）

- [修改] 兩個圖形向中間各移動 4 px；水平外框空白間距由 18 px 改為 10 px。
- [保留] 原 DXF 輪廓與 BLE/OTA/電池/量測功能。
