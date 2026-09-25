# BRD_BBP 與 BX-09 API 比對 — R2 雙邊沿版

日期：2026-09-20。修訂識別：`API_V1.1_R2`。原 API 原文保存在 `docs/BX09_API_V1.1.md`；本次優先依使用者最新確認的記錄規則實作。

**以原報告相同的 30 項條件判定：27 項符合、2 項部分符合、1 項不符合。3 項差異是本次明確指定的起錄時點與每圈兩筆政策，不沿用 R1 的 30/30 宣稱。**

註解：使用者最新要求為「裝載後轉速不為 0 就記錄；上升沿→上升沿、下降沿→下降沿各以整圈時間計算，每圈兩次數值」。本次程式與測試均已依此改為 R2。原規格第 26、27 項屬建議流程，與新要求衝突的部分由新要求取代。

## 1. 版本與判定範圍

| 版本 | 符合 | 部分符合 | 不符合 | 合計 | 說明 |
|---|---:|---:|---:|---:|---|
| R2，本次預設模式 1 | 27 | 2 | 1 | 30 | 依最新需求，裝載後起錄，每圈兩筆整圈週期。 |
| R1，前次報告原判定 | 30 | 0 | 0 | 30 | 發射確認後起錄，單下降沿，每圈一筆；歷史報告保留供追溯。 |

本次核對預設 `BBP_COMPAT_AUTONOTIFY=1`，不把模式 0 的舊輪詢行為列為全符合。79 節含重複內容、上位機責任及未知項目，不直接拿章節數當功能數。

## 2. 完整專案與修改位置

- 壓縮包：`/workspace/scratch/858468988b82/deliverables/BRD_BBP.zip`。
- 完整原始碼：`/workspace/scratch/858468988b82/dual_edge/BRD_BBP/`。
- 開啟 `BRD_BBP/BRD_BBP.ino`；根目錄全部 `.cpp/.h` 放同一層。
- 6 個正式修改檔案、位置與標記見 `CHANGELOG.md`；主程式沒有新增片段。
- 相對 R1 程式與測試差異：`docs/BRD_BBP_DUAL_EDGE_R2.patch`。
- R1 基準 ZIP SHA-256：`65904f9c1fc4e5c841f83565ce00372711c918b5489e5a51ca04d4615d1959aa`。

## 3. 30 項逐項比對

