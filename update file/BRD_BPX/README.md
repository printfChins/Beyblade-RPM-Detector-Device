# BRD_BPX V1.0 韌體版本說明

此資料夾提供 BRD_BPX V1.0 的兩種量測模式韌體：

- `BRD_BPX_LOAD_V1.0.bin`
- `BRD_BPX_AUTO_V1.0.bin`

兩個版本使用相同的 BRD_BPX 基礎功能，主要差異為量測開始方式與發射完成判定方式。

## 版本差異

| 項目 | BRD_BPX_LOAD_V1.0 | BRD_BPX_AUTO_V1.0 |
|---|---|---|
| 量測模式 | LOAD | AUTO |
| 是否使用 LOAD IR | 使用 | 不使用 |
| 量測啟動 | LOAD 穩定 HIGH 100 ms 後清零並啟動 RPM 檢測 | 偵測到 RPM 後自動進入量測 |
| RPM 開始後 LOAD 功能 | 不參與發射判定 | 不使用 LOAD |
| 發射判定 | 即時 RPM 嚴格低於本次 MAX 的 20% | RPM 曾達到 2000 RPM，之後連續 1000 ms 無有效 RPM |
| AUTO RPM 門檻 | 不使用 | 2000 RPM |
| 有效歸零 | 依 LOAD 模式既有 RPM 流程 | RPM 小於 2000，之後 0 RPM 持續至少 250 ms |
| OLED READY 顯示 | `WAIT LOAD` / `LOADED READY` | `AUTO READY` |
| BLE loaded 狀態 | 依實體 LOAD 狀態 | RPM != 0 為 loaded，RPM == 0 為 unloaded |

## BRD_BPX_LOAD_V1.0

LOAD 版本適用於使用實體 LOAD IR 感測器確認陀螺已裝載的硬體。

量測流程：

1. 等待 LOAD 進入 HIGH。
2. LOAD 必須連續穩定 HIGH 至少 100 ms。
3. 確認裝載後，清除上一輪量測資料並啟動 RPM 檢測。
4. RPM 開始後，LOAD 狀態不再參與發射判定。
5. 持續更新即時 RPM 與本次 MAX。
6. 當即時 RPM 嚴格低於本次 MAX 的 20% 時，判定本次發射完成並封存結果。

### LOAD 模式重點

- LOAD 只負責確認裝載與啟動量測。
- RPM 開始後，即使 LOAD 狀態改變，也不以 LOAD 判定發射完成。
- 發射完成條件只依 RPM 與 MAX 比例判定。

## BRD_BPX_AUTO_V1.0

AUTO 版本不使用 LOAD 作為開始、發射或歸零條件，完全依 RPM 自動判定。

### AUTO 參數

```text
AUTO_RPM_THRESHOLD = 2000 RPM
AUTO_RESET_ZERO_MS = 250 ms
AUTO_LAUNCH_ZERO_MS = 1000 ms
```

量測流程：

1. 不等待 LOAD，裝置保持 `AUTO READY`。
2. 偵測到有效 RPM 後自動開始量測。
3. RPM 達到或超過 2000 RPM 時，建立有效發射候選。
4. 若 RPM 小於 2000，之後 0 RPM 持續至少 250 ms，執行有效歸零。
5. 若本次量測曾達到 2000 RPM，250 ms 歸零只將即時 RPM 清為 0，不清除發射候選。
6. 必須持續 0 RPM 至少 1000 ms，才正式判定為有效發射並封存結果。

### AUTO 模式重點

- 不使用 LOAD IR 作為量測條件。
- 2000 RPM 是有效發射候選的唯一 RPM 門檻。
- 250 ms 用於有效歸零。
- 1000 ms 連續 0 RPM 用於確認有效發射完成。
- 達到 2000 RPM 後，即使先觸發 250 ms 歸零，也會保留發射候選直到 1000 ms 判定完成。

## 韌體選擇

使用實體 LOAD IR 進行裝載判定：

`BRD_BPX_LOAD_V1.0.bin`

不使用 LOAD IR，希望由 RPM 自動開始與結束量測：

`BRD_BPX_AUTO_V1.0.bin`
