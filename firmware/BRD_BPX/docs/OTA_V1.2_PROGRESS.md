# OTA V1.2 執行紀錄

已批准方案: A，自訂 BLE OTA 與現有 GitHub 燒錄網站。

- Task 1: complete。先確認 HASH 失敗，實作 Session 後 sanitizer 測試通過。
- Task 2: complete。先確認 GATT create 失敗，實作後全部七組主機測試及主程式語法檢查通過。
- Task 3: in progress。網頁八項傳輸測試先 RED 後 GREEN；UI 整合待完成。
- 主機 LeakSanitizer 無法存取 /proc tracing 資料，沿用專案 ASAN_OPTIONS=detect_leaks=0；AddressSanitizer/UBSan 保留。
- ESP32-C3 Arduino 編譯工具鏈及實機不可用；不聲稱 MCU 編譯或實機驗證，也不保留附件 V1.1 BIN。

- Task 3: complete。十項 Node 協議測試與語法檢查通過；UI smoke 因缺少瀏覽器未執行，此限制保留。
- 獨立程式審查：無 Critical，四項 Important 已重現並修正，均有主機回歸驗證。
- 修正 GATT 取消重疊：新增 write/read in-flight 取消測試；不與未完成 GATT 操作重疊 ABORT。
- 修正歷史遺失：最後一筆 pending 或 dirty 時禁止 OTA 開始，600ms 發布與 NVS 成功保存後才放行。
- 修正驗證期斷線：VERIFY / esp_ota_end 邊界注入斷線重連，在原子 commit ticket 前拒絕分區提交。
- 修正 Flash 校驗：讀回完整 app 分區範圍比較 MD5，注入損壞/讀取失敗不切 boot。
- OLED 實際 framebuffer 檢查補齊 DONE 所需 N 字型，回歸測試通過。
- Deferred minors: none。

- GitHub create_branch 回傳 403 Resource not accessible by integration；未使用其他身分繞過權限，未建立分支/PR、未更新網站。
- Task 4: 完成交付檔與網站 Git patch。原始檔 Git blob SHA 與 GitHub main 一致，patch 可從原始樹乾淨套用並重建全部網站檔案。