| ID | 檢查項目 | 規格節次 | 本次 | 程式證據與註解 |
|---|---|---|---|---|
| 01 | 名稱 BEYBLADE_TOOL01 | 3、75 | 符合 | `brd_config.h` 第 29 行；名稱放於 Scan Response。 |
| 02 | Service UUID 為 55C40000-F8EB-11EC-B939-0242AC120002 | 3、75 | 符合 | `brd_config.h` 第 30 行；建立並廣播指定 Service。 |
| 03 | F002 為 Notify 通道，使用 Notification | 4、65 | 符合 | `brd_bbp.cpp` 第 229 行；F002 具 Notify 屬性，發送使用 notify()。 |
| 04 | 同時支援 WRITE 與 WRITE WITHOUT RESPONSE（建議） | 5 | 符合 | `brd_bbp.cpp` 第 230 行；同時設定 WRITE 與 WRITE_NR。 |
| 05 | 命令為 1 byte；嚴格按此長度處理 | 5 | 符合 | `brd_bbp.cpp` 第 112 行；非 1 byte 命令拒絕處理。 |
| 06 | 通知固定 17 bytes；多 byte 數值為 Little Endian | 6、22、58 | 符合 | `brd_bbp_protocol.h` 第 12 行；17 bytes；put16() 輸出 Little Endian。 |
| 07 | A0 header、0x3A、MAX、總次數與 6-byte UID 的位置 | 9 | 符合 | `brd_bbp_protocol.cpp` 第 101 行；A0／0x3A、MAX、Total、UID 位移保持正確。 |
| 08 | 相容解讀：Loaded=0x04，Unloaded=0x00 | 10 | 符合 | `brd_bbp.cpp` 第 295 行；模式 1 依去抖後 LOAD 送 0x04／0x00。 |
| 09 | A0[4] 提供隨電池狀態變化的 Battery Raw | 9 | 符合 | `brd_bbp.cpp` 第 294 行；Battery Raw 隨實際 SOC 映射到 0~250；原廠標定另列 U2。 |
| 10 | 每個正常 0x51 請求可取得一包 A0 | 8、70 | 符合 | `brd_bbp_session.cpp` 第 81 行；命令佇列逐筆派送，每個正常請求可取得 A0。 |
| 11 | 0x61 安全忽略，不自行清除或重設 | 64、70 | 符合 | `brd_bbp_session.cpp` 第 92 行；忽略未知功能，不修改資料。 |
| 12 | 0x74 回傳完整 B0 至 B7、70 至 73，並可再次重送最新 Shot | 13、61、62、68、70、77 | 符合 | `brd_bbp_session.cpp` 第 84 行；建立最新 State 的 12 頁，支援整組重讀。 |
| 13 | 0x75 真正清除 Shot History | 63、70、77 | 符合 | `brd_bbp_session.cpp` 第 87 行；清除 State、作廢 Capture、標記保存；Flash 受安全時段與寫入成功條件限制。 |
| 14 | 其他未知命令安全忽略 | 70 | 符合 | `brd_bbp.cpp` 第 121 行；未知命令增加診斷後忽略。 |
| 15 | 只訂閱 Notify，無須先寫命令即可收到初始通知 | 2、12、73 | 符合 | `brd_bbp.cpp` 第 329 行；訂閱後經 100 ms 穩定期即可收到 A0，無須先寫命令。 |
| 16 | 已識別的 LOAD 改變主動送 A0 | 71 | 符合 | `brd_bbp.cpp` 第 334 行；已識別的 LOAD 改變排入 A0；沿用既有去抖與 HOLD 限制。 |
| 17 | Shot 完成、更新資料後主動通知 History/Profile | 71、72 | 符合 | `brd_bbp.cpp` 第 337 行；Shot 發布後排入完整 12 頁 History/Profile。 |
| 18 | 約 500 ms 是主機輪詢策略，不當作裝置固定 Notify 週期 | 11 | 符合 | `brd_config.h` 第 34 行；10 ms 為待送頁面排程間隔；無固定 500 ms 自動 A0。 |
| 19 | History 1 至 48 於 B0 至 B5，49／50 於 B6，offset 正確 | 14、15 | 符合 | `brd_bbp_protocol.cpp` 第 123 行；50 筆依正確頁碼及 LE16 位移送出，49／50 筆落於 B6。 |
| 20 | B6[7..8] MAX、[9..10] Total、[11] Count/Index | 17 | 符合 | `brd_bbp_protocol.cpp` 第 127 行；B6 offsets 7／9／11 對應 MAX／Total／Count。 |
| 21 | 保存最近 50 筆，History 長度與累計次數分開 | 16 | 符合 | `brd_bbp_protocol.cpp` 第 84 行；50 筆 ring 與 uint32 累計分開；update 在連線判斷前完成本地紀錄。 |
| 22 | B7[16] 為 B0 至 B6 payload 總和 mod 256，不含 header | 18、42 | 符合 | `brd_bbp_protocol.cpp` 第 130 行；只累加 B0~B6 payload，不含 header，結果放 B7[16]。 |
| 23 | Profile 是 70、71、72、73 四頁，每頁 8 個 uint16，共 32 槽 | 19 至 22、58、59 | 符合 | `brd_bbp_protocol.cpp` 第 118 行；70~73 四頁，每頁 8 個 LE16，共 32 槽。 |
| 24 | Profile 使用連續每圈週期的 nRefs，按 period_us/8 編碼 | 19、23 至 26、43 至 50、52、55、74、79 | 部分符合 | `brd_measurement.cpp` 第 193 行、`brd_bbp_protocol.cpp` 第 57 行；每筆仍為整圈除 8，但兩類邊沿交錯形成每圈兩筆，週期區間重疊；舊時間累加語意不成立。 |
| 25 | 有效點少於 32 時，其餘槽位補 0 | 27、28、50、53 | 符合 | `brd_bbp_protocol.cpp` 第 96 行；先清零，再只複製有效點；包括全零 Profile。 |
| 26 | 超過 32 圈保留初期前 32 個有效週期（建議） | 54 | 部分符合 | `brd_bbp_protocol.cpp` 第 54 行；仍固定前 32 筆、不重建視窗，但每圈兩筆，約涵蓋 16 圈，並非 32 個不同的連續圈。 |
| 27 | Launch 偵測後開始 Profile Capture（建議流程） | 57、72 | 不符合 | `brd_bbp_session.cpp` 第 22、40 行；依最新要求，裝載後首筆有效 RPM 就起錄，發射時不清空，保留跨發射的整圈。 |
| 28 | Shot End 凍結資料；Profile 容量不直接決定量測結束 | 56、57 | 符合 | `brd_bbp_session.cpp` 第 48 行；Shot End 凍結，滿 32 筆仍繼續更新代表值。 |
| 29 | BX-09 封包不自行加入 ACK、NACK、SEQ 或 Session ID | 65、66、77 | 符合 | `brd_bbp_protocol.cpp` 第 113 行；維持原 17-byte 欄位；未加入 ACK、NACK、SEQ 或 Session ID。 |
| 30 | Connect／Init／Reconnect 不自動執行 0x75 清除 | 63 | 符合 | `brd_bbp.cpp` 第 71 行；連線 callback 只處理連線狀態，不執行 0x75。 |

