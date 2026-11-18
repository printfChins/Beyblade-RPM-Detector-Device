# BRD_BPX — Falling Edge 即時 RPM 與原始週期優先量測

日期：2026-11-19。以使用者提供的 `BRD_BPX(20261118-184548).zip` 為修改基準；未擅自變更原韌體版本字串、BLE UUID、BBP 17-byte 格式、電源與 OLED 硬體設定。

## 量測規則

1. **LOAD 模式**：GPIO1 完成原有裝載去抖（預設 50ms）後，才啟用 GPIO3 `FALLING` 中斷。裝載前 GPIO3 RPM ISR 不啟用；第一個下降緣只建立時間基準。開始轉動後 LOAD ISR 會停用，減少負擔。**AUTO 模式**不依賴裝載，始終在待測狀態監聽下降緣。
2. **每次下降緣**：ISR 僅將 `micros()` 時戳推入 128 筆固定環形佇列；不做除法、排序、BLE、malloc 或 Flash 寫入。主迴圈先批次處理 GPIO 事件，使用兩筆相鄰下降緣 `Δt = current_us - previous_us`，立即計算 `RPM = 60,000,000 / Δt / PULSES_PER_REV`。第一筆基準沒有完整一圈，因此不會產生轉速。
3. **原始儲存**：每筆 `Δt > 0` 的週期不做中位數、平均、平滑、合理性比對，直接存入固定 RAM 並計算 RPM，持續更新 MAX；只有零週期做除零保護。因現有 `uint16_t` 畫面／BBP API 上限，顯示 RPM 限制為 60,000；原始週期仍以 `uint32_t` 保留。
4. **停止條件**：LOAD 使用 `current_rpm * 100 <= MAX * finish_percent`；AUTO 使用 `<`，且需先達到設定的 AUTO 最低有效轉速。一筆達標即立即結束，不再等待第二筆低值。保留 300ms 無下降緣的停止備援；AUTO 尚未達最低有效門檻時不會封存發射。
5. **停止後處理**：先禁止 ISR 並鎖定 MAX，才由 RAM 按順序擷取量測起始的前 32 個完整旋轉週期，每圈一點、不均勻抽點、不插入後期 MAX，並獨立保存完整發射 MAX（V1.5.1 曲線修訂）。**所有 BBP Notify、設定回覆、OTA 排程與 NVS 更新皆在量測結束後恢復**。BLE Link 可維持連線，底層 BLE 協定交握並未關閉。
6. **HOLD**：沿用原有 OLED MAX 停留與 LOAD 重新裝載條件；HOLD 結束後才回待測並恢復適當中斷。

## 實際容量與限制

- RPM ISR Queue：`RPM_ISR_QUEUE_SIZE = 128`，有效可排 127 筆。主迴圈被遮斷太久可能 Queue overflow；會記錄 `rpm_queue_overflows`，重建週期基準，不以跨缺口時間偽造 RPM。
- 原始週期 RAM：`RPM_RAW_CAPTURE_CAPACITY = 16384`，每筆 4 bytes，**合計 65,536 bytes**。例：持續 30,000 RPM 可存約 32.8 秒；這是單次保留上限，不代表測量會在滿額時終止。
- 若原始 RAM 填滿：新增 `raw_capture_overflows` 診斷累加；後續仍每圈計算即時 RPM/MAX，但**超額的原始週期無法保存**。MAX 仍獨立更新。BBP 曲線因此只能使用保留下來的前段原始週期。
- BBP 官方封包仍限制每條曲線 32 點；32 點是**停止後由原始資料取前 32 圈的輸出**，不是採樣期間只記錄 32 筆。此改動尚未新增 BLE 傳輸完整 16,384 筆原始週期的自訂 API，也未把原始週期逐圈寫入 NVS，避免 Flash 壽命與採樣延遲問題。
- **無濾波是刻意要求**：光學反射、比較器毛刺、接點或干擾產生的額外下降緣會直接產生 RPM 峰值，甚至讓停止判定提前成立。要從硬體實測確認波形品質；本版不嘗試軟體修正這些事件。
- 主迴圈仍保留電池低電保護與 OLED 顯示；NimBLE 底層中斷／連線維持仍可能執行，不等於完全關閉 RF 系統。

## 檔案變更（完整路徑皆位於 `BRD_BPX/`）

| 檔案 | 變更 |
|---|---|
| `brd_config.h` | [修改] FALLING、RAW RAM 容量與 32 點規格、立即發布，不再設最短／最長 RPM 週期濾波 |
| `brd_measurement.cpp` | [刪減] 雙極性與 5 點中位數。[新增] Falling 時戳 Queue、直接 RPM/MAX、全量 RAM、停止後曲線與中斷門控 |
| `brd_measurement.h` | [新增] `brd_measurement_capture_active()` 與原始取樣容量診斷 |
| `BRD_BPX.ino` | [修改] 主 loop 在採樣期間略過 BLE/OTA/設定與 NVS 排程；不更動 setup 或其它硬體腳位 |
| `brd_bbp.cpp` | [修改] 採樣期間阻止應用層傳送，停止後直接建立 BBP 發布事件 |
| `brd_settings_ble.cpp` | [修改] 設定通知延後至採樣結束 |
| `brd_bbp_session.cpp/.h` | [修改] 明確註記曲線輸入只來自停止後整理的原始週期 |
| `tests/test_falling_raw.cpp` | [新增] 真實量測模組主機測試 |
| `tests/test_measurement_fixes.cpp` | [修改] 模擬真實 Falling-only 與 BLE 延後傳輸 |
| `tests/run_host_tests.py`、`tests/run_fix_regressions.py`、相關測試替身 | [修改] 配合新的無濾波／立即停止行為 |
| `README.md`、`CHANGELOG.md`、`SETTINGS_API.md` | [修改] 更新量測及 BLE 時序文件 |

## 編譯與主機驗證

Arduino IDE 開啟 `BRD_BPX/BRD_BPX.ino`，同層完整保留全部原本 `.cpp/.h`、`partitions.csv` 和資料夾。若使用 `tests/stubs` 只代表主機替身，**不是實機燒錄版本**。

主機測試：在 `BRD_BPX/` 下執行 `python3 tests/run_host_tests.py`。驗證 GPIO Falling、LOAD/AUTO、立即 MAX、第一個低轉速即停止、停止後 BBP、前 32 圈順序、曲線不插入後期 MAX、RAM 上限、Queue overflow、`micros()` 回繞及現有 BLE/NVS/OTA 功能。主機測試無法取代 ESP32-C3 實機波形、Arduino IDE 編譯及電流／中斷負載量測。
