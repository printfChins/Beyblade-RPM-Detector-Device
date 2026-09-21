# BRD_BBP 與 BX-09 API 比對 — R3 單圈曲線版

日期：2026-09-21。修訂識別：`API_V1.1_R3`。依最新使用者要求：保留雙邊沿觸發，採裝載後最先觸發的邊沿作本次曲線參考，每完整一圈一筆；放大 OLED 連線圖示。

**原表 30 項檢查：29 項符合、0 項部分符合、1 項不符合。** 第 24、26 項恢復單圈曲線語意；第 27 項文件建議「發射後起錄」，本版依使用者要求保留裝載後起錄，不以此宣稱原文全符合。

## 1. 版本比較

| 版本 | 符合 | 部分符合 | 不符合 | 合計 |
|---|---:|---:|---:|---:|
| R3，本次模式 1 | 29 | 0 | 1 | 30 |
| R2，前次雙點曲線 | 27 | 2 | 1 | 30 |
| R1，歷史報告原判定 | 30 | 0 | 0 | 30 |

原 API 原文在 `docs/BX09_API_V1.1.md`。本次以相同 30 項核對，不將未確認的原廠事項算入通過。模式 0 為舊輪詢回歸，不屬於本表預設模式。

## 2. 完整檔案與位置

- 完整原始碼：`/workspace/scratch/858468988b82/single_curve/working/BRD_BBP/`。
- 交付 ZIP：`/workspace/scratch/858468988b82/deliverables/BRD_BBP.zip`。
- 開啟 `BRD_BBP/BRD_BBP.ino`，全部根目錄 `.cpp/.h` 與主程式放同層。
- 6 個正式修改檔案及主要行號見 `CHANGELOG.md`。
- R2→R3 程式／測試差異：`docs/BRD_BBP_SINGLE_CURVE_R3.patch`。
- R2 基準 SHA-256：`108f8a8071b649c1a5e07920132ce484c63e458a6bf6a6be851bcce2fb3a685e`。

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

## 4. 本次要求與實際結果

| 要求 | R3 行為與驗證 |
|---|---|
| 曲線單圈計算 | 一個完整參考週期一點；32 圈測試保存前 16 圈 raw750、後 16 圈 raw1000，沒有因收雙邊沿而提前填滿。 |
| 雙邊沿觸發，先到者為參考 | 先到正緣則固定正緣，先到負緣則固定負緣；測試亦涵蓋 GPIO 初始電位與第一筆有效 RPM 不決定參考。 |
| 裝載後有效轉動開始記錄 | 首個參考事件只建立時間基準，下一有效同類事件完成整圈即加入 Profile；發射不清除前段。 |
| 即時 RPM 與 MAX | 兩類邊沿都更新；非參考讀值可提高 MAX，但不占曲線槽位。正式 BLE 通知測試驗證 MAX15000、Profile raw750／1500／0。 |
| OLED 藍牙圖示放大 | 2 倍點陣、14×14 區域，原點 (114,16)。實際亮點邊界 x114..125、y16..29，五位數最右像素 x105，互不重疊。 |

連續有效取樣時，每筆 `raw/125.0` 的毫秒時間直接累加，已移除 R2 的重疊整圈問題。第一個參考事件前、被排除的資料、長暫停與飽和週期無法由此格式還原成完整絕對時間軸。新規則從下一筆新 Shot 生效，既有 NVS 曲線不重算。

## 5. 原廠未確認事項

| 代號 | 事項 | 本包範圍 |
|---|---|---|
| U1 | 原廠 Write UUID | 本包 F002 同時 Notify／Write／Write NR，不推定另一個原廠 UUID。 |
| U2 | 完整 A0 flags、未知欄位、電池標定 | 只核對 Loaded 相容語意與 SOC 映射。 |
| U3 | 0x61 用途、0x75 原廠精確回覆 | 忽略 0x61、真正清除 0x75，不加入自創 ACK。 |
| U4 | 官方 Stored SP、選點及內部演算法 | 使用整次雙邊沿 MAX RPM 作 BRD 代表 SP，未證明等同原廠評分。 |

## 6. 驗證

7 組宿主測試通過，含 15 個雙邊沿／單圈參考案例與正式 BLE wrapper 的取樣選擇通知驗證。使用 g++ C++17、Wall／Wextra／Werror、ASan／UBSan。獨立審查無 Critical、Important、Minor 發現。

ESP32-C3：Arduino-ESP32 3.3.11、NimBLE-Arduino 2.5.1；80 MHz／4 MB 乾淨編譯成功，無警告。Flash 602127／1310720 bytes，靜態 RAM 29628／327680 bytes；app 602272 bytes，完整映像 4194304 bytes。映像各區已比對本次編譯輸出。

OLED 預覽由實際繪圖函式輸出 framebuffer，見 `docs/OLED_BLE_R3_Preview.png`。全部結果與限制見 `VALIDATION.md`，日誌與校驗值在 `docs/`。

註解：未執行實板燒錄、IR 波形、中斷延遲、真實 BLE／官方 App、斷電保存驗證。完整 4 MB 映像會覆寫 NVS；保留歷史請用一般專案上傳並停用全 Flash 擦除。
