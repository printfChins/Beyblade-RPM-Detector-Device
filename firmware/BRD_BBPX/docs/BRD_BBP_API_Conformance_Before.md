# BRD_BBP 與 BX-09 API 規格符合性比對

比對日期：2026-09-19  
依據：使用者提供的《BX-09 BeyBattle Pass BLE CMD - API 規格書.md》，文件版本 Reverse Engineering API V1.1。

**目前下載包：30 項裝置端檢查中，28 項符合、0 項部分符合、2 項不符合文件建議。**

註解：這 2 項都是曲線收集策略與文件「建議流程」的差異，並非已證明的原廠封包格式錯誤。本文的「符合」表示程式符合所列檢查條件，不代表已通過 BX-09 實機或官方 App 相容性認證。

## 1. 版本與統計方式

| 比對版本 | 符合 | 部分符合 | 不符合 | 合計 |
|---|---:|---:|---:|---:|
| C：目前 BRD_BBP.zip，預設開啟主動 Notify | 28 | 0 | 2 | 30 |
| O：先前交付的純命令回覆版本 | 24 | 0 | 6 | 30 |
| V：使用者確認可用的 BRD_BBP_V0.1(2).zip | 19 | 4 | 7 | 30 |

註解：文件共有 79 節，其中包含重複說明、範例、上位機演算法及尚未定案內容。以下合併成 30 項裝置端檢查，不把章節數直接當成功能數。4 類尚未定案事項另列於第 5 節，不納入上述分母。

判定方式：

- 符合：可從實際程式確認符合該列條件；數值格式與資料意義分開判斷。
- 部分符合：已有基本功能，但在相同功能內仍有明確限制或流程缺口。
- 不符合：未實作，或實作與所列文件規則／建議不同。屬於建議的項目明確標註。
- 上位機責任：不算成 Peripheral 韌體缺漏。

## 2. 比對來源與完整檔案位置

| 代號 | 實際來源 |
|---|---|
| 規格 | `/workspace/scratch/858468988b82/upload/BX-09 BeyBattle Pass BLE CMD - API 規格書.md` |
| C 壓縮包 | `/workspace/scratch/858468988b82/deliverables/BRD_BBP.zip` |
| C 原始碼目錄 | `/workspace/scratch/858468988b82/ble_fix/BRD_BBP/` |
| O 原始碼快照 | `/workspace/scratch/858468988b82/ble_compare/delivered/BRD_BBP/` |
| V 壓縮包 | `/workspace/scratch/858468988b82/upload/BRD_BBP_V0.1(2).zip` |
| V 原始碼目錄 | `/workspace/scratch/858468988b82/ble_compare/working/BRD_BBP/` |

已逐檔確認：C 壓縮包內的 `.ino/.cpp/.h/.md` 與 C 原始碼目錄一致；V 壓縮包內同類檔案與 V 原始碼目錄一致。O 是保留的先前原始碼快照，不能用目前同名的 BRD_BBP.zip 代替。

| 壓縮包 | SHA-256 |
|---|---|
| C：BRD_BBP.zip | `fbd5e52da9f445c885b6721c04aac3668de583aa2625bdcae7e5bbd6804e057a` |
| V：BRD_BBP_V0.1(2).zip | `9eab3d44e281c54d28f16eb101d3828a44fbc66570d4872935a386704e91aec1` |

下表的 `C/檔名`、`O/檔名`、`V/檔名`，分別接在上述原始碼目錄後面；行號均對應實際檔案。主要檔案如下：

| 用途 | 完整檔案位置 |
|---|---|
| C 主程式 | `/workspace/scratch/858468988b82/ble_fix/BRD_BBP/BRD_BBP.ino` |
| C 組態 | `/workspace/scratch/858468988b82/ble_fix/BRD_BBP/brd_config.h` |
| C BLE 傳輸 | `/workspace/scratch/858468988b82/ble_fix/BRD_BBP/brd_bbp.cpp` |
| C 封包與曲線 | `/workspace/scratch/858468988b82/ble_fix/BRD_BBP/brd_bbp_protocol.cpp` |
| C 命令與紀錄 | `/workspace/scratch/858468988b82/ble_fix/BRD_BBP/brd_bbp_session.cpp` |
| C 量測狀態機 | `/workspace/scratch/858468988b82/ble_fix/BRD_BBP/brd_measurement.cpp` |
| O BLE 傳輸 | `/workspace/scratch/858468988b82/ble_compare/delivered/BRD_BBP/brd_bbp.cpp` |
| V BLE 與封包 | `/workspace/scratch/858468988b82/ble_compare/working/BRD_BBP/brd_ble.cpp` |
| V 組態 | `/workspace/scratch/858468988b82/ble_compare/working/BRD_BBP/brd_config.h` |
| V 量測狀態機 | `/workspace/scratch/858468988b82/ble_compare/working/BRD_BBP/brd_measurement.cpp` |
| V 事件紀錄 | `/workspace/scratch/858468988b82/ble_compare/working/BRD_BBP/brd_record.cpp` |

