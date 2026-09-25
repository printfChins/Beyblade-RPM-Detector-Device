# BRD_BBP 與 BX-09 API 規格符合性比對 — 修正後

日期：2026-09-20。修訂識別：`API_V1.1_R1`。依據使用者提供的 Reverse Engineering API V1.1，完整原文隨附 `docs/BX09_API_V1.1.md`。

**預設主動通知模式：30 項裝置端檢查，30 項符合、0 項部分符合、0 項不符合。**

註解：此結論涵蓋前次報告相同的 30 項條件，其中第 26、27 項是文件的曲線建議流程。79 節包含重複內容、上位機責任及未知內容，不以章節數當功能數。以下結果來自原始碼審查、6 組宿主測試與 ESP32-C3 目標編譯，不表示已取得原廠或官方 App 相容性認證。

## 1. 修正前後統計

| 版本 | 符合 | 部分符合 | 不符合 | 合計 |
|---|---:|---:|---:|---:|
| R：本次 API_V1.1_R1，模式 1 | 30 | 0 | 0 | 30 |
| C：前次主動通知版 | 28 | 0 | 2 | 30 |
| O：先前純命令回覆版 | 24 | 0 | 6 | 30 |
| V：使用者可用 V0.1(2) | 19 | 4 | 7 | 30 |

C／O／V 統計保留自前次報告；本次修改與回歸目標是由 C 修成 R，未重新測試 O／V。舊報告保存在 `docs/BRD_BBP_API_Conformance_Before.md`。

註解：`BBP_COMPAT_AUTONOTIFY=1` 是全符合設定，同時支援 Push Notify 與 CMD。可選的模式 0 是保留的舊回歸模式，並不符合本表的全部事件通知及 Loaded 語意條件。

## 2. 完整專案位置

- 交付壓縮包：`/workspace/scratch/858468988b82/deliverables/BRD_BBP.zip`。
- 本次原始碼：`/workspace/scratch/858468988b82/api_conform/BRD_BBP/`。
- 開啟檔案：`BRD_BBP/BRD_BBP.ino`；全部根目錄 `.cpp/.h` 與主程式放同一資料夾。
- 修改位置：`BRD_BBP/CHANGELOG.md`，列出 7 個正式修改檔案與起始行。
- 完整差異：`BRD_BBP/docs/BRD_BBP_API_V1.1.patch`；提供完整檔案，無須逐段貼入。
- 前次 C 壓縮包 SHA-256：`fbd5e52da9f445c885b6721c04aac3668de583aa2625bdcae7e5bbd6804e057a`。

下表檔名均相對本次 `BRD_BBP/`，行號對應交付原始碼。

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
| 10 | 每個正常 0x51 請求可取得一包 A0 | 8、70 | 符合 | `brd_bbp_session.cpp` 第 85 行；命令佇列逐筆派送，每個正常請求可取得 A0。 |
| 11 | 0x61 安全忽略，不自行清除或重設 | 64、70 | 符合 | `brd_bbp_session.cpp` 第 96 行；安全忽略，不清除或重設資料。 |
| 12 | 0x74 回傳完整 B0 至 B7、70 至 73，並可再次要求重送最新 Shot | 13、61、62、68、70、77 | 符合 | `brd_bbp_session.cpp` 第 88 行；建立最新 State 的 12 頁，不因讀取清除，支援整組重讀。 |
| 13 | 0x75 真正清除 Shot History | 63、70、77 | 符合 | `brd_bbp_session.cpp` 第 91 行；清除 State、作廢本次 Capture、標記保存；Flash 受安全時段與寫入成功條件限制。 |
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
| 24 | Profile 使用連續每圈週期的 nRefs，按 period_us/8 編碼，非直接 RPM 或固定時間取樣 | 19、23 至 26、43 至 50、52、55、74、79 | 符合 | `brd_bbp_protocol.cpp` 第 57 行；FALLING 相鄰週期四捨五入除 8，飽和 uint16；不直接傳 RPM 或 50 ms 取樣。 |
| 25 | 有效點少於 32 時，其餘槽位補 0 | 27、28、50、53 | 符合 | `brd_bbp_protocol.cpp` 第 96 行；先清零，再只複製有效點；包括全零 Profile。 |
| 26 | 超過 32 圈保留初期前 32 個有效週期（建議） | 54 | 符合 | `brd_bbp_protocol.cpp` 第 54 行；[修改] 第 32 點後停止寫入；更高峰不能重建視窗。 |
| 27 | Launch 偵測後開始 Profile Capture（建議流程） | 57、72 | 符合 | `brd_bbp_session.cpp` 第 38 行；[修改] launched(confirmed_us) 後才收完整週期；跨越去抖確認點的週期不保存。 |
| 28 | Shot End 凍結資料；Profile 容量不直接決定量測結束 | 56、57 | 符合 | `brd_bbp_session.cpp` 第 23 行；Shot End 凍結；Profile 滿 32 點不決定量測結束，代表值仍可更新。 |
| 29 | BX-09 封包不自行加入 ACK、NACK、SEQ 或 Session ID | 65、66、77 | 符合 | `brd_bbp_protocol.cpp` 第 113 行；維持原 17-byte 欄位；未加入 ACK、NACK、SEQ 或 Session ID。 |
| 30 | Connect／Init／Reconnect 不自動執行 0x75 清除 | 63 | 符合 | `brd_bbp.cpp` 第 71 行；連線 callback 只處理連線狀態，不執行 0x75。 |

