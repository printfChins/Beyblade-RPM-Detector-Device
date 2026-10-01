# 2026-10-01 RPM 正/負緣先觸發固定參考

- [修改] `RPM_IR_TRIGGER_EDGE` 由 `FALLING` 改為 `CHANGE`。
- [新增] 每次量測第一個實際 RPM 邊沿決定固定參考極性。
- [修改] 正緣先到：只使用正緣 -> 正緣完整一圈計算 RPM。
- [修改] 負緣先到：只使用負緣 -> 負緣完整一圈計算 RPM。
- [修改] 另一極性不更新 RPM、MAX 或 BLE 曲線，且同一輪量測中不得切換參考極性。
- [修正] 參考選定後 ISR 只將相同極性送入 RPM Queue，避免 `CHANGE` 模式使 Queue 流量長期加倍。
- [保留] RPM Queue overflow 修正：保留既有曲線/MAX/參考極性，只重新建立同極性週期基準。
- [保留] LOAD 100 ms、SPINNING 後忽略 LOAD 發射判定、前 32 點 BLE 曲線、MAX 獨立更新、RPM < MAX 20% 發射成功。
- [驗證] rising-first、falling-first、固定參考、32 點/MAX、20% 門檻與 overflow 參考保持測試通過；8/8 host tests 通過。

# 2026-10-01 採樣中途重置 / Queue Overflow 修正

- [修正] `SPINNING` 後 LOAD HIGH/LOW 僅更新穩定狀態，不得重置 RPM、MAX 或 BLE 曲線。
- [修正] 只有 `WAIT_LOAD` 狀態下，LOAD 穩定 HIGH 100 ms 才能清零並啟動新一輪檢測。
- [修正] RPM ISR Queue overflow 不再呼叫 `brd_bbp_capture_abort()`，保留已取得的前 32 點與 MAX。
- [修正] RPM Queue overflow 後重新建立下降沿基準，禁止用 overflow 前後不連續的邊沿直接計算 RPM。
- [修正] RPM Queue overflow 事件回放期間暫停 inactivity timeout，避免誤觸發 300 ms 歸零/發射完成。
- [修正] LOAD Queue overflow 不再 `reset_measurement(false)` 或重新啟動整個 input capture；只重新同步目前 LOAD 電位並重新做 100 ms 去抖。
- [驗證] 新增 SPINNING LOAD 變化、RPM Queue overflow、LOAD Queue overflow 回歸測試；8/8 host tests 通過。

# 2026-10-01 LOAD 100 ms / 32 點發射判定

- [修改] 所有實體 LOAD 狀態切換需連續穩定至少 100 ms 才更新穩定狀態。
- [修改] LOAD 模式曲線由裝載後第一筆非 0 有效 RPM 開始；該筆 RPM 的邊沿類型成為固定單圈參考，固定保留前 32 點。
- [新增] LOAD 保持裝載且曲線達 32 點時，直接強制判定發射成功並封存。
- [修改] LOAD 已穩定切為非裝載且曲線未滿 32 點時，RPM 降至本次 MAX 的 20% 以下完成發射；300 ms 無脈衝視為 RPM=0。
- [修正] LOAD 卸載的 100 ms 確認期間若才出現有效 RPM，不再誤判為「卸載前未轉動」。

# BRD_BBPX LOAD / AUTO 雙模式修改紀錄

- [修改] BLE 裝置名稱格式改為 `BEYBLADE_TOOL_BRD_XXXX`。
- `XXXX` 取 `esp_efuse_mac_get_default()` 的 ESP32 eFuse MAC 最後 2 bytes，轉成四位大寫 HEX。
- 例如 eFuse MAC 尾碼為 `0x126A`，BLE Scan Response 名稱為 `BEYBLADE_TOOL_BRD_126A`。


日期：2026-09-25。

## 完整檔案與放置位置

專案根目錄：`BRD_BBPX/`

