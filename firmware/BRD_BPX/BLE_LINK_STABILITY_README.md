# V1.3：藍牙傳送後斷線修正

本包延續 RPM 優先修正版。完整解壓縮，以同一個 `BRD_BPX` 資料夾的全部 `.ino/.cpp/.h` 重新編譯，入口為 `BRD_BPX.ino`。依使用者要求，版本由 V1.2 更新為 V1.3，程式修正識別仍為 `BLE_LINK_STABILITY_20261025`。不附新版 BIN，也沒有帶入舊版 build。

```c
/*
    [問題確認]
    原 brd_bbp.cpp 有兩條韌體主動斷線路徑：
    1. 32 筆 BBP 命令佇列滿時，清空佇列並 disconnect()。
    2. Notify 失敗且距離建立／上次送出該頁 >= 1500ms 時，disconnect()。
       這段時間包含主迴圈優先服務 RPM 而延後 BLE 的等待。

    [判斷界線]
    兩條路徑都已用實際 BBP 狀態機的主機測試重現。
    尚無測試者的 GAP reason、App、電源或重啟紀錄，
    不能斷言它們就是這次實機斷線的唯一原因。
*/
```

| 類別 | 檔案／位置 | 本次變更 |
|---|---|---|
| 修改 | `brd_bbp.cpp` → `BbpCharacteristicCallbacks::onWrite()` | 佇列滿只拒絕超額新命令並增加診斷，保留已接受的 32 筆及連線。BBP 不新增 ACK，超額查詢可由 App 稍後重送。 |
| 刪減 | `brd_bbp.cpp` → `brd_bbp_update()` | 移除佇列滿及 Notify 逾時的強制斷線。 |
| 修改 | `brd_bbp.cpp` → 分頁傳送狀態 | 失敗保留相同快照與頁碼；最早 100ms 後重試，成功才前進並恢復 10ms 正常節奏。真實斷線／取消訂閱仍丟棄舊傳輸。 |
| 新增 | `brd_bbp.h/.cpp` → `brd_bbp_get_diagnostics()` | `disconnect_events` 與 `last_disconnect_reason`，記錄已接受主連線的 GAP 原始原因值。callback 只記錄 RAM，不列印或寫 Flash。 |
| 新增 | `brd_config.h` | `BBP_NOTIFY_RETRY_INTERVAL_MS=100UL`。原 1500ms 常數仍供設定服務的通知等待上限使用。 |
| 新增／修改 | `tests/test_bbp_link.cpp`、`tests/test_priority_ble.cpp`、`tests/stubs/NimBLEDevice.h`、`tests/run_host_tests.py` | 六個連線案例、真實主迴圈的 RPM 與分頁快照整合驗證，並讓測試能觀察韌體發出的斷線要求。 |

```c
/*
    [功能優先順序保留]
    RPM 擷取／計算 > OLED > BLE／設定／歷史保存。
    BLE 沒有忙等、沒有提高優先級；每輪最多一次 Notify 共同額度保留。
    本次 BRD_BPX.ino 不需修改，既有 RPM 優先排程保留。
    RPM GPIO、門檻、LOAD 去抖、IR TEST、OLED、OTA、NVS 格式沿用。

    [保護機制保留]
    第二個連線仍會被拒絕；低電／ADC 故障仍可停止 BLE；
    已保存「充電停用 BLE」時，充電策略仍可關閉 BLE。
    真正的手機斷線、射頻失聯或裝置重啟，無法由應用層重試來阻止。
*/
```

主機驗證命令：`python3 tests/run_host_tests.py`，使用 C++17、`-Wall -Wextra -Werror`、AddressSanitizer、UndefinedBehaviorSanitizer。

- 12 個 host executables、既有 13 個 RPM 優先案例、六個 BLE 連線案例全部通過。
- 六個案例：一頁成功後連續失敗超過 1.5 秒、40 筆命令突發、100ms 重試節流、RPM 延後 BLE 兩秒、重連重用 handle、取消／重新訂閱。
- 真實主迴圈：Notify 失敗三秒期間，1500 個 2000us 週期均得到 30000 RPM；OLED 持續更新，沒有擷取停用／事件遺失，MAX、歷史正確，新結果不混入舊分頁快照。
- LOAD／AUTO 各兩小時虛擬時間、共 14400 發及 micros 回繞的既有測試仍通過。這是時間模擬，並非四小時實機測試。
- 尚未以 ESP32 目標工具鏈編譯、連結或燒錄，未驗證實機 RF、電源、App 接收及控制器時序。不得把 host 測試視為實機斷線已完全解決。

實機請用原本同一支 App 重複連線及完成多次 RPM 量測，確認首次 A0、完整 B0～B7／70～73、下一次量測均能保持連線，再交叉測試不同手機與至少 30～60 分鐘運作。若仍斷線，記錄：發生在第一包或量測結果後、間隔多久、OLED 有無開機／低電／ADC 畫面、RPM 是否持續、App 斷線原因與序列埠重啟紀錄。

`brd_bbp_get_diagnostics()` 是韌體 C++ 唯讀 API，沒有新增 BLE 特徵或自動輸出。除錯器可讀取 `notify_failures`、`command_queue_overflows`、`disconnect_events`、`last_disconnect_reason`。最後原因保留 NimBLE callback 的完整整數值，不能直接當作一個 byte 的 HCI 原因。記錄只在 RAM，重啟不保留；應用層停用 BLE 也可能先清掉 handle，不能只靠此欄位判斷停用或重啟。

其餘 RPM 修改、全部結構及編譯設定見 `RPM_PRIORITY_README.md`；本包實際清單見 `FILE_MANIFEST.txt`。