## 3. 30 項完整比對表

| ID | 檢查項目 | 規格節次 | C 目前包 | O 先前版 | V 可用版 | 程式證據與註解 |
|---|---|---|---|---|---|---|
| 01 | 名稱 BEYBLADE_TOOL01 | 3、75 | 符合 | 符合 | 符合 | C/brd_config.h 第 29 行；V/brd_config.h 第 107 行。C 將完整名稱放於 Scan Response。 |
| 02 | Service UUID 為 55C40000-F8EB-11EC-B939-0242AC120002 | 3、75 | 符合 | 符合 | 符合 | C/brd_config.h 第 30 行；V/brd_config.h 第 109 行；均用該 UUID 建立並廣播服務。 |
| 03 | F002 為 Notify 通道，使用 Notification | 4、65 | 符合 | 符合 | 符合 | C/brd_bbp.cpp 第 229、401 行；V/brd_ble.cpp 第 263、498 行。實際 Write UUID 是否與原廠一致另列 U1。 |
| 04 | 同時支援 WRITE 與 WRITE WITHOUT RESPONSE（建議） | 5 | 符合 | 符合 | 部分符合 | C/brd_bbp.cpp 第 230 行有 WRITE、WRITE_NR；V/brd_ble.cpp 第 265 行只有 WRITE_NR。 |
| 05 | 命令為 1 byte；嚴格按此長度處理 | 5 | 符合 | 符合 | 部分符合 | C/brd_bbp.cpp 第 112 行拒絕非 1 byte；V 第 151、517 行接受 1 至 20 bytes 並僅判讀首 byte。V 能接受正常 1-byte 命令，但未嚴格限制格式。 |
| 06 | 通知固定 17 bytes；多 byte 數值為 Little Endian | 6、22、58 | 符合 | 符合 | 符合 | C/brd_bbp_protocol.h 第 12 行、.cpp 第 10 行及 brd_bbp.cpp 第 401 行；V/brd_ble.cpp 第 83、496 行。此列只核對封裝，不代表 Profile 數值單位正確。 |
| 07 | A0 header、0x3A、MAX、總次數與 6-byte UID 的位置 | 9 | 符合 | 符合 | 符合 | C/brd_bbp_protocol.cpp 第 125 行；V/brd_ble.cpp 第 342 行。狀態、電量的意義另列 08、09。 |
| 08 | 相容解讀：Loaded=0x04，Unloaded=0x00 | 10 | 符合 | 不符合 | 符合 | C/brd_bbp.cpp 第 295、325 行按去抖後 LOAD；V 第 523、604 行相同。O 的 Session::flags() 以有效轉動至發布前為 0x04，靜態裝載不會設置，卸載後也可能暫留。只按文件的 atlas/BeyMeter 相容解讀判定。 |
| 09 | A0[4] 提供隨電池狀態變化的 Battery Raw | 9 | 符合 | 符合 | 不符合 | C/brd_bbp.cpp 第 293 行使用實際 SOC 映射；V/brd_config.h 第 142、143 行將兩種狀態固定為 0。此列不認定 C 的 0 至 250 標定等同原廠。 |
| 10 | 每個正常 0x51 請求可取得一包 A0 | 8、70 | 符合 | 符合 | 部分符合 | C/brd_bbp_session.cpp 第 68 行搭配命令佇列。V/brd_ble.cpp 第 510、526 行一次清空佇列、只設一個 pending 布林值；多個已排入的 0x51 可能合併成一次回覆。一般低頻輪詢有回覆。 |
| 11 | 0x61 安全忽略，不自行清除或重設 | 64、70 | 符合 | 符合 | 符合 | C/brd_bbp_session.cpp 第 79 行安全忽略；V/brd_ble.cpp 第 527 行當作未知命令，只累計診斷。這不表示已解析其原廠用途。 |
| 12 | 0x74 回傳完整 B0 至 B7、70 至 73，並可再次要求重送最新 Shot | 13、61、62、68、70、77 | 符合 | 符合 | 不符合 | C/brd_bbp_session.cpp 第 71 行重新建立 12 頁；已保存的 State 不因讀取而清除。V/brd_ble.cpp 第 517 行只有 0x51 分支，0x74 會被忽略。 |
| 13 | 0x75 真正清除 Shot History | 63、70、77 | 符合 | 符合 | 不符合 | C/brd_bbp_session.cpp 第 74 行清除 State、作廢本次 capture 並標記保存；V 命令派送器未實作。C 的 NVS 寫入仍受安全時段與寫入成功條件限制。 |
| 14 | 其他未知命令安全忽略 | 70 | 符合 | 符合 | 符合 | C/brd_bbp.cpp 第 121 行；V/brd_ble.cpp 第 527 行。 |
| 15 | 只訂閱 Notify，無須先寫命令即可收到初始通知 | 2、12、73 | 符合 | 不符合 | 符合 | C/brd_bbp.cpp 第 326、384 行，預設等訂閱穩定 100 ms 後送 A0；V 第 578 行亦排入 A0。O 第 310 行只從命令佇列建立回覆。 |
| 16 | 已識別的 LOAD 改變主動送 A0 | 71 | 符合 | 不符合 | 符合 | C/brd_bbp.cpp 第 334 行；V/brd_ble.cpp 第 599 行。指經既有去抖／狀態機確認的變化，不是每個 GPIO 毛刺；HOLD 自鎖期間沿用原量測限制。 |
| 17 | Shot 完成、更新資料後主動通知 History/Profile | 71、72 | 符合 | 不符合 | 符合 | C/brd_bbp.cpp 第 316、337、387 行；V 第 611、619、640 行。O 完成後只更新本地紀錄，等待 0x74。 |
| 18 | 約 500 ms 是主機輪詢策略，不當作裝置固定 Notify 週期 | 11 | 符合 | 符合 | 符合 | 三版均無固定每 500 ms 自動 A0。C 的 10 ms、V 的 8 ms 是待送封包間隔，不是每隔該時間必有一次狀態通知。 |
| 19 | History 1 至 48 於 B0 至 B5，49／50 於 B6，offset 正確 | 14、15 | 符合 | 符合 | 符合 | C/brd_bbp_protocol.cpp 第 144、147 行；V/brd_ble.cpp 第 356、378 行。 |
| 20 | B6[7..8] MAX、[9..10] Total、[11] Count/Index | 17 | 符合 | 符合 | 符合 | C/brd_bbp_protocol.cpp 第 151 行；V/brd_ble.cpp 第 380 行。V 用 put_u16 寫入 count，但 count 最大 50，因此第 11 byte 正確、第 12 byte 為 0。 |
| 21 | 保存最近 50 筆，History 長度與累計次數分開 | 16 | 符合 | 符合 | 部分符合 | C/brd_bbp_protocol.cpp 第 109 行使用 ring，且 brd_bbp.cpp 第 315 行在連線判定前更新紀錄。V 有 50 筆移位緩衝，但第 586 行未連線即返回；連續離線發射可能在下次 brd_record_begin/reset 時覆蓋尚未加入歷史的結果。V 的總次數也在 65535 飽和。 |
| 22 | B7[16] 為 B0 至 B6 payload 總和 mod 256，不含 header | 18、42 | 符合 | 符合 | 符合 | C/brd_bbp_protocol.cpp 第 154 行；V/brd_ble.cpp 第 385 行。Checksum 未擴充到 Profile。 |
| 23 | Profile 是 70、71、72、73 四頁，每頁 8 個 uint16，共 32 槽 | 19 至 22、58、59 | 符合 | 符合 | 符合 | C/brd_bbp_protocol.cpp 第 141、159 行；V/brd_ble.cpp 第 406 行。此列只判定頁面與槽位結構。 |
| 24 | Profile 使用連續每圈週期的 nRefs，按 period_us/8 編碼，非直接 RPM 或固定時間取樣 | 19、23 至 26、43 至 50、52、55、74、79 | 符合 | 符合 | 不符合 | C/brd_measurement.cpp 第 204、226 行傳入週期，protocol.cpp 第 56 行四捨五入與限制 uint16 範圍。V/brd_ble.cpp 第 473 行直接寫 sample.rpm；且來源是雙邊沿各自形成的一圈 RPM 事件，未建立單一 FALLING 週期的 raw 陣列。 |
| 25 | 有效點少於 32 時，其餘槽位補 0 | 27、28、50、53 | 符合 | 符合 | 不符合 | C/Profile::reset() 清零、append() 只複製 size() 筆。V/brd_ble.cpp 第 461 行固定填滿 32 槽，資料不足時會重複選取既有事件；不會依有效點數補尾端 0。 |
| 26 | 超過 32 圈保留初期前 32 個有效週期（建議） | 54 | 不符合 | 不符合 | 不符合 | C/O 的 protocol.cpp 第 69 行遇到後續更高峰會重建視窗。V/brd_ble.cpp 第 467 行跨整段事件按索引均勻選取，亦非前 32 圈。文件未證實原廠的超長曲線選點策略。 |
| 27 | Launch 偵測後開始 Profile Capture（建議流程） | 57、72 | 不符合 | 不符合 | 不符合 | C/O 的 measurement.cpp 第 193、226 行於 SPINNING_LOADED 就收資料；Session::launched() 只設旗標、不清除前段。V/measurement.cpp 第 213 行在第一個裝載轉動邊沿開始 record，第 278 行才標記 launch。 |
| 28 | Shot End 凍結資料；Profile 容量不直接決定量測結束 | 56、57 | 符合 | 符合 | 符合 | C/measurement.cpp 第 164 行 finish、Session 第 20 行阻擋已 pending 的新點；結束由轉速門檻／無脈衝條件控制。V/record.cpp 第 138 行完成紀錄後才由 BLE 更新 profile；32 槽不控制量測終止。 |
| 29 | BX-09 封包不自行加入 ACK、NACK、SEQ 或 Session ID | 65、66、77 | 符合 | 符合 | 符合 | 所有編碼函式均維持 17-byte 原頁面欄位。內部 generation/session 及 NVS/record CRC 僅供程式管理，沒有塞入 BLE 封包。 |
| 30 | Connect／Init／Reconnect 不自動執行 0x75 清除 | 63 | 符合 | 符合 | 符合 | C/O 的連線 callback 只改連線狀態；clear 在 Session::command(0x75)。V 連線只清命令佇列，不清既有 RAM history。此列不等同宣稱 V 具斷電保存功能。 |

