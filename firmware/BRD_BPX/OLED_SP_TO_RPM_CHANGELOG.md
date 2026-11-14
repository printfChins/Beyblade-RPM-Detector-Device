# OLED SP → RPM 修改紀錄

- 修改檔案：`BRD_BPX/brd_oled.cpp`
- 修改位置：`oled_begin_frame()` 主畫面、非 OTA 的數值顯示分支。
- [刪減] 即時轉速標籤 `SP`。
- [新增] 即時轉速標籤 `RPM`。
- [保留] `MAX` 標籤、`display.value` 來源、原有字庫、字體倍率、I2C 傳輸、電池及 BLE 圖示、RPM/LOAD/AUTO 計算。
- 範例：原 `SP 15000` → `RPM 15000`，`MAX 20000` 不變。
- 驗證：`g++ -fsyntax-only brd_oled.cpp` 通過；`python3 tests/run_host_tests.py` 全部通過。
- 本 ZIP 僅包含原始碼、設定、文件與測試，不包含原 ZIP 內已編譯但**尚未套用修改**的 `*.bin` 與 `build/` 暫存產物。請重新編譯並燒錄新版韌體才能看到顯示調整。
