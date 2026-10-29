# BRD BPX PRO V1.2 — 裝載文字修正（2026-10-22）

- [恢復] 風格 1（DIAL，索引 1）：保留左下 SP 並恢復右下 WAIT LOAD（W1 呼吸）及 LOADED（L2 聚焦鎖定）。
- [刪除] 風格 2（TORQUE，索引 2）：刪除左側 SP 下方 `LOAD` 字樣；MAX 狀態 `PEAK` 不變。
- [保留] NEBULA（索引 0）的 W1/L2 和所有其他原有畫面。
- [保留] WAIT LOAD 文字水平置中、不顯示尾端光點；LOADED 左右括號不得碰到文字。
- [保留] 量測、BLE、OTA、電池、16 MHz SPI、33 ms UI 更新排程等設定不動。

程式：`BRD_BPX_PRO/brd_ui_style_scene.cpp`
檢查：`BRD_BPX_PRO/tests/test_w1_l2_layout.cpp`
