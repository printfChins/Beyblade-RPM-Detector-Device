# BRD BPX PRO V1.2 — 充電 UI 修改紀錄（2026-10-22）

## 設計

- [刪除] 舊有電池左側閃電與備援 UI 右側閃電。
- [新增] 電池邊框採風格主題色，內部保持實際電量比例，亮帶只在已填滿區域移動。
- [新增] 電池圖示下方三點循環提示，即使電量低也可以辨識正在充電。
- [新增] MAIN 頂部右側保留百分比，並顯示 CHG 字樣。
- [修改] IR TEST 保留電壓數值、感測圖形與原有位置，電池使用相同動畫。
- [保留] 未充電時保留原有電池樣式，停止充電時立即恢復正常顯示。

## 韌體結構

- `brd_config.h`：`BRD_UI_CHARGE_ANIM_INTERVAL_MS` 設為 240 ms。
- `brd_ui_types.h`：在 `Model` 加入 `charge_phase`。
- `brd_tft.cpp`：主迴圈讀取 GPIO10 充電狀態並產生相位；無延遲、無新 ISR。
- `brd_ui_motion.cpp`：只在 MAIN / IR TEST 且充電相位改變時通知重繪。
- `brd_ui_style_scene.cpp`：三風格同時更改主要畫面、IR TEST 電池元件。
- `brd_ui_widgets.cpp/.h`：備援 UI 同樣改成三點動畫，不再繪製閃電。
- `tests/test_charge_ui.cpp`：主機端像素級回歸測試。

## 驗證界限

- 使用 160 × 80 RGB565 C++ 場景實際繪圖程式進行主機端測試與預覽。
- 字型替身與真正的 U8g2 字型略有差異。
- 尚未透過 Arduino IDE 編譯 ESP32-C3 韌體，亦未完成 TFT 實機確認。


## 2026-10-22 第二次調整
- [修改] 充電提示改為電池右側閃爍閃電。
- [刪除] 電池下方三點特效。
- [保留] 電池內部亮帶掃描充電效果。

- [修改] 閃電顏色改為各風格高亮混白主題色，提升辨識度。


## 最新變更：移除所有充電閃電 (2026-10-22)
- [刪除] `brd_ui_style_scene.cpp` 的 `charge_bolt()` 及 MAIN / IR TEST 呼叫。
- [刪除] `brd_ui_widgets.cpp` 的備援 UI 閃電與獨立 Dirty 區塊。
- [保留] 電池內部依 `charge_phase` 掃描的亮帶動畫。
- [保留] 電量百分比、IR TEST 電壓、BLE 圖示、GPIO10 充電判斷。
- [保留] 原設定 16 MHz SPI 與 33 ms 主要更新排程。
