# BRD_BPX V1.2.2 — HOLD／LOAD 重新武裝修正

## 韌體基準

- 以 `BRD_BPX_V1.2.1_BLE_DisconnectFix_BatteryCurve_ChargeComp_NoFULL.zip` 原始碼為唯一修改基礎。
- 韌體版本由 `V1.2.1` 更新為 `V1.2.2`。
- 保留 V1.2 的 RPM ISR、轉速計算、MAX×30%（執行時由設定覆寫）與 2,500ms MAX HOLD。
- BLE 維持 V1.2.1 的 100ms Notify 失敗重試、命令佇列滿不主動斷線；沒有新增 30 秒電量 Notify。
- 電池維持 V1.2.1 的放電曲線、充電補償、低電保護；**沒有 FULL 偵測**。

## 問題追查

`brd_measurement_update()` 原本在 `g_max_lock` 到期時將 `g_load_stable = LOW`，但沒有證明 GPIO1 真的曾經 LOW。隨後 `restart_input_capture()` 直接讀取 GPIO1：若仍為 HIGH，原來的 HIGH 被誤認成新的裝載，`handle_load_change()` 進而呼叫 `reset_measurement(true)` 清掉 MAX，並允許同一裝載週期第二次計次。

真正 2.5 秒 `g_max_lock` 期間，LOAD ISR 入列已被停用，理論上不會由單純 GPIO1 彈跳中斷 HOLD；使用者看見 MAX 被立即歸零可能是在 HOLD 2.5 秒已到期、但 MAX 數字仍被保留顯示時。若確實在首次完整 MAX 畫面後未滿 2.5 秒就消失，還要檢查供電／ADC 錯誤／OTA／重啟／實際燒錄 BIN。

## 變更說明（K&R C++ 註解）

- `[修改] BRD_BPX/brd_measurement.cpp`
  - `[新增] g_load_release_required`：發射完成後開始要求真正卸載。
  - `[刪減] HOLD 到期時直接設定 g_load_stable = LOW`。
  - `[新增] INPUT_TIMER_LOAD_RELEASE`：從 HOLD 到期重新啟用擷取起，真正 GPIO LOW 經 50ms 去抖後解除「必須卸載」鎖。
  - `[修改] handle_load_change()`：當必須卸載時，所有 HIGH／LOW 狀態變化僅供去抖，不允許 reset_measurement(true)。
  - `[保留] g_max_lock=true 時完全不處理 LOAD/RPM；期滿後保留原 MAX，等真正 LOW 再 HIGH 50ms 才清除。
  - `[保留] AUTO 模式原本重新武裝方式，不套用 LOAD 的物理卸載限制。
- `[修改] BRD_BPX/brd_config.h`：版本更新到 V1.2.2；其餘設定常數不變。
- `[新增] BRD_BPX/tests/test_hold_guard.cpp`：真實量測模組 + 真實 BBP Session 測試。
- `[修改] BRD_BPX/tests/run_host_tests.py`：整合新增測試。
- `[修改] BRD_BPX/tests/test_runtime.cpp`：原本假設 HOLD 後可立刻 HIGH 的測試改為確認實體 LOW 50ms 後再 HIGH。

## 實際工作流程（LOAD 模式）

1. 裝載 LOW → HIGH，穩定 `LOAD_IR_DEBOUNCE_US`（預設 50ms），開始 RPM 量測。
2. 即時 RPM 嚴格低於 MAX×結束百分比後封存結果，停止 ISR 入列；HOLD 開始。
3. OLED 完整送出 MAX 畫面時重新計算 2,500ms HOLD；若 OLED 失敗，原本的計時備援依舊保留。
4. HOLD 期間 GPIO1 彈跳或 RPM 殘餘脈衝不會清除 MAX。
5. HOLD 到期後只重新讀取 GPIO1，**不假造 LOW**；即使一直 HIGH 也維持 MAX，不會建立第二筆。
6. 只有真實 GPIO1 LOW 穩定 50ms 後才允許下一輪裝載；若 LOW 從 HOLD 期間就開始，仍須在 HOLD 結束後維持 50ms。
7. 下一次實體 HIGH 穩定 50ms 才清除 MAX 並開始新量測。

## 回歸測試

執行方式（Linux）：`cd BRD_BPX && python3 tests/run_host_tests.py`。

- **PASS**：2.5 秒 HOLD 期間 LOAD 多次彈跳、RPM 殘餘脈衝，MAX 與歷史不歸零。
- **PASS**：2.5 秒到期後 LOAD 持續 HIGH，模擬完整高低速 RPM 脈衝，歷史仍只有一筆。
- **PASS**：LOW <50ms 彈跳後 HIGH，不會重新裝載。
- **PASS**：LOW 穩定 50ms 再 HIGH 穩定 50ms，才建立下一筆歷史。
- **PASS**：HOLD 結束當下 GPIO1 已是 LOW 且沒有新負緣，持續 LOW 50ms 仍可成功解鎖。
- **PASS**：原有 RPM、OLED framebuffer、NVS/OTA、BLE 六組重試與斷線、21 點電池 SOC、充電 72 秒/1% 測試。

測試使用 host stubs，非 ESP32-C3 目標編譯、真實 GPIO 波形測試或 BLE 無線測試。不得將 host PASS 視為已完成實機驗證。

## 檔案安裝位置

```text
BRD_BPX/
├── BRD_BPX.ino                   # 完整主程式；不修改
├── brd_measurement.cpp          # 唯一修改的量測核心檔
├── brd_config.h                 # 只有版本字串更新
├── brd_bbp.cpp / .h             # 不修改
├── brd_battery.cpp / .h         # 不修改
├── brd_oled.cpp / .h            # 不修改
├── tests/
│   ├── test_hold_guard.cpp      # 新增
│   ├── test_runtime.cpp         # 更新 LOAD/ HOLD 期望
│   └── run_host_tests.py        # 新增整合項
└── HOLD_FIX_V1.2.2.md
```

Arduino IDE 開啟 `BRD_BPX/BRD_BPX.ino`，完整原始碼同資料夾；需重新編譯 app-only `.bin` 後 OTA。這個 ZIP 不包含舊 BIN。
