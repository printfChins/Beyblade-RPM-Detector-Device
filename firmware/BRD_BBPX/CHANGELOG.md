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