## 4. 三項差異與本次需求驗證

| 最新需求 | 實作 | 驗證結果 |
|---|---|---|
| 裝載後非零轉速開始記錄 | 量測層完成同類整圈且通過有效值檢查，立即呼叫 Session 記錄。 | PASS，尚未發射的 4 筆全部保留。 |
| 上升沿→上升沿 | 獨立 HIGH 時間基準。 | PASS，6000 us 得 10000 RPM、raw 750。 |
| 下降沿→下降沿 | 獨立 LOW 時間基準。 | PASS，8000 us 得 7500 RPM、raw 1000。 |
| 每圈兩次，不以半圈脈寬算 RPM | CHANGE ISR 同時保存兩類電位。 | PASS，50 us 窄脈波、4000 us 同類整圈仍得兩筆 15000 RPM。 |
| 發射不丟失拉轉資料 | launch 僅標記，不清 Profile 或排除跨界週期。 | PASS，前段資料保留；原先其他 Session／傳送回歸已更新。 |

第 24 項的 raw 單位與每筆 RPM 正確，但兩筆所涵蓋的整圈時間重疊。第 26 項仍保留前 32 **筆**，目前約涵蓋 16 圈；第 27 項按最新要求提早起錄。這三項不能用原文全符合掩蓋取樣政策差異。

註解：每個邊沿事件更新量測值，OLED 沿用 100 ms 刷新；曲線沿用 32 格，不新增 BLE 即時逐點串流。半圈更新不假設感測器剛好 50% 佔空比。

註解：原上位機逐點累加 `raw / 125.0` 時，R2 穩態時間軸約放大兩倍。單純除二不能精確還原不對稱脈寬的邊沿時間。本次沒有在原 17 bytes 內自創時間戳欄位，也沒有自行降採樣；計時例子見 `docs/DUAL_EDGE_RECORDING.md`。

公開 [atlas_bey Profile 解析程式](https://github.com/shark-minister/atlas_bey/blob/main/core/src/result.cc) 以每圈一點解讀輸出，不能用來證明原廠內部是否使用雙邊沿。R2 是使用者指定的記錄政策，並非原廠內部實作的複製認證。

## 5. 原廠仍未確認的 4 類事項

| 代號 | 事項 | 本包可確認範圍 |
|---|---|---|
| U1 | 原廠 Write Characteristic UUID | 本包 F002 同時支援 Notify／Write／Write NR；不自行推定另一個原廠 UUID。 |
| U2 | A0 未知欄位、完整 flags、電池標定 | 核對 Loaded 相容語意與 SOC 映射，不宣稱完整原廠電池／位元定義。 |
| U3 | 0x61 用途、0x75 原廠回覆細節 | 沿用安全忽略 0x61、真正清除 0x75，沒有自創 ACK。 |
| U4 | 原廠 Stored SP、精確選點及內部演算法 | 本包 MAX RPM 作代表 SP；R2 每圈兩筆為使用者需求，未證明等同原廠評分。 |

註解：4 類未知不納入 30 項分母，也不以猜測判為通過。上位機 Header 組包、缺頁偵測、checksum 後重送與官方 SP 估算，仍屬上位機責任。

## 6. 實際驗證

- 7 組宿主測試全部通過，包含新增 10 個雙邊沿案例；使用 g++ C++17、ASan／UBSan、Wall／Wextra／Werror。
- 獨立唯讀審查未發現 Critical 或 Important；審查指出的雙邊沿溢位復原測試缺口已納入永久回歸，最終 7 組重跑通過。
- ESP32-C3：Arduino-ESP32 3.3.11、NimBLE-Arduino 2.5.1，80 MHz／4 MB，乾淨編譯成功、無警告。
- Flash 601851／1310720 bytes；靜態 RAM 29620／327680 bytes；app 602000 bytes。
- 本次重新編譯 4194304-byte 完整映像；bootloader、partition、app 已逐段核對，app 位於 0x010000。

詳 `VALIDATION.md`、`docs/host_tests.log`、`docs/compile.log` 與 `docs/SHA256SUMS.txt`。

註解：未執行實板燒錄、實際 IR 波形／中斷延遲、真實 BLE 擷取、NVS 斷電測試及官方 App 互通。舊 NVS 曲線不重算，格式沒有取樣模式標記；本次之後的新 Shot 才採雙邊沿。完整 4 MB 映像會覆寫 NVS，需要保留歷史時使用一般專案上傳且停用全 Flash 擦除。