## 4. 已修正的兩項差異與配套

### 26：固定保留前 32 個有效週期

[刪減] 舊版遇到後段更高峰時重新建立峰值視窗的流程及 recent 緩衝。

[修改] 先輸入 raw 1000~1039，再輸入 raw 500，保存值仍是 1000~1031，共 32 點。後續更高峰只更新獨立的 History 代表值，不改寫曲線。沒有平均、插值或重採樣。

### 27：發射確認後才收 Profile

[修改] LOAD HIGH→LOW 經既有去抖確認後，量測層傳入去抖到期的事件時間。Session 清空 Profile、啟用 Capture。發射前的週期、起點在確認時間之前而終點在之後的跨界週期均排除。這個界線是「確認發射」時間，不是未去抖的原始 LOW 邊沿，也不是 loop 或 BLE 發送時間。

[新增] 邊界案例：發射前 period=4000 us；12000 us 確認發射；10000~18000 us 的跨界週期排除；後續 12000 us、16000 us 週期編碼為 raw 1500、2000，剩餘補零。History 仍保存整次 MAX 15000，而曲線最高解碼值是 5000。

[修改] 代表 SP 與 Profile 分開。沒有發射後週期的有效 Shot 仍寫 History、增加 Counter，Profile 全零；NVS 允許保存與重新載入。這避免改成 Launch 後收樣就遺失已完成的 Shot。

[刪減] NVS 不再要求最後一筆 History 等於曲線峰值；保留 CRC、長度／版本、範圍、ring 索引與曲線連續非零／零尾端檢查。對應規格第 31、33 節。

## 5. 未定案的 4 類事項

| 代號 | 事項 | 本包可確認範圍 |
|---|---|---|
| U1 | 原廠 Write Characteristic UUID、是否必須是另一特徵 | 本包 F002 同時支援 Notify／Write／Write NR；規格未完全確認原廠 Write UUID，不自行推定 F001。 |
| U2 | A0 未知欄位、完整 flags、Battery Raw 標定 | 表中只核對相容 Loaded 語意及電池相關變動值，不宣稱完整原廠 bit／電池標定一致。 |
| U3 | 0x61 真正用途及 0x75 原廠精確回覆序列 | 依規格建議安全忽略 0x61、真正清除 0x75，不增加自創 ACK。 |
| U4 | 原廠 Stored SP 評分、精確 Profile 選點與發射演算法 | 本包採整次 MAX RPM 作代表 SP，Profile 按文件建議保留發射確認後前 32 圈；尚未證明等同官方評分及內部量測。 |

註解：這 4 類不納入 30 項分母，不能以猜測補成「已驗證符合」。時間軸、Header 組包、缺頁偵測、checksum 驗證後重送 0x74、最少 7 點及前段峰值估算屬上位機責任，本次沒有改寫上位機。

## 6. 驗證結果與限制

| 驗證 | 結果 |
|---|---|
| conformance | PASS：前 32 點、發射邊界、空曲線、後峰代表值、獨立值儲存、micros 回繞。 |
| protocol | PASS：17 bytes、LE、歷史 ring、B7 checksum、raw 與 NVS 檢查。 |
| session | PASS：發布延遲、凍結、51／74／61／75、重置與時間回繞。 |
| measurement | PASS：正式狀態機、去抖確認時間、跨界排除、無後續脈衝、佇列溢位。 |
| transport | PASS：保留模式 0 的命令／NVS／重試回歸。 |
| subscriber | PASS：預設模式 1 訂閱／LOAD／Shot 通知，CMD、頁序、失敗重試與世代隔離。 |
| 獨立程式碼審查 | 無 Critical／Important／Minor 發現；另驗證正式量測路徑回繞、清除、下一發與空曲線保存／恢復／重送。 |
| ESP32-C3 目標編譯 | Arduino-ESP32 3.3.11、NimBLE-Arduino 2.5.1；80 MHz／4 MB，乾淨編譯成功，無警告。 |

Flash 601779／1310720 bytes；靜態 RAM 26540／327680 bytes。新 app 601936 bytes；完整燒錄映像 4194304 bytes。日誌見 `docs/compile.log`、`docs/host_tests.log`，校驗值見 `docs/SHA256SUMS.txt`。

註解：宿主測試使用硬體邊界替身；未執行實板燒錄、IR 光學量測、真實 BLE 擷取、斷電實驗、使用者上位機或官方 App 互通。NVS 舊資料可讀，但舊曲線不重算；新語意資料不保證可供舊韌體降版讀取。完整 4 MB 映像會覆寫 NVS；需要保留歷史時使用 Arduino IDE 一般上傳並停用全 Flash 擦除。
