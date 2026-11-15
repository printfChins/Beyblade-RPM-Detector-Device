# BRD BPX V1.4.4 BLE 設定 API

[修改] 依實際 `brd_settings_model.cpp`、`brd_settings_ble.cpp`、`brd_ble_identity.cpp` 說明。舊版說明保存在 `docs/SETTINGS_API_V1.4.3.md`。本版不修改 BattlePass BBP 封包 17 bytes、原 BBP Service UUID、OTA UUID 與 GPIO / RPM ISR。

## BLE 掃描及連線

- [V1.4.4 修改] 出廠名稱：`BEYBLADE_TOOL01`（不再自動附加 MAC 尾碼）。已在 V1.4.4 中保存過名稱時，開機優先使用 NVS 自訂名稱。
- [保留] BBP Service UUID：`55c40000-f8eb-11ec-b939-0242ac120002`；BBP Characteristic UUID：`55c4f002-f8eb-11ec-b939-0242ac120002`。
- [保留] 獨立設定服務 UUID：`c7570001-9040-4c7b-87b1-086ef858ace7`。設定服務 UUID 不另外放入廣播，連線後進行 GATT Discover 即可找到。
- [重要] 更名後原連線不會被踢除；斷線且歷史/設定安全後才重新初始化 BLE，更新 Scan Response 廣播名稱。改名不改硬體 UID，原 BBP 傳輸資料維持原識別資料。

## GATT Characteristics

所有 UUID 的尾碼與上列設定服務共用 `-9040-4c7b-87b1-086ef858ace7`。

| 尾碼前 8 碼 | 用途 | 屬性 | 回覆格式 |
|---|---|---|---|
| `c7570002` | 原設定命令 | WRITE | 8-byte 指令 |
| `c7570003` | 原設定回覆 | READ / NOTIFY | 20-byte 快照 |
| `c7570004` | 原 OLED 頁面 | READ / WRITE | 1-byte，0 主頁 / 1 IR TEST |
| `c7570005` | [新增] 韌體版本 | READ | UTF-8 字串 `V1.4.4`（無結尾 NUL） |
| `c7570006` | [新增] BLE 裝置名稱 | READ / WRITE | UTF-8/ASCII 字串（1～20 bytes） |
| `c7570007` | [新增] BLE 名稱保存狀態 | READ | 1-byte 狀態碼 |
| `c7570008` | [新增] 電池容量 | READ | 4-byte：生效 uint16 LE + 草稿 uint16 LE（mAh） |
| `c7570009` | [新增] HOLD 時間 | READ | 4-byte：生效 uint16 LE + 草稿 uint16 LE（ms） |

### 可透過原設定命令修改的參數

| ID | 名稱 | 允許值 | 預設 |
|---|---|---|---|
| `01` | 模式 | 0=LOAD, 1=AUTO | 0 |
| `02` | AUTO RPM 最低門檻 | 1～59999 | 2000 |
| `03` | MAX 結束百分比 | 1～100 | 30 |
| `04` | 充電時關閉 BLE | 0=否, 1=是 | 0 |
| `05` | [修改] 電池容量 | **200、250、300 mAh** | 200 |
| `06` | [新增] HOLD 自鎖時間 | **1000～10000 ms** | 2500 |

[重要] SET 僅修改草稿。**只有收到 SAVE/SAVED 後才會套用及持久化**。不能在轉動中套用新設定。LOAD、AUTO 目前共用單一 HOLD 時間；HOLD 的狀態機、MAX、下一次裝載去抖未被改寫。

## 原設定命令（完全維持 8-byte / 20-byte 協定）

請求：`C1 01 CMD SEQ PARAM VALUE_0 VALUE_1 VALUE_2`，Value 為 uint24 Little Endian。

- `CMD=01` GET；`CMD=02` SET；`CMD=03` SAVE；`CMD=04` DEFAULTS；`CMD=05` DISCARD。
- GET、SAVE、DEFAULTS、DISCARD 的 `PARAM` 與 `VALUE` 必須全部為 0。
- SEQ 為客戶端自訂 0～255 序號，應逐筆遞增；讀取時確認回覆 `byte2/byte3` 與命令/序號一致。

| 操作 | HEX 8 bytes |
|---|---|
| GET (序號 01) | `C1 01 01 01 00 00 00 00` |
| 設定 200 mAh (序號 02) | `C1 01 02 02 05 C8 00 00` |
| 設定 250 mAh (序號 03) | `C1 01 02 03 05 FA 00 00` |
| 設定 300 mAh (序號 04) | `C1 01 02 04 05 2C 01 00` |
| 設定 HOLD=4000 ms (序號 05) | `C1 01 02 05 06 A0 0F 00` |
| SAVE (序號 06) | `C1 01 03 06 00 00 00 00` |
| DISCARD (序號 07) | `C1 01 05 07 00 00 00 00` |

回覆為 20 bytes：`C2 01 CMD SEQ STATUS PARAM FLAGS MODE AUTO_L AUTO_H BLE_OFF STAGED_MODE STAGED_AUTO_L STAGED_AUTO_H STAGED_BLE_OFF FINISH STAGED_FINISH LAST_SAVE REV_L REV_H`。

