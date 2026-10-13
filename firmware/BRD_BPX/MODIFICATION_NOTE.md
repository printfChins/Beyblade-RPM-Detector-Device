# 修改檔案與貼放位置

[修改] 2026-10-07：`BRD_BPX/brd_config.h` 第 74 行 `BLE_OFF_WHILE_CHARGING` 由 `1U` 改為 `0U`，預設停用充電關閉 BLE 功能。完整同名檔案已整合，不需另貼程式；已有 Flash 設定仍優先，API 參數 `04=0` 後 SAVE 可更新已有裝置。

[新增] 2026-10-07 充電關閉藍牙圖示的完整檔案位置與貼放說明，請見 [CHARGING_BLE_ICON_NOTE.md](CHARGING_BLE_ICON_NOTE.md)。本次版本為 V1.1；以下保留 2026-10-05 設定功能的歷史修改紀錄。

以最新提供的 `BRD_BPX(1).zip` 為基礎；兩次上傳的內容相同。交付 ZIP 的根目錄為 `BRD_BPX/`，主程式為 `BRD_BPX/BRD_BPX.ino`。完整解壓縮即可開啟，保留所有正式 `.cpp/.h` 與 `.ino` 在同一資料夾。

## 新增檔案（完整檔案已附）

- `BRD_BPX/brd_settings_model.h`、`brd_settings_model.cpp`：四參數／草稿／保存交易／編碼。
- `BRD_BPX/brd_settings.h`、`brd_settings.cpp`：NVS 載入、驗證、延後保存與充電策略。
- `BRD_BPX/brd_settings_ble.h`、`brd_settings_ble.cpp`：獨立 BLE 設定服務與排程。
- `BRD_BPX/SETTINGS_API.md`：App 所需完整 API。
- `BRD_BPX/tests/`：可重建主機回歸及硬體邊界替身。
- `BRD_BPX/docs/`：本次設計、計畫與實際開機 framebuffer 預覽。

## 修改位置

以下行號對應本次完整交付檔案；程式已含 `[新增]`、`[修改]`、`[刪減]` 註解。

| 完整相對路徑 | 起始行 | 說明 |
|---|---:|---|
| `BRD_BPX/brd_config.h` | 27 | [修改] 正確開機名稱 BRD BPX 與獨立小字版本 |
| `BRD_BPX/brd_config.h` | 64 | [新增] 設定 UUID、NVS、命令佇列與保存間隔 |
| `BRD_BPX/brd_settings_model.h` | 16 | [新增] 四項設定、命令、狀態及純 C++ 模型 |
| `BRD_BPX/brd_settings_model.cpp` | 155 | [新增] 驗證、RAM 草稿、固定封包／儲存格式 |
| `BRD_BPX/brd_settings.cpp` | 30 | [新增] 開機載入、安全窗口保存、讀回驗證、斷線草稿處置 |
| `BRD_BPX/brd_settings_ble.cpp` | 94 | [新增] 獨立設定 GATT、世代隔離、有界排程與通知超時 |
| `BRD_BPX/brd_measurement.cpp` | 229 | [修改] AUTO / LOAD 共用百分比與 AUTO 最低資格 |
| `BRD_BPX/brd_measurement.cpp` | 670 | [新增] 修改窗口、Flash 暫停／恢復並保留完成結果 |
| `BRD_BPX/brd_bbp.cpp` | 76 | [修改] 轉接連線事件；量測中拒絕 0x75 清除 |
| `BRD_BPX/brd_bbp.cpp` | 208 | [修改] 原歷史保存加入原子捕捉暫停保護 |
| `BRD_BPX/brd_bbp.cpp` | 232 | [修改] 充電關 BLE 時仍載入本機歷史；同時建立設定服務 |
| `BRD_BPX/brd_oled.cpp` | 551 | [修改] 上方 2 倍大字、下方 1 倍小字版本 |

`brd_measurement.h` 同時新增修改窗口與暫停／恢復介面；`brd_oled.cpp` 正常畫面模式改讀執行值。原 BBP 封包、GPIO、參考極性、最早 32 點曲線及 MAX 獨立更新均保留。

## 主程式新增部分的貼放位置

完整 `BRD_BPX.ino` 已整合，無需另貼。新增入口如下：

| 位置 | 行號 | 目的 |
|---|---:|---|
| `#include "brd_settings.h"` | 19 | [新增] 設定／BLE 模組 include |
| `brd_settings_begin();` | 23 | [新增] setup 最前面載入 NVS 設定 |
| `if (!brd_settings_bluetooth_allowed` | 58 | [新增] loop 中依充電設定停用／恢復 BLE |
| `brd_settings_ble_update();` | 65 | [新增] 處理查詢／命令回覆，量測修改回 BUSY |
| `brd_settings_update();` | 67 | [新增] 閒置或量測結束後保存並套用 |

量測更新維持優先順序；之後處理既有 BBP、設定 BLE、設定保存及 OLED。修改命令量測中拒絕，但查詢／通知保留。

## 刪減與保留

- [刪減] AUTO 原本 `AUTO_LAUNCH_ZERO_MS=1000ms` 及獨立零轉速發射計時，改成共用百分比；未達最低門檻的 250ms 重置保留。
- [刪減] 舊開機「小字 BRD + 大字名稱與版本混合」排版，改為大字 BRD BPX、小字 FW VER V1.0。
- [交付排除] 原 ZIP 的 build / `.bin/.elf/.map` 和主機測試二進位，並非可直接燒錄的韌體包。
- [保留] config.h 預設 LOAD、AUTO 門檻 2000、結束 30%、LOAD 去抖 50ms、MAX HOLD 2500ms；充電 BLE 關閉為新增開關，預設啟用。
- [保留] 版本號 V1.0、原名稱／BBP UUID／廣播與歷史 blob；設定另存 brd_cfg/settings_v1。

完整檔案清單見 FILE_MANIFEST.txt；測試與未完成的板子驗證見 VALIDATION.md。
