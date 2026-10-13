# BRD BPX 執行時設定設計

依據本次對話與 BRD_BPX.zip，修改既有韌體，不新增 App。

- 四項可寫參數：LOAD/AUTO 模式、AUTO 最低 RPM、共同結束百分比、充電時自動關閉 BLE。
- config.h 保留預設值；BLE 修改的是 RAM 草稿，SAVE 才提出持久化請求。
- 量測中拒絕修改設定與清除紀錄；在 callback 與主 loop 處理前各檢查一次。不延後執行量測中收到的修改命令。保留 GET、既有查詢及 Notify，BLE callback 只做有界入列。
- 閒置或結果 HOLD 時，且電池與 ADC 正常，才暫停輸入捕捉、寫入 NVS、套用設定，再恢復下一次量測。
- 保存中的設定凍結，不接受第二次修改。斷線丟棄未 SAVE 的草稿；已接受 SAVE 的請求保留。
- Flash 失敗不套用設定；App 可讀取錯誤狀態並重試。相同設定不重複寫入，實際寫入至少間隔一秒。
- AUTO 保留最低 RPM 資格門檻；兩種模式皆在 current_rpm * 100 < max_rpm * finish_percent 時立即結束。
- AUTO 未達門檻的轉動不發布結果。達門檻後採共同 300 ms 無脈衝歸零備援，移除等待 1000 ms 的發射條件。
- 切換模式須更新 LOAD 中斷，保留已完成的 MAX 畫面與已凍結的 BLE 曲線。
- 充電 BLE 關閉只關閉無線服務，量測與本機歷史仍運作；拔除充電後恢復 BLE。開關預設開啟。
- 新增獨立 BRD 設定服務，保留原本 BBP service、名稱、廣播與 17-byte 曲線格式。
- OLED 開機頂部大字 BRD BPX，下方小字 FW VER V1.0；沿用來源專案 V1.0。
- 不提供舊 build/bin 作為本次更新韌體。此環境未安裝 ESP32 交叉編譯工具，交付原始碼與主機測試，不聲稱實機通過。

設定範圍：模式 0/1；AUTO 門檻 1..59999 RPM；結束百分比 1..100；充電開關 0/1。

API：GET=1、SET=2、SAVE=3、DEFAULTS=4、DISCARD=5。8-byte 請求與 20-byte 回覆，支援預設 ATT MTU 23。

NVS：獨立 namespace brd_cfg，單一 settings_v1 blob，固定小端序、schema 1 與 CRC32；保留 brd_bbp/history_v1。
