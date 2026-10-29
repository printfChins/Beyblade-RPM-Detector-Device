# BRD_BPX_PRO V1.4 — RPM 優先、完成後產生曲線

## 目標與優先順序

1. GPIO3 ISR 入列與完整單圈 RPM 計算為最高優先；維持正緣→正緣、負緣→負緣，不以半圈誤算。
2. 量測期間只把每次**有效**完整單圈原始資料寫到固定 RAM；不做 TFT 64 點限制、不以 50ms 跳過取樣、不做 BLE 32 點封包採樣。
3. 發射完成（既有 MAX 百分比判定）後，主迴圈才把這一發原始資料縮為 TFT 64 點和 BLE 32 點；曲線在完成前保持空白。
4. BLE 仍只傳「包含全程 MAX 的那個極性」的 32 筆完整單圈週期；曲線頭尾及真實 MAX 保留。
5. 對 ISR Queue 與原始資料 RAM 上限設置可檢查診斷，避免把原始事件缺損誤認成正常取樣。

## 主要流程

```text
GPIO3 CHANGE ISR（只入列時間與電位）
        ↓
brd_measurement_update()（先清 ISR Queue）
        ↓
正/負緣分別使用相鄰同極性邊緣計算完整一圈 RPM
        ↓
更新即時 SP、MAX、LOAD/AUTO 發射判定
        ↓
brd_shot::record(period_us, event_us, polarity, rpm)
  └─ 固定 8192 筆 RAM，每筆 8 bytes，不產生曲線、不呼叫 BLE
        ↓
發射完成 → brd_shot::finish(measured_max)
        ↓
主迴圈 brd_shot::publish_finished()（僅在完成時執行一次）
   ├─ 全部原始有效事件均等抽取最多 64 點 → TFT 曲線
   └─ 取最高 MAX 所在極性、最多 32 筆單圈 → BLE BBP
        ↓
既有 BLE / NVS 發布流程、TFT 動畫與畫面更新
```

## 定量限制

| 參數 | V1.4 設定 | 說明 |
|---|---:|---|
| `RPM_ISR_QUEUE_SIZE` | 256 | ISR 原始 GPIO 邊緣入列；滿載另有 overflow 診斷 |
| `SHOT_RAW_SAMPLE_CAPACITY` | 8192 筆 | 量測後、前處理之前存完整有效單圈樣本 |
| 每筆樣本 | 8 bytes | `period_us`(4)、`rpm`(2)、相對時間/極性(2) |
| 原始樣本固定記憶體 | 65,536 bytes | 靜態 BSS，無動態配置，不占堆疊 |
| 單次時間偏移 | 最多 32.767 秒 | 以 1ms 相對時間 + 1-bit 極性保存；原始完整週期仍為 us |
| 30,000 RPM | 約 1000 筆/秒 | 兩極性共 1000 筆/s；8192 筆約可保存 8.192 秒 |
| TFT 曲線 | 最多 64 點 | 只有完成後繪製；強制包含 MAX |
| BLE 曲線 | 最多 32 點 | 選含 MAX 的同極性單圈曲線；封包格式維持 |
| `RPM_ZERO_TIMEOUT_MS` | 不變 | 仍依 V1.3 量測判定規則 |

注意：8192 是「完整原始樣本」的上限，不是無限筆。若量測過長或超過 32.767 秒，`brd_shot::diagnostics()` 的 `truncated` 和 `dropped` 會顯示截斷。MAX 仍以量測模組計算為準，不因樣本滿載而影響數值；但**截斷時曲線無法代表後段完整資料**，後續需要實機觀察該診斷。

## 檔案異動與放置位置

全部位於 Arduino 專案資料夾 `BRD_BPX_PRO/`：

| 檔案 | 修改內容 |
|---|---|
| `brd_shot_capture.h` | [新增] 原始測速資料介面及診斷結構 |
| `brd_shot_capture.cpp` | [新增] 所有有效同極性單圈原始值保存、完成後縮點、MAX 保留 |
| `brd_measurement.cpp` | [刪減] 量測期間即時輸出 BLE/TFT 曲線；[新增] 原始資料入列與結束標記、ISR backlog 讀取 |
| `brd_measurement.h` | [新增] `brd_measurement_rpm_backlog()` |
| `brd_ui_curve.cpp/.h` | [刪減] 固定 50ms 實時取樣；[新增] 完成後一次發布 64 點曲線 |
| `brd_bbp_session.cpp/.h` | [新增] 代表 SP 可使用量測真實 MAX |
| `brd_bbp.cpp/.h` | [新增] 量測真實 MAX 的傳入介面 |
| `brd_config.h` | [修改] 版本 V1.4、原始樣本容量及 ISR Queue 256 |
| `BRD_BPX_PRO.ino` | [新增] include、發射後處理呼叫及 Queue 高水位時 BLE/TFT 讓路；其餘流程保留 |
| `tests/test_shot_capture.cpp` | [新增] 3000 筆高速原始記錄、8,192 上限、MAX/雙極性縮點與完成後產出測試 |
| `tests/test_runtime.cpp` 等 | [修改] 既有整合測試以 V1.4 發射後發布時序驗證 |

## 不修改的部分

GPIO 腳位、IR ADC 裝載門檻 4000/100ms、固定同極性完整一圈計算公式、LOAD/AUTO 模式、百分比結束判定、MAX 主畫面、TORQUE W1/L2 動畫、HISTORY 8 筆、OTA 程式、BLE 17-byte / 12-page、NVS 儲存映射均不更動。

## 驗證與待確認

- [通過] 主機端 g++ 回歸測試，包含高轉速連續 3,000 筆、緩衝溢位、後段 MAX 和 64/32 點輸出。
- [通過] 既有 GPIO、ADC、LOAD/AUTO、BLE/OTA、TORQUE UI、動畫與燒錄檔以外的 host 語法檢查。
- [未執行] Arduino ESP32-C3 交叉編譯、ESP32-C3-MINI-1-N4 RAM 實體餘裕檢查、實機 30,000 RPM 持續量測及示波器確認。
