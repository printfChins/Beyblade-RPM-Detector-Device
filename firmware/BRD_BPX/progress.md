# 執行紀錄：docs/superpowers/plans/2026-10-25-rpm-priority.md

使用者已要求直接修正；採現有上傳專案的獨立目錄執行。

Task 1: complete — 原生 IRAM capture、HOLD 顯示分離、1024 FIFO、16筆快照、事件世代及 IR LOSS。
Task 2: complete — NVS 保留事件、設定延後原子套用、完成資料 FIFO、獨立 OTA 暫停。
Task 3: in progress — 真實主迴圈排序及三服務 Notify 額度測試通過，待長時間/審查/交付。
驗證：11 個 host executable、9 個優先級案例、既有 OTA/設定/BBP/OLED/DXF 測試全部通過，ASan+UBSan。
裁定：HOLD 不阻擋新量測；舊尾段以新 LOAD 或 300ms 安靜重新武裝；已知事件遺失不發布不完整 MAX。成本：比原本可更早啟動下一輪，事件遺失時該輪不計入歷史。
裁定：採原生 IDF GPIO service，若已被非本模組安裝則顯示 IR ERR，避免假設可在 Flash 期間執行。

Task 3: complete — 11個 executable、13個優先級案例、ASan/UBSan、全部既有回歸通過。
長時間：LOAD/AUTO各120分鐘、各7200發；共4小時虛擬時間、14400發，包含micros回繞；無事件遺失、無重複計次。
獨立審查：rpm_priority_review；兩項Important、無Critical。
Final: fixed 同模式保存重置LOAD/READY — case11 RED→GREEN，完整suite通過。
Final: fixed HOLD尾段溢位誤判安靜重複計次 — case12 RED→GREEN，完整suite通過。
Final: fixed 嚴格BBP模式待發布旗標提早消失 — case13 RED→GREEN，完整suite通過。
審查界線：未認證ESP32目標編譯、IRAM map、cache-off實際執行、GPIO延遲及真正ISR併發；這些需目標工具鏈與實機。
延期minor：無。

# 後續：docs/superpowers/plans/2026-10-25-ble-link-stability.md

基準：520d4b262670d3e75a29e6b0e573b35913539ffc；獨立分支 ble-link-stability。
使用者回報一次藍牙傳送後斷線，延續既有直接修正授權；可選現象問題未得到答案。
Pre-flight: 一項 BBP 工作，無跨 task 介面衝突。
Ruling: 保留既有工具鏈、C++ 非阻塞排程及 RPM > OLED > 背景功能；不提高 BLE 優先級 — 符合既有明確要求 — 成本為塞車時傳送完成時間較長。
Task 1: RED — cases1/2/4 觀察到韌體主動 disconnect；case3 觀察到失敗重試過密；cases5/6 原本已通過，為重連／訂閱保護覆蓋。
Task 1: GREEN — 移除 Notify 強制斷線後 cases1/3/4 轉綠，case2 仍紅；移除佇列滿強制斷線後六案全綠。
Task 1: suite — python3 tests/run_host_tests.py exit0；12 executables、13 RPM cases、6 BLE cases、真實主迴圈 Notify 失敗三秒時 30000 RPM／OLED／MAX／歷史／快照隔離通過；ASan/UBSan。
Task 1: review pending；實機原因尚未確認，無 ESP32 目標工具鏈／硬體。
Task 1: complete — 獨立 ble_link_review 審查無 Critical/Important/Minor；六案及真實主迴圈整合獨立複測 ASan/UBSan 全綠。見 docs/BLE_LINK_REVIEW.md。
Final: Ruling: 無目標編譯／實際 NimBLE、ISR/控制器/callback 併發、RF/電源/App/現場根因證據 — 交付原始碼及實機驗收項目，不宣稱現場已根治 — 成本為實機可能仍有其他斷線原因。
Final: Ruling: Notify 成功維持既有「交給 stack」語義，不新增 ACK — 保持 BBP 相容性 — 成本為 App 接收確認仍需上位機處理。
Final: minor (deferred): 無。

# 2026-10-25 使用者指定 V1.3

依「新版改 V1.3」更新單一版本來源 `PROJECT_FIRMWARE_VERSION`，派生為 `BPX V1.3`、`FW VER V1.3`。RPM/OLED/BLE 行為沿用已交付修正；歷史版本註解及協定格式版本不混同韌體版本。交付名稱改為 BRD_BPX_V1.3_RPM_BLE_Fix_20261025.zip。
驗證完成：原預覽輸出在 OTA 扇形檢查後被覆蓋，已修正 host 預覽工具以保存／還原開機畫面；目視確認新圖顯示 FW VER V1.3。完整 suite exit0，12 executables、13 RPM、六項 BLE、ASan/UBSan 與語法檢查全部通過，輸出見 docs/V1_3_TEST_RESULTS.txt。沒有目標工具鏈或實機驗證，來源 ZIP 不附 BIN。
