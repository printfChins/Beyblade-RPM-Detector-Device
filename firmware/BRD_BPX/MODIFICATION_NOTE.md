# BRD_BBPX Measurement Modification Note

## 修改檔案
- `BRD_BBPX/brd_config.h`
- `BRD_BBPX/brd_measurement.cpp`
- `BRD_BBPX/tests/test_dual_edge.cpp`
- `BRD_BBPX/BRD_BBPX.ino`
- `BRD_BBPX/README.md`
- `BRD_BBPX/VALIDATION.md`

## 2026-10-01 正/負緣先觸發固定參考

1. RPM GPIO 由 `FALLING` 改回 `CHANGE`。
2. LOAD 模式仍需 LOAD HIGH 穩定 100 ms 才啟動新一輪量測。
3. 量測啟動後，第一個實際 RPM 邊沿決定本次固定參考極性。
4. 若第一個事件為正緣，只使用「正緣 -> 下一個正緣」的完整一圈週期計算 RPM。
5. 若第一個事件為負緣，只使用「負緣 -> 下一個負緣」的完整一圈週期計算 RPM。
6. 同一輪量測中參考極性不會切換；另一極性不更新 RPM、MAX，也不加入 BLE 曲線。
7. 參考極性選定後，ISR 只將相同極性的事件放入 RPM Queue，避免 `CHANGE` 模式長期造成兩倍 Queue 流量。
8. BLE 曲線仍保存最早 32 筆有效 RPM 週期，32 點之後 MAX 仍繼續更新。
9. LOAD 在 SPINNING 後不參與發射判定，也不能重置 RPM/MAX/曲線。
10. LOAD 模式發射完成條件仍為即時 RPM 嚴格小於本次 MAX 的 20%。
11. RPM Queue overflow 時保留本次已選參考極性，只清除週期基準；下一個相同極性邊沿重新建立基準，第二個相同極性邊沿恢復 RPM。

## 未改變
- LOAD 狀態切換仍需穩定 100 ms。
- BLE Profile 容量仍為 32 點。
- MAX 與 32 點曲線仍獨立。
- 300 ms 無參考極性邊沿時，即時 RPM 仍歸零。
- 發射成功條件仍為 `current_rpm < max_rpm * 20%`。
- 先前的 RPM / LOAD Queue overflow 防止中途重置修正仍保留。