## 4. 差異的實際影響

### C：目前下載包的 2 項差異

**26：後續更高峰會取代原本前 32 點。**

實際執行 Profile 類別：先輸入 40 個週期，使 raw 依序為 1000 至 1039，原本保存 1000 至 1031。再輸入 period=4000 us（raw=500）後，保存資料改為 raw 1032 至 1039，加上 raw 500，共 9 個有效點。

註解：已確認不是永遠固定前 32 圈。仍是連續週期資料，也未做平均或插值；差異是視窗起點及保留策略。接收端會從新視窗開始累加時間，不能把該曲線的時間原點視為原本第一圈。

**27：Profile 含有 Launch 前的轉動資料。**

實際執行 Session：先在 launched() 前輸入 period=8000 us，再呼叫 launched()，接著輸入 period=4000 us，finish 並發布。保存曲線前兩點是 1000、500，表示發射前的那一點仍在資料中。

註解：這與文件「Launch detected 後開始 Capture」建議流程不同。是否應保留拉轉加速段，需要配合實際 IR 可觀測區間與發射定義；本報告只指出差異，未把文件建議當作已確認的原廠韌體實作。

### O：先前交付版另外少了 4 項

相較 C，O 另外不符合 08、15、16、17：A0 bit2 的相容狀態語意、訂閱後通知、LOAD 事件通知、Shot 完成通知。