- `FLAGS` bit0=PENDING；bit1=DIRTY；bit2=NVS 尚未保存/保存失敗；bit3=執行 300mAh；bit4=草稿 300mAh；**bit5=執行 250mAh；bit6=草稿 250mAh**。200mAh 時 bit3/4/5/6 均為 0。
- `STATUS`：00=OK、01=PENDING、02=SAVED、03=BAD_LENGTH、04=BAD_VERSION、05=BAD_COMMAND、06=BAD_PARAMETER、07=OUT_OF_RANGE、08=BUSY、09=FLASH_ERROR、0A=BAD_MAGIC。
- 原版回覆 20 bytes 沒有 HOLD 欄位；**讀取 HOLD 請使用 `c7570009`**，容量也建議直接讀 `c7570008` 取得完整數值，而非自行解碼 bit 位元。
- WAIT/READY 時若沒在旋轉，可以先 SET 多個參數、再 SAVE；SAVE 接受後收到 PENDING，須繼續等待 SAVED。允許 GET 在測速時讀值，SET/SAVE 在量測不安全時回 BUSY。

## BLE 更名獨立 API

對 `c7570006-9040-4c7b-87b1-086ef858ace7` 寫入直接的 ASCII 位元組，例如 `BEYBLADE_TOOL02`，不含 `\0`，不使用 C1 封包。

- [新增] 可用字元：ASCII `A-Z`、`a-z`、`0-9`、底線 `_`、減號 `-`。長度 **1～20 bytes**，可在 ATT MTU=23 的 Write Request 單包寫入。
- [新增] 名稱保存與原 SET/SAVE 分開。BLE Write ACK 只能表示藍牙收件，不代表 NVS 已保存。
- `c7570007` 讀取狀態：`00`=IDLE；`01`=PENDING；`02`=SAVED；`03`=INVALID；`04`=BUSY；`05`=FLASH_ERROR。
- `c7570006` 的 READ 永遠回傳**已保存且生效**的名稱，未保存的新草稿不回傳。
- [保留] callback 不寫 Flash。名稱由主 loop 在 RPM 安全窗口、正常電量與非 OTA 期間保存並讀回驗證。保存失敗保留舊名稱；需要重寫才能再次嘗試。
- [重要] 保存成功並不會強行中斷現有連線；**斷線後**、無待保存交易和歷史紀錄時，BLE 重新廣播新名稱。手機可能有裝置名稱快取，可重新掃描或關閉再開啟藍牙。

## 電池容量與充電時間

原 `BATTERY_CHARGE_CURRENT_MA=100U` 不變。`BATTERY_CHARGE_SOC_RISE_INTERVAL_MS(capacity_mah)` 的公式未改：

`3600000 × capacity_mAh ÷ (100 × charge_current_mA)`（ms/1%）。

| 設定容量 | 估計上升 1% | 理論 0 → 100% |
|---|---:|---:|
| 200 mAh | 72 秒 | 120 分鐘 |
| 250 mAh | 90 秒 | 150 分鐘 |
| 300 mAh | 108 秒 | 180 分鐘 |

[保留] 充電百分比必須低於 ADC 電壓查表限制才會按限速上升。容量切換在 SAVE 後，下一筆 ADC 將重新計算升幅計時起點；不會修改物理充電電流，且實際充電進入 CV 階段時間可能更長。原放電 21 點電壓曲線、低電鎖定及 FULL 政策未更動。

## NVS 相容性

- 原設定 namespace/key：`brd_cfg/settings_v1`，**key 不變**，新編碼 `BRDC` + version 3，長度 20 bytes：byte5～11 保留舊四個設定與電池容量；byte12～13 = HOLD uint16 LE；byte14～15 = 0；byte16～19 = 前 16 bytes CRC32/IEEE。
- [V1.4.4 新增] 開機相容原 `v1`、`v2` 固定 16 bytes blob，並補入 `HOLD=2500ms`；讀到後**不會立刻重寫**，下次 SAVE 才遷移到 v3。
- [保留] BBP 歷史 namespace `brd_bbp/history_v1`、原 OTA 分割區不變。
- 名稱獨立 namespace/key：`brd_identity/name_v1`，25 bytes（`BRDN`、1-byte 長度、20-byte ASCII 名稱欄）。首次無名稱設定時預設 `BEYBLADE_TOOL01`。
- [重要] 若降回只支援 v1/v2 的舊韌體，舊版可能不識別 v3 設定並改用預設值；建議備份設定並保留 NVS，不要在燒錄時 Erase All Flash。

## 使用工具

提供 `tools/BRD_BPX_BLE_Config.html`。Chrome/Edge 在 HTTPS 或 localhost 環境開啟後，按「搜尋並連線」，能讀韌體版本、容量、HOLD、名稱，並直接保存三項設定。BLE 操作須在裝置具備有效電量、未執行 OTA、未轉動時進行。