| 檔案 | 標記 | 內容 |
|---|---|---|
| `BRD_BBPX/brd_config.h` | 新增 | `BRD_MEASUREMENT_MODE_LOAD` / `BRD_MEASUREMENT_MODE_AUTO`、`AUTO_RPM_THRESHOLD=2000`、250 ms 歸零及 1000 ms 發射條件。 |
| `BRD_BBPX/brd_measurement.cpp` | 修改 | AUTO 不掛 LOAD 中斷；RPM-only 狀態機、有效歸零/發射判定、MAX/HOLD 行為、AUTO BLE loaded=RPM!=0。 |
| `BRD_BBPX/brd_oled.cpp` | 修改 | AUTO 模式第一行固定顯示 `AUTO READY`，HOLD 優先；補入 U 字型。 |
| `BRD_BBPX/tests/test_measurement_auto.cpp` | 新增 | AUTO 模式宿主回歸測試。 |
| `BRD_BBPX/tests/run_host_tests.py` | 修改 | 增加 AUTO 測試，總數由 7 組增加為 8 組。 |

主程式 `BRD_BBPX/BRD_BBPX.ino` 不需新增程式碼。


## BLE 裝置名稱尾碼

- [修改] 固定名稱 `BEYBLADE_TOOL01` 改為 `BEYBLADE_TOOLXX`。
- `XX` 直接取 `esp_efuse_mac_get_default()` 取得的 ESP32 eFuse MAC 最後 1 byte，轉成兩位大寫 HEX。
- 例如 eFuse MAC 尾碼為 `0x6A`，BLE Scan Response 名稱為 `BEYBLADE_TOOL6A`。
- Service UUID、Characteristic UUID、BBPX 封包與 LOAD / AUTO 模式均不變。

## CFG 模式選擇

`BRD_BBPX/brd_config.h`：

```c
#define BRD_MEASUREMENT_MODE_LOAD         0U
#define BRD_MEASUREMENT_MODE_AUTO         1U
#ifndef BRD_MEASUREMENT_MODE
#define BRD_MEASUREMENT_MODE              BRD_MEASUREMENT_MODE_LOAD
#endif
#define AUTO_RPM_THRESHOLD                2000U
#define AUTO_LAUNCH_ZERO_MS               1000UL
#define AUTO_RESET_ZERO_MS                250UL
```

預設為 LOAD。要使用 AUTO，只需把 `BRD_MEASUREMENT_MODE` 改成 `BRD_MEASUREMENT_MODE_AUTO`。

## AUTO 行為

- RPM < 2000，且之後 0 RPM 持續至少 250 ms：有效歸零。
- RPM 曾 >= 2000，且之後 0 RPM 持續至少 1000 ms：有效發射並封存 Shot。
- 達到 >=2000 後，250 ms 只把即時 RPM 歸零，不清除有效發射候選。
- AUTO 不讀 LOAD 作為量測條件，也不掛 LOAD interrupt。
- AUTO OLED 顯示 `AUTO READY`；HOLD 優先。
- AUTO 預設 BBPX 相容通知中的 A0 loaded bit：RPM != 0 為已裝載，RPM == 0 為未裝載。
- BBPX 沒有獨立 Launch RPM 欄位，因此不新增發射點 RPM 傳輸；History 代表值仍為整次量測 MAX RPM 的既有 BBPX 映射。
- BBPX 曲線格式維持原本 32 個有效完整週期，未加入 0 RPM 起始點，避免 0 被協議解讀為曲線結束。

## 刪減

本次未刪除 LOAD 模式。原本 LOAD 去抖、HIGH->LOW 發射、MAX 20% 結束、300 ms 無脈衝結算都保留。

舊 R4 `.bin` 與完整 4 MB 映像未包含本次 AUTO 修改，已從交付包移除，避免誤燒。

## 2026-10-01 RPM / Launch logic update
- [修改] RPM 計算改為 FALLING -> FALLING，每一整圈只產生一筆 RPM。
- [修改] LOAD 維持 100 ms 穩定判定；LOAD 僅用於歸零及啟動檢測，不參與發射判定。
- [刪減] 移除 >1000 RPM 才開始曲線的限制。
- [刪減] 移除曲線滿 32 點即強制判定發射成功。
- [修改] 第一筆非 0 RPM 起即寫入曲線，BLE 僅保留最早 32 點。
- [修改] MAX 與曲線獨立；曲線滿 32 點後仍持續更新 MAX。
- [修改] 發射成功條件改為即時 RPM 嚴格小於整次 MAX 的 20%。
- [修改] LOAD 卸載後若量測已開始，仍持續依 RPM 判定直到發射成功。