註解：若上位機只訂閱 F002 而不寫入 0x51／0x74，O 沒有通知來源；C 與 V 有。這是原始碼能證實的流程差異，尚不能單憑此報告認定使用者所有實機問題都由它造成。

### V：使用者可用版的 7 項不符合

| ID | 不符合內容 | 影響 |
|---|---|---|
| 09 | Battery Raw 固定 0 | 無法從該欄取得實際電池狀態。 |
| 12 | 未實作 0x74 | 缺頁後無法依文件要求重讀整組資料。 |
| 13 | 未實作 0x75 | 無法透過標準命令清除歷史。 |
| 24 | Profile 直接存 RPM | 接收端以 nRefs 解碼後，SP 與時間都會錯。 |
| 25 | 不足 32 點仍重複填滿 | 會產生原本未量測到的重複時間段。 |
| 26 | 跨整段抽取事件 | 不符合保留初期前 32 圈的建議。 |
| 27 | 發射前就開始收集 | 不符合 Launch 後開始 Capture 的建議流程。 |

數值例子：15000 RPM 應編碼 nRefs=500，LE 為 F4 01；上位機解出 SP=15000、dt=4 ms。V 直接填入 15000（LE 為 98 3A），依這份文件會被解成 SP=500、dt=120 ms。

V 的 4 項部分符合為 04、05、10、21：只支援 WRITE_NR、命令長度未嚴格限制、佇列內 0x51 可能合併、離線發射的歷史保存缺口。

