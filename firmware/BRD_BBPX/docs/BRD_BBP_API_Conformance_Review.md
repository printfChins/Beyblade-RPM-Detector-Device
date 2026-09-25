# BRD_BBP 與 BX-09 API 比對 — R4 OLED 圖示版

日期：2026-09-21。修訂識別：`API_V1.1_R4`。

本次僅將 OLED 藍牙符號替換為使用者附件 `BRD_BLE_OLED(3).zip` 的原生 7×13 點陣。逐檔比對確認其他正式程式與 R3 一致，通訊／曲線判定沿用 R3：**29 項符合、0 項部分符合、1 項不符合**。

註解：第 27 項原文件建議發射後起錄；本專案依使用者要求，在裝載後取得有效參考週期即起錄。這項差異與 OLED 圖示無關，未宣稱原文件全符合。

完整原始碼：`/workspace/scratch/858468988b82/oled_reference/working/BRD_BBP/`。修改位置與整檔替換方法在 `CHANGELOG.md`，R3 詳細報告保留於 `docs/BRD_BBP_API_V1.1_R3_Review.md`。

## 沿用的 30 項判定

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
| 10 | 每個正常 0x51 請求可取得一包 A0 | 8、70 | 符合 | `brd_bbp_session.cpp` 第 83 行；命令佇列逐筆派送，每個正常請求可取得 A0。 |
| 11 | 0x61 安全忽略，不自行清除或重設 | 64、70 | 符合 | `brd_bbp_session.cpp` 第 94 行；忽略未知功能，不修改資料。 |
| 12 | 0x74 回傳完整 B0 至 B7、70 至 73，並可再次重送最新 Shot | 13、61、62、68、70、77 | 符合 | `brd_bbp_session.cpp` 第 86 行；建立最新 State 的 12 頁，支援整組重讀。 |
| 13 | 0x75 真正清除 Shot History | 63、70、77 | 符合 | `brd_bbp_session.cpp` 第 89 行；清除 State、作廢 Capture、標記保存；Flash 受安全時段與寫入成功條件限制。 |
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
| 24 | Profile 使用連續每圈週期的 nRefs，按 period_us/8 編碼 | 19、23 至 26、43 至 50、52、55、74、79 | 符合 | `brd_measurement.cpp` 第 206、243 行，`brd_bbp_session.cpp` 第 38 行；以本次首個邊沿固定參考，只保存相鄰同類整圈，不重複收兩類週期。 |
| 25 | 有效點少於 32 時，其餘槽位補 0 | 27、28、50、53 | 符合 | `brd_bbp_protocol.cpp` 第 96 行；先清零，再只複製有效點；包括全零 Profile。 |
| 26 | 超過 32 圈保留初期前 32 個有效週期（建議） | 54 | 符合 | `brd_bbp_protocol.cpp` 第 54 行；每完整一圈一筆，固定保留前 32 個有效參考週期，滿後只繼續更新代表 MAX。 |
| 27 | Launch 偵測後開始 Profile Capture（建議流程） | 57、72 | 不符合 | `brd_bbp_session.cpp` 第 22、42 行；依使用者要求從裝載後取得第一筆有效參考整圈起錄，發射時保留前段。 |
| 28 | Shot End 凍結資料；Profile 容量不直接決定量測結束 | 56、57 | 符合 | `brd_bbp_session.cpp` 第 49 行；Shot End 凍結，滿 32 圈後仍可更新代表值。 |
| 29 | BX-09 封包不自行加入 ACK、NACK、SEQ 或 Session ID | 65、66、77 | 符合 | `brd_bbp_protocol.cpp` 第 113 行；維持原 17-byte 欄位；未加入 ACK、NACK、SEQ 或 Session ID。 |
| 30 | Connect／Init／Reconnect 不自動執行 0x75 清除 | 63 | 符合 | `brd_bbp.cpp` 第 71 行；連線 callback 只處理連線狀態，不執行 0x75。 |


## 本次 OLED 驗證

原生 7×13，亮點位於 x119..125、y17..29。來源圖示函式與本次連線／斷線畫面差異逐點相同；五位數 RPM/MAX 與 HOLD、電池、充電圖示均無遮擋。

ESP32-C3 編譯成功且無警告。Flash 602037 bytes、靜態 RAM 29628 bytes、app 602192 bytes、完整映像 4194304 bytes；各映像區段已比對。

本次未重跑 R3 的 7 組通訊／曲線宿主測試，歷史結果不當成本次新增驗證。尚未實板測試。詳 `VALIDATION.md` 與 `docs/OLED_BLE_R4_Preview.png`。

原廠 Write UUID、完整 A0／電池標定、0x61／0x75 精確原廠語意、Stored SP／選點演算法仍未完全確認；本次圖示替換不改變這些限制。
