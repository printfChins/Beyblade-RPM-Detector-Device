> 歷史紀錄：目前版本為 R3，請以 README.md 與 docs/SINGLE_REV_REFERENCE.md 為準。

> 歷史紀錄：以下為前次主動通知修正；目前 R2 雙邊沿規則請參考 README.md、CHANGELOG.md 與 VALIDATION.md。

<!--
[BRD_BBP 修正 新增]
依使用者提供的可用版本，追查已連線但沒有資料。
本文件記錄來源差異、實際修正及驗證界線。
-->
# 通知流程修正

使用者確認症狀為「已連線但無資料」。兩份程式使用相同名稱、Service UUID 與 Characteristic UUID，差異出現在連線及訂閱之後的通知觸發條件。

| 情境 | 使用者確認可用的 V0.1 | 前次交付 | 本次預設模式 1 |
|---|---|---|---|
| 訂閱後 | 自動 A0 | 不通知 | 等 100 ms 後自動 A0 |
| 裝載／卸載 | 自動 A0 | 等 0x51；bit2 也不代表靜態 LOAD | 自動 A0，bit2 對應 LOAD |
| 發射結果完成 | 自動 B0~B7、70~73 | 等 0x74 | 自動 B0~B7、70~73 |
| 主機不送命令 | 仍有事件資料 | 沒有任何通知 | 仍有事件資料 |
| 查詢 0x51／0x74 | 只有 0x51 | 兩者支援 | 兩者保留 |

[原因]
前次過度依賴 battlepass-emulation.md 所描述的主機輪詢行為，沒有涵蓋只訂閱的上位機。僅有封包編碼測試和目標編譯，無法證明主機能在正確時機取得封包。

[修正]
在同一個 BLE 排程中新增三種主動事件。命令回覆與事件通知仍使用一致快照、固定 17 bytes、逐頁間隔及通知失敗重試。12 頁傳送期間的 LOAD 通知會延後，避免頁序被插斷。設定模式 0 可恢復文件的純輪詢及旗標語意。

[來源定位]
下表行數為解壓縮來源的原始行數，不是修正後行數。

| 來源 | 專案檔案 | 行 | 判斷位置 |
|---|---|---:|---|
| 可用附件 | BRD_BBP/brd_ble.cpp | 578 | `if (link.connected && link.subscribed)` |
| 可用附件 | BRD_BBP/brd_ble.cpp | 599 | `if (!g_bbp_loaded_valid` |
| 可用附件 | BRD_BBP/brd_ble.cpp | 611 | `if (record.ready` |
| 前次交付 | BRD_BBP/brd_bbp.cpp | 312 | `if (xQueueReceive` |
| 前次交付 | BRD_BBP/brd_bbp_session.cpp | 71 | `case 0x74:` |

[其他已確認差異]
可用附件以 BRD_BLE_OLED V1.16 為基底，使用 CHANGE 捕捉雙邊沿、35% 結束門檻及直接 RPM 陣列；前次交付則以最初提供的 BRD_OLED V0.12 為基底，使用 FALLING、20% 結束門檻及 raw period 曲線。本次只修正已定位的通知流程，沒有將兩套量測機制混合。能收到資料與 RPM／SP、曲線正確性是不同驗證項目。

[測試證據]
新增 tests/test_subscriber.cpp，模擬手機只訂閱，執行真正的 brd_bbp.cpp、Session 與封包核心。修正前初始 A0、LOAD 及結果通知斷言失敗；修正後通過。原有純輪詢測試以模式 0 繼續通過。詳 VALIDATION.md 與 docs/host_tests.log。

[限制]
沒有使用者實際上位機的封包擷取紀錄，也沒有 ESP32-C3 實板可重現無線傳輸；上述為已證實能造成該症狀的流程差異及修正，不把宿主測試宣稱為實機互通驗證。0x61 仍未知、不回覆。