註解：V 能與特定上位機交換通知，不代表 Profile 數值及所有 CMD 都符合此文件。

## 5. 4 類無法依本規格定案的事項

| 代號 | 尚未定案事項 | 本次能確認到的範圍 |
|---|---|---|
| U1 | 原廠 Write Characteristic UUID、是否必須是另一個特徵 | 三版均使用 F002 接收命令；文件第 4.1、76 節明確說實際 Write UUID 未完全確認。因此不能認定與原廠完整 GATT 相同，也不能自行認定應改 F001。 |
| U2 | A0 未知欄位、完整 flags 意義、Battery Raw 標定 | 08 只評 atlas/BeyMeter 的 loaded/unloaded 相容解讀；09 只評是否送出電池相關值。A0[2]/[5]/[6]、所有原廠 bit 與電池換算精確一致性未證實。 |
| U3 | 0x61 真正用途及 0x75 原廠精確回覆序列 | 安全忽略 0x61、實作清除、不增加自創 ACK，可依文件建議判定；未知的原廠功能與回覆順序不能宣稱完成。 |
| U4 | 原廠最終 SP 評分與 Profile 選點、發射位置的精確演算法 | 文件允許 BRD 先採 SP 約等於 RPM 的測試映射。C/O 使用 Profile 峰值當代表 SP；V 使用 MAX RPM。均未證明等同官方 Stored SP。 |

註解：C/O 的 NVS 解碼還要求最後一筆 History SP 等於所存曲線的峰值（brd_bbp_protocol.cpp 第 226 行）。這能檢查目前自產資料的內部一致性，但不能拿來當作 BX-09 原廠資料必然成立的規則。文件第 31、33 節明確指出 Stored SP 與 Profile Max SP 不必相等；本專案未實作原廠資料匯入 Parser，所以此處列為假設界線，未再算一項裝置端缺漏。

## 6. 不計入韌體未完成項目的上位機工作

| 上位機責任 | 規格節次 | 判定理由 |
|---|---|---|
| nRefs 解碼、dt 累加、SP 曲線、raw=0 跳過 | 24 至 30、60 | Peripheral 負責正確送 raw；主機負責建立座標軸。 |
| Stored SP／Estimated SP 分開、最少 7 點、前段局部峰值分析 | 31 至 35 | 文件明確是上位機分析演算法，不要求 Peripheral 重現。 |
| A0 與 Profile 接收時間關聯及 launch marker | 36 | 主機的估算不能取代 raw 所定義的量測時間。 |
| 依 Header 組包、重複頁覆蓋、缺頁偵測、完整頁集合 | 37 至 41、67、73 | BRD_BBP 是資料發送端，沒有接收 B0 至 B7／70 至 73 的 App。 |
| 校驗 B7、丟棄部分接收資料、再送 0x74 | 38、62、68、69 | 主機偵測與發起重讀；Peripheral 的配合能力已計入第 12 項。 |

## 7. 本次驗證紀錄

本次重新執行 C 專案現有的 tests/run_host_tests.py，結果為：

| 測試組 | 結果 |
|---|---|
| protocol | PASS |
| session | PASS |
| measurement | PASS |
| transport | PASS |
| subscriber | PASS |

另直接呼叫正式 Profile／Session 程式，重現第 26、27 項差異；未修改任何正式原始碼。30 列判定與統計另以程式核對數量。

註解：現有測試通過不代表符合全部規格。其中 protocol 測試本來就接受後段峰值換窗，session 測試也接受 Launch 前資料。本次以使用者的新規格逐項對照，才把這兩個行為列出。以上為原始碼與宿主驗證，未執行實板 BLE 擷取、原廠 App 互通驗證或官方 SP 校正。

本次交付為比對報告；正式程式、主程式放置位置及資料夾結構均未改寫。
