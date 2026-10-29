# RPM 優先修正獨立審查

獨立審查無Critical，兩項Important皆修正並以失敗到通過的測試驗證。

1. 同模式保存設定重建LOAD去抖，造成保存後首圈被忽略。修正為僅在模式真正變更時建立必要狀態；case11。
2. HOLD尾段事件大量溢位，以最後保留事件誤判安靜，導致同發重複計次。修正為以本輪快照涵蓋的ISR活動時間延後重新武裝；case12。

另補嚴格BBP模式 pending旗標於完成資料實際發布前保留；case13。

完整suite：11個host executable，13個RPM優先級場景，ASan/UBSan通過。LOAD/AUTO各120分鐘、各7200發。完整輸出見RPM_PRIORITY_TEST_RESULTS.txt。

未認證項目：ESP32目標編譯、實際IRAM map/cache-off執行、IRQ延遲及真正ISR併發。未提供實機根治承諾。

無延期Minor。採上傳專案的完整原始碼封包交付，沒有遠端分支合併或發布操作。
