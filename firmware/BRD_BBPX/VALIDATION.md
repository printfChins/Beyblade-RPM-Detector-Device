# BRD_BBPX LOAD / AUTO 雙模式驗證紀錄

日期：2026-09-25。

## 已執行

在專案根目錄執行：

```text
python3 tests/run_host_tests.py
```

結果：8 組宿主測試全部通過。

```text
PASS dual_edge
PASS conformance
PASS protocol
PASS session
PASS measurement
PASS measurement_auto
PASS transport
PASS subscriber
PASS 8 host test binaries
```

## AUTO 專用驗證

`tests/test_measurement_auto.cpp` 實際覆蓋：

- AUTO 模式不掛 LOAD interrupt。
- 開機 0 RPM 時 loaded=false。
- 1500 RPM（<2000）停止後，250 ms 前保持 RPM，滿 250 ms 歸零且不產生 Shot。
- 3000 RPM（>=2000）停止後，1000 ms 前不產生 Shot，滿 1000 ms 才成立有效發射。
- 有效發射後 loaded=false、HOLD/MAX 啟用、launch event 計數增加。
- BBPX History 代表值維持 3000 RPM；曲線仍使用既有 raw period 格式。
- HOLD 結束保留 MAX；下一次 RPM 啟動才清除 MAX。
- 下一次有效 RPM 非 0 時 loaded=true。
- AUTO READY 狀態可作為 storage safe period。

既有 LOAD、雙邊沿、協議、Session、傳輸及 subscriber 回歸同時全部通過。

## 編譯限制

目前工作環境沒有 Arduino CLI / Arduino-ESP32 完整編譯工具鏈，因此本次沒有產生新的 ESP32-C3 `.bin`，也沒有宣稱完成實板編譯或燒錄驗證。舊 R4 binary 已移除，避免誤認為包含本次修改。

尚未執行：實板 IR 訊號、實際 BLE 封包擷取、手機上位機/官方 App 互通驗證。
