## V1.4 — 2026-10-29（RPM 優先 / 完成後曲線）

- [新增] `brd_shot_capture.h/.cpp`：每筆有效同極性完整單圈 RPM 全程寫入固定 8192 筆原始資料；不在量測中以時間間隔丟棄點。
- [修改] 發射結束後才由非 ISR 主迴圈整理 TFT 64 點 / BLE 32 點，保留全程 MAX；TFT 量測期間不繪曲線。
- [修改] BLE 只取含真實 MAX 的正/負緣單一極性完整單圈，且抽樣覆蓋整次發射；封包／Flash 格式不變。
- [修改] ISR Queue 由 128 擴至 256；主迴圈積壓 1/4 以上時暫緩 BLE/TFT，優先處理 RPM。
- [新增] 有界樣本緩衝滿載 `truncated/dropped` 診斷；30k RPM 雙緣約 8.2 秒容量。
- [保留] 原 RPM 計算、MAX 門檻、ADC 裝載、TORQUE UI、動畫、OTA 及 GPIO 不改。
- [測試] `tests/test_shot_capture.cpp` 及原有主機端回歸測試通過，尚未實機編譯/燒錄。

## 2026-10-29 — V1.3 TORQUE V3 / 雙極性單圈採樣 / UI

- [修改] GPIO1 LOAD ADC：12-bit raw `< 4000` 為已裝載，`>= 4000` 為未裝載；20ms 取樣、100ms 去抖與失效保護保留。
- [修改] GPIO3 CHANGE ISR：正→正及負→負各計算完整一圈 RPM；不以半圈時間直接換算轉速。
- [修改] BLE：同時保留兩組單圈週期（每組前 32 點），結束時比較兩組全程最高 RPM，只封存最高值所在組；BBP 封包/NVS 格式不變。
- [修改] TFT：64 點曲線，每一點繪製圓點並連線；主頁 RPM 字體 x3 並左對齊 x60；MAX 移除左下 PEAK；20,000 RPM 16 段指示條。
- [修改] WAIT LOAD／LOADED 保留原右側 W1／L2 動畫；HISTORY 最近 8 筆左右 4+4，無進度條，歷史最高值黃色強調。
- [修改] BOOT 原開機等待內以 40ms/幀播放底部 8 格 0→滿動畫；OTA 標籤由 x6 移至 x1。
- [保留] RPM ISR 的輕量化與時間排序、AUTO/LOAD 原發射判斷、BLE/OTA、25FPS 目標、ST7735S SPI 16 MHz。
- [測試] 主機端測試含雙極性單圈、BLE max 選擇、RAW ADC 邊界、開機動畫、UI 與 OTA；**尚未做 ESP32-C3 真機 Arduino 編譯／燒錄**。

## 2026-10-29 — TORQUE 單風格精簡

- [刪除] 風格一 NEBULA 與風格二 DIAL 繪圖分支、警告版型和過時預覽。
- [保留] 風格三 TORQUE 畫面設計與 UI 座標，7 類畫面逐畫素比對原始版本一致。
- [修改] 主題數量 3→1、索引 2→0，並維持 TORQUE 專屬的非阻塞動畫排程。
- [保留] RPM/LOAD/ADC、BLE、OTA、NVS、LCD 驅動與主程式量測流程。

## 2026-10-22 — 三風格 UI 動畫目標調整為 25 FPS

- [修改] `brd_config.h`：MAIN、GRAPH、HISTORY、IR TEST 更新間隔統一 40 ms（目標 25 FPS）。
- [修改] GPIO5 四頁滑動步進由 50 ms 改為 40 ms，仍維持五步動畫。
- [保留] TFT SPI 16 MHz、外層輪詢 10 ms、充電流光 240 ms、低電量閃爍 500 ms。
- [保留] 風格 0 NEBULA、1 DIAL、2 TORQUE；W1、L2、裝載狀態、量測、BLE、OTA 與 GPIO 不變。
- [測試] 改用 `tests/test_25fps_schedule.cpp` 檢查動畫排程與 40 ms 幀間隔。

## 2026-10-22 — Restore DIAL status and remove TORQUE LOAD label

- [恢復] 風格索引 1 DIAL 的 W1 WAIT LOAD 和 L2 LOADED，放在右下角獨立狀態區。
- [刪除] 風格索引 2 TORQUE 左側主 SP 下方的 LOAD 文字；MAX 的 PEAK 保留。
- [保留] 索引 0 NEBULA 的狀態、三風格共用 W1/L2 對齊，以及其他 UI／量測／通訊設定。
- [測試] `tests/test_w1_l2_layout.cpp`、`tests/test_ui_ble_icon.cpp`。

## 2026-10-22 — [刪除] 第四風格 ORBIT，保留索引 0～2

- [刪除] ORBIT SIGNAL BEACON 畫面、動畫、狀態橋接、獨立頁面及專用測試／預覽。
- [修改] `BRD_UI_MOTION_THEME` 恢復 `2U`（TORQUE）；可用索引僅有 `0 NEBULA`、`1 DIAL`、`2 TORQUE`。
- [修改] `brd_ui_motion.h` 的 `THEME_COUNT` 從 4 回到 3，移除 ORBIT 專用的刷新、畫面路由邏輯。
- [保留] W1 WAIT LOAD 呼吸、L2 LOADED 鎖定、單一 SP、DIAL 無 TACHO、電池充電流光。
- [保留] 16 MHz SPI、33 ms UI 更新、量測、BLE、OTA 與 GPIO 原有功能。

## 2026-10-22 — DIAL 風格 2 移除 TACHO

- [刪除] `brd_ui_style_scene.cpp` 的 DIAL（風格 2）一般主畫面左上角 `TACHO`。
- [保留] DIAL 在顯示 MAX 時的 `MAX SP`，NEBULA / TORQUE 構圖不修改。
- [保留] W1／L2 特效、單一 SP、電池內流光、BLE／OTA／量測流程與 SPI／UI 更新排程。

## 2026-10-22 - MAIN 單一 SP 標示調整

- [刪除] `brd_ui_style_scene.cpp`：NEBULA、DIAL、TORQUE 轉速數字下方的多餘 `SP`。
- [保留] W1 呼吸、L2 聚焦鎖定、主要 SP、轉速數字、BLE / OTA / 充電電池流光。
- [保留] SPI 16 MHz、UI 33 ms；不變更量測與通訊。

## V1.2 — W1 呼吸 / L2 聚焦鎖定、SP UI（2026-10-22）
- [新增] WAIT LOAD W1 呼吸特效，三風格都在各自獨立狀態區呈現。
- [新增] LOADED L2 聚焦鎖定：8 幀內由外向內，括號與文字保持至少 5px 的空白。
- [修改] 主畫面、IR TEST 與備援頁面可見的 RPM 單位統一為 SP，不更動內部轉速量測單位。
- [修改] `brd_ui_motion.cpp`：裝載狀態改變時重置特效幀序，WAIT LOAD 低干擾持續呼吸。
- [保留] 16 MHz SPI、33ms 主畫面排程、BLE、OTA、電池流光、量測和 GPIO5 四頁切換。
- [驗證] 主機回歸通過，另新增三風格 W1/L2 安全區畫素測試；未進行 ESP32-C3 實機驗證。

## V1.2 — U8g2 現成字形替換（2026-10-22）
- [刪減] 移除先前自製的 7x11 / 6x10 glyph/bitmap 點陣表。
- [新增] Arduino 相依函式庫 U8g2_for_Adafruit_GFX；選用官方 7x13 / 6x10 字型。
- [修改] 狹窄欄位依 `getUTF8Width()` 量測；小字 baseline 交由字型庫決定。
- [保留] NEBULA / DIAL / TORQUE、SPI 8 MHz、20 FPS 目標與測量/OTA/BLE。
- [限制] 尚未在 Arduino ESP32-C3 Core 3.3.11 真實編譯、驗證字型逐像素顯示。

## V1.2 - 3 styles permanent rings / 20000 RPM indicator scale

- [修改] 01 NEBULA 內/外底環不再隨裝載狀態消失，保留青藍外圈和黃色內圈。
- [修改] LOADED 黃色特效旋轉；LIVE 青藍指示弧隨峰值延伸；MAX 保留滿格。
- [新增] `BRD_UI_INDICATOR_FULL_SCALE_RPM` 由 CFG 定義，預設 20000 RPM 滿格；三風格共用。
- [保留] SPI 8 MHz、20 FPS 排程目標、BLE/OTA/RPM 與四頁導航。
- [新增] `tests/test_ui_indicator_full_scale.cpp` 三風格 0/50/100/飽和測試。

## V1.2 UI Peak Hold / NEBULA 三階段雙環（2026-10-22）
- [修改] 所有三種風格的 LIVE 主畫面顯示本次實測峰值，只升不降，最後等於 MAX。
- [保留] BLE 與 SP GRAPH 仍使用未修改的即時 RPM，確保下降資料正確。
- [修改] 01 NEBULA 未裝載雙圈熄滅；裝載後黃圈旋轉；LOAD 解除後藍圈依峰值增長；MAX 停止黃圈且保留藍圈。
- [新增] NEBULA 三階段動畫與峰值回歸測試、160x80 C++ 影格預覽。
- [保留] 02 DIAL、06 TORQUE 圖形、GPIO5 四頁切換、OTA、BLE、Dirty Tile。
- [注意] 未於 ESP32-C3 Arduino 3.3.11 實機編譯，版本號維持 V1.2。

## V1.2 — MAX On Main UI（2026-10-21）

- [修改] `show_max` 狀態維持 MAIN，不跳轉獨立 MAX 頁面。
- [修改] 九種主題內顯示 `MAX SP` 與量測峰值；配合 Dirty Tile 及非阻塞短暫強調動畫。
- [新增] 九風格主畫面 LIVE/MAX 像素回歸測試與 C++ 原生預覽。
- [保留] 韌體對外版本 V1.2，GPIO5 四頁、BLE、OTA 及低電 ADC 設定不變。

## BRD BPX PRO V1.2 — BLE Icon / No Bottom Status

- [刪除] `brd_ui_style_scene.cpp`: 主頁右下角 LOADED/WAIT LOAD/HOLD/SAVED/READY 文字與原 BLE 圓點。
- [新增] `brd_ui_style_scene.cpp`: 9 主題 Bluetooth Rune 三狀態顯示；不更動電池圖示。
- [修改] `brd_ui_motion.cpp`: 將 `ble_disabled` 納入場景狀態簽章，確保即時局部更新。
- [新增] `tests/test_ui_ble_icon.cpp` / `tests/render_ble_9.cpp` / `docs/UI_BLE_NoBottomStatus/`。
- [驗證] 12 組回歸測試及 4 組 UI 測試通過；非 ESP32-C3 實機編譯。

---

## V1.2 UI style reduction (2026-10-21)

- [刪除] 11 種未保留主題（04 HYPER、05 SCOPE、08 ZEN、09 MATRIX、10 SPECTRUM、12 HORIZON、14 NOVA、15 CIRCUIT、16 PULSE、19 ION、20 STRIKE）。
- [保留] 9 個舊編號 01/02/03/06/07/11/13/17/18，主畫面與七頁像素資料保持一致。
- [修改] Theme 顏色表、主場景 Switch、警告頁主題圖樣、風格數量、測試及預覽。
- [不變] 裝置量測、GPIO5 四頁短按、ADC、BLE、OTA、TFT SPI 與 Dirty Tile。

## 2026-10-21 — GPIO5 四頁短按切換（V1.2）

- [修改] GPIO5 `INPUT_PULLUP` 短按放開後切頁：Main/Max → Graph → History → IR TEST → Main/Max。
- [修改] `brd_ui_input.cpp/.h`：40ms 去抖、800ms 短按上限、長按與系統頁按鍵抑制。
- [新增] `brd_ui_navigation.h`：UI 本地四頁及 BLE、OTA、低電、ADC 異常頁面優先權。
- [修改] `brd_ui_core.cpp`：使用獨立路由介面。
- [修改] `brd_tft.cpp`：本地 IR TEST 第一次切入同步取得 GPIO1/GPIO4 ADC 原始讀值。
- [修改] `brd_config.h` 與 `brd_ui_types.h`：四頁和短按參數。
- [新增] `tests/test_ui_navigation.cpp` 與 GPIO5 四頁測試；主機端回歸 12+3 組通過。
- [保留] ST7735 顯示修正版、20 主題、8x16 Dirty Tile、BLE、OTA、RPM IRQ 與電池警告。
- [限制] 尚未使用 Arduino ESP32 Core 3.3.11 在實機編譯及按鍵驗證。

## V1.2 Arduino ESP32 Core 3.3.11 PI 巨集相容性修正

- [修正] `brd_ui_warning_pages.cpp`：局部 `constexpr float PI` 改為 `kBrdPi`，同步替換所有角度換算參照。
- [修正] `brd_ui_style_scene.cpp`：局部 `constexpr double PI` 改為 `kBrdPi`，同步替換所有角度換算參照。
- [新增] `tests/test_arduino_pi_compat.py`：以 Arduino `PI` 巨集預處理器定義執行主機端編譯檢查。
- 不更動 LCD 驅動、20 主題、GPIO、BLE/OTA、曲線和 Dirty Tile；韌體版本維持 V1.2。
- 尚未使用 ESP32-C3 官方工具鏈執行完整交叉編譯及實機驗證。

## V1.2 — LOW BATTERY 最後一格紅色閃爍（2026-10-20）

- [刪除] 低電頁電池內驚嘆號、外部小狀態燈。
- [新增] 20 套風格最後一格紅色電量，以 500 ms 亮、500 ms 滅持續循環。
- [修正] 解除原本警告動畫 4 秒後停止的限制（只修改 LOW BATTERY；ADC FAULT 保持原設定）。
- [保留] 8×16 Dirty Tile、BLE/OTA/RPM/ADC 與各主題原本布局。
- [測試] 新增 12 秒 500 ms 亮滅及局部 SPI 更新驗證。

## 2026-10-20 — Warning Themes
- [新增] `brd_ui_warning_pages.cpp/.h`，低電池及 ADC FAULT 依 20 組主題獨立設計。
- [修改] `brd_ui_style_scene.cpp` 轉交新警告模組；移除舊共用版型。
- [保留] 運作邏輯、GPIO、BLE、OTA、Dirty Tile 傳輸、低電與 ADC 偵測門檻。
- [新增] 完整警告畫面 GIF／PNG 與測試。

## 2026-10-20 低電池警告畫面優化（維持 V1.2）

- [修改] 20 套 LowBattery 畫面改為大型空電池＋低電量短條＋驚嘆號＋CHARGE NOW。
- [保留] AdcFault、其他頁面、量測／BLE／OTA、Dirty Tile 框架不變。
- [新增] tests/test_style_20.cpp 圖示驗證、20 套預覽 GIF 產生工具及說明文件。

## BRD BPX PRO - IR TEST ADC UI integration (2026-10-19)

- [新增] GPIO4/ADC1_CH4 RPM 原始 12-bit ADC 取樣模組 (`brd_rpm_adc.cpp/.h`)，供 IR TEST 顯示使用，不影響 GPIO3 RPM 脈衝中斷。
- [修改] GPIO0 電池、GPIO1 LOAD、GPIO4 RPM 共用單一 ADC1 oneshot unit；GPIO1 額外提供原始 raw getter。
- [修改] IR TEST: 右上角電池圖示+電壓、中央 IR 白色有效圖、左 LOAD/右 RPM 直立 ADC 條及 0~4095 數字；Dirty Region。
- [同步預覽版] 全部頁面移除 BTN 提示與 MODE 顯示，主頁 WAIT LOAD/LOADED/HOLD 使用 1x 字級。
- [新增] RPM ADC 與 IR TEST 左右條狀圖局部重繪 host 測試。
- [保留] AUTO/LOAD, BLE, OTA, GPIO5 按鍵切頁以及原有 32 點 BLE 曲線。
- [驗證] 15 組主機端測試通過；尚未 Arduino IDE/ESP32 實機編譯與 LCD 硬體驗證。

## 2026-10-19 — TFT 橫向 UI Layout 與滿版轉速曲線

- [修改] `brd_ui_page_main.cpp`：上方移除 BPX PRO，換成 WAIT LOAD／LOADED／READY／HOLD／SAVED。
- [修改] `brd_ui_widgets.cpp/.h`：BLE WAIT 文字改成藍牙圖示，位於電池左側；充電閃電移到電池右側，三者獨立 Dirty 更新。
- [修改] `brd_ui_page_main.cpp`：移除右側 STATE/MODE 欄，SP 與數值橫向占滿內容區；5 位數使用 GFX scale 4。
- [修改] `brd_ui_widgets.cpp`：MODE 移到最底部，按鍵提示移到右下角。
- [修改] `brd_ui_page_graph.cpp`：圖表擴至 160 px 幅寬，完成曲線依實際點數延伸至最右方。
- [新增] `tests/test_ui_layout.cpp`，與新版版面 PNG；檔案說明 `docs/UI_MAIN_GRAPH_LAYOUT.md`。
- [保留] Adafruit ST7735/GFX、Dirty Region、LOAD ADC、BLE、OTA、GPIO5 按鍵與 V1.2 版本。
- [限制] 13 組主機測試通過，尚未進行實際 Arduino-ESP32 編譯與實機驗證。

## 2026-10-19 — BPX PRO 模組化 UI 與 GPIO5 曲線頁

- [新增] `brd_ui_input.cpp/.h`: INPUT_PULLUP 鍵值短按事件、40 ms 消抖、放開重上鎖。
- [新增] `brd_ui_curve.cpp/.h`: 獨立 UI 最多 50 點 100 ms RAM 曲線，完成時凍結。
- [新增] `brd_ui_page_graph.cpp`: SP GRAPH 頁面，Dirty Line Segments／局部縮放。
- [修改] `brd_ui_page_main.cpp`: BPX PRO、SP 文字放大，右側 STATUS 保留，底部 BTN:GRAPH。
- [修改] `brd_ui_core.cpp/.h`, `brd_ui_types.h`, `brd_ui_pages.h`: 新增 Graph 路由與切換。
- [修改] `brd_tft.cpp`: 收集 UI 曲線、按鍵導覽、保留 MAX 呈現 ACK。
- [修改] `brd_config.h`: UI_BUTTON_DEBOUNCE_MS、UI_CURVE_SAMPLE_MS、UI_CURVE_POINTS。
- [修改] `brd_ui_widgets.cpp/.h`: 新增 Main 頁底部切頁提示。
- [新增] 專用 UI 按鍵／曲線單元測試及 Graph Dirty 區域回歸測試。
- [保留] 主程式、LOAD ADC、BLE BBP、OTA、IR TEST、Flash 歷史資料路徑。

## BPX PRO LOAD ADC GPIO1 (2026-10-19)
- [新增] `brd_adc_shared.cpp/.h`：電池 GPIO0 與 LOAD GPIO1 共享單一 ADC1 oneshot unit，分別配置通道、校正。
- [新增] `brd_load_adc.cpp/.h`：20ms 類比取樣、1.60/1.95V 遲滯、故障超時設為未裝載。
- [刪減] `brd_measurement.cpp`：移除 GPIO1 數位 ISR、舊 LOAD IRQ 佇列及溢位處理，改為 ADC 狀態事件。
- [修改] `brd_config.h`：LOAD ADC 設定與穩定 100ms 去抖。
- [修改] `brd_io.cpp`：IR2 使用相同 ADC 狀態，不再讀 GPIO1 數位電位。
- [修改] `brd_battery.cpp`：ADC1 初始化轉交共用 owner，保留電池電壓/電量演算法。
- [新增] `tests/test_load_adc.cpp`、`tests/test_adc_shared.cpp`：遲滯及 ADC1 資源共享測試。
- [保留] BPX BLE、OTA、Adafruit ST7735、160x80 Dirty Region UI。

## 2026-10-18 — PRO TFT Modular Dirty Region UI Framework

- [重寫] `brd_tft.cpp`：僅保留狀態收集、節流與 MAX generation 回報，保留 `brd_tft_begin/update` 原介面。
- [新增] `brd_ui_types.h`：統一頁面列舉與畫面狀態快照。
- [新增] `brd_ui_screen.h/.cpp`：官方 Adafruit ST7735 初始化、SPI 與背光獨立模組。
- [新增] `brd_ui_core.h/.cpp`：頁面路由；只有換頁才全清畫面，同頁直接 dispatch Dirty 元件。
- [新增] `brd_ui_widgets.h/.cpp`：獨立電池/充電、BLE、測量模式元件。
- [新增] `brd_ui_page_main.cpp`：只覆寫 RPM 變更字元；STATUS、MODE、文字標籤有個別重畫區。
- [新增] `brd_ui_page_ir.cpp`：IR1/IR2/電池電壓分離更新，保留既有 DXF 頂點座標。
- [新增] `brd_ui_page_ota.cpp`：新增進度百分比扇形差分繪圖。
- [新增] `brd_ui_page_alert.cpp`：Boot/Low Battery/ADC Error 靜態頁面。
- [新增] `tests/test_ui_dirty.cpp`：不變值不傳畫面、部分更新不呼叫全螢幕清除、換頁僅清一次。
- [修改] `tests/render_boot.cpp` / `tests/run_host_tests.py` / `tests/test_ir_dxf_geometry.py`：配合頁面模組拆分。
- [新增] `docs/UI_DIRTY_REGION_ARCHITECTURE.md`：新增 UI 頁面工作規範與檔案位置。
- [保留] V1.2 BLE/OTA、160x80、CS 接地、RESET 10k/1uF、Adafruit 官方庫、PRO GPIO。
- [注意] 未使用 ESP32 實際工具鏈編譯與實機驗證，不附 `.bin`。

## PRO TFT 顯示修正（2026-10-18）

- [修改] `TFT_INVERSION_ON=1` 修正白底（PLUGIN 反相命令映射）。
- [修改] `TFT_SPI_FREQUENCY_HZ` 由 16MHz 降 8MHz。
- [修改] `flush()` 由單筆 25.6KB 改 8 行一筆 2.56KB。
- [新增] `TFT_TRANSFER_ROWS` 與 TFT 實機診斷文件。
- [保留] OLED 以外 BPX 韌體模組、CS 接地及 RESET 10k/1uF 不變。
- [注意] 實機有待確認。

## PRO V1.2 - 橫向深色顯示與 RC RESET 啟動時序 / 2026-10-18

- [修改] `brd_config.h`：80×160 直式改為 160×80 橫式，`TFT_ROTATION=1`；新增 180ms 上電穩定等待及 80ms 重新初始化間隔。
- [修改] `brd_tft.cpp`：所有畫面轉為橫向深色 UI，雙重 Adafruit ST7735 `initR()`、初始化時關閉背光。
- [確認] LCD RESET 硬體為 10kΩ 上拉 3.3V + 1µF 下拉 GND，時間常數約 10ms；未改動電路。
- [注意] GPIO21 ROM UART TX 與 LCD SCLK 共用，CS 接地無法屏蔽上電雜訊；本次韌體只能降低風險，不能保證完全消除。
- [修正] `brd_tft.cpp` 新增 G/K 字型，解決 CHARGE/CHECK 顯示缺字。
- [修改] `tests/test_ir_dxf_geometry.py` 驗證 160x80 的 IR TEST 新位置與 10px 水平間距。
- [保留] 量測、BLE、設定 API、OTA 與原 BPX V1.2 版本字串均不變。

# BRD BPX PRO 異動紀錄

## PRO V1.2 - Adafruit ST7735 library migration / 2026-10-18

- [修改] `brd_tft.cpp`：改用 `Adafruit_ST7735`（依賴 Adafruit GFX / BusIO），移除手動 ST7735 初始化、SPI transaction 與 CASET/RASET。
- [修改] `brd_config.h`：使用 `INITR_MINI160x80_PLUGIN`，由函式庫管理 X26/Y1 偏移及 BGR；調整 inversion 設定以保留舊指令。
- [修改] 緩衝區改為 12800 個 uint16_t 並以 big-endian RGB565 傳送 80×160 全幀，保留 UI。
- [新增] 測試 Adafruit ST7735 API stub，執行主機端畫面渲染回歸。

## PRO V1.2 / 2026-10-18

- [新增] `BRD_BPX_PRO/BRD_BPX_PRO.ino`：獨立專案入口，保留原版 BPX V1.2 行為。
- [新增] `brd_tft.cpp`, `brd_tft.h`：ST7735S 80×160 RGB565 SPI 驅動與主畫面、IR TEST、低電與 OTA 圓球進度條。
- [刪減] 舊 `brd_oled.cpp`, `brd_oled.h`，不再包含 SSD1306 I2C 或 GPIO8 充電 LED 控制。
- [修改] `brd_config.h`：PRO 名稱與 GPIO4/5/6/7/8/2/20/21、TFT 時序、色序與偏移。保留 BPX UUID/OTA/設定/測量預設值。
- [修改] `brd_io.cpp`, `brd_io.h`：電源鎖存、按鈕輸入、IR 發射電源使能；保留原 RPM、LOAD、電池與充電接腳。
- [修改] `brd_measurement.cpp`：畫面保持參數從 OLED 換成 TFT，值仍為 2500 ms。
- [修改] `brd_ota.cpp`：驗證期間刷新由 OLED API 改為 TFT API。
- [PRO 修改] 更新畫面改成變化時才傳輸；普通量測最多 10 FPS，保護 BLE OTA 與 RPM 中斷排程。
- [新增] 測試 SPI mock、ST7735S RGB565 渲染器與預覽 PNG；原主機迴歸測試維持執行。
- [保留] 原版 V1.2 燒錄/OTA 分割表；不附不相容舊 binary。

**韌體版本字串暫維持 `V1.2`**，但專案名稱與開機畫面識別為 BPX PRO。尚未進行實機 ESP32 編譯和電路驗證。

## 2026-10-18 — ST7735 官方 Adafruit GFX 重構（Display Library Only）

- [重寫] `brd_tft.cpp`：以 Adafruit 官方 GFX 方法直接繪製文字、形狀及 OTA 進度；LCD SPI 控制及初始化使用 Adafruit_ST7735。
- [刪除] RGB565 影格陣列、byte swapping、自製 5x7 字庫、手動設定 LCD address window / 原始像素資料傳輸。
- [保留] 160×80 橫向、黑底、CS 接 GND、RESET 10 kΩ + 1 µF、GPIO20/21/2/8、AUTO/LOAD、BLE、OTA 及所有狀態頁。
- [新增] `examples/Adafruit_ST7735_PanelCheck/` 官方函式庫最小實機測試；`tests/test_adafruit_api.py` 及更新主機端 GFX 模擬。
- [注意] 未進行 ESP32 真實工具鏈編譯或 LCD 實機測試；此版本為待燒錄驗證的原始碼。


## 2026-10-19 — BRD BPX PRO 完整 UI 版面整合（V1.2）

- [修改] 主畫面狀態標題 `WAIT LOAD` / `LOADED` / `HOLD` 使用 Adafruit GFX `setTextSize(2)`；主數值使用 `setTextSize(3)`；`MAX` 標籤使用 `setTextSize(2)`。
- [刪除] 主頁與曲線頁底部的 `MODE`、`BTN` 導覽文字。
- [修改] 曲線頁 Y 軸 24～68、X 軸 3～157，底部改為靠左 `MAX SP XXXXX`：標籤 X=4、數值 X=46。
- [修改] IR TEST 顯示 2 倍標題、右上電池圖示與電壓、中央觸發白色感測器、左 LOAD ADC 與右 RPM ADC 垂直條，移除原本 `ADC 0-4095` 底部整列。
- [修改] OTA 標題統一為 `OTA`，加大進度百分比；開機與警告頁採用一致的字體層級與分隔線。
- [保留] Adafruit ST7735/GFX 官方驅動、160×80 橫向、Dirty Region、GPIO5 按鍵、LOAD ADC、RPM ADC、BLE 與 OTA 傳輸邏輯。
- [驗證] 15 組主機端測試通過；18 種頁面從正式 C++ UI 模組重新輸出，未完成 ESP32-C3 實板編譯與螢幕實測。

## TFT Motion UI 20 Styles (V1.2 source integration)

- [ADD] `brd_ui_motion.h/.cpp`: 20 dark RGB565 themes, seven animated screen renderers.
- [ADD] `brd_bbp_recent_history()` read-only last-four BBP history API.
- [CHANGE] GPIO5 cycles Main/Graph/History, and Main automatically shows Max state.
- [CHANGE] `brd_ui_core` delegates to motion pages; OTA/Boot keep the previous page implementation.
- [CHANGE] 60-ms frame interval (300 ms for warnings), dirty regions and no animation-time `delay()`.
- [KEEP] Adafruit ST7735, 160x80, 8 MHz SPI, LOAD/RPM ADC, BLE and OTA protocol.
- [TEST] Host test and render; no actual Arduino/ESP32-C3 compile or physical ST7735 measurement yet.

## BRD BPX PRO V1.2 — UI original styles / dirty tile improvement (2026-10-20)
- [新增] 原始20種風格 7頁顯示模組及可擴充純GFX場景接口。
- [新增] 8x16 Dirty Tile RGB565 單畫布局部傳輸。
- [修改] 非阻塞更新排程、CPU及SPI傳輸節流。
- [刪除] 舊版共用少量版面再改20個色盤的主畫面。
- [保留] OTA/ADC/BLE/LOAD判定/NVS流程與 GPIO5 切頁。

## V1.2 - GPIO5 四頁滑動動畫（2026-10-21）

- [新增] `brd_ui_transition.cpp/.h`：主畫面→曲線→歷史→IR TEST→主畫面，使用 RGB565 橫向推移、5 步、每步 32px、非阻塞 80ms 排程。
- [修改] `brd_ui_core.cpp`：僅 GPIO5 本地四頁套用轉場；MAX、BOOT、LOW BATTERY、ADC FAULT、OTA、BLE 強制 IR TEST 直接切換。
- [修改] `brd_ui_motion.cpp/.h`：轉場完成後不清除螢幕、不重新送完整畫面。
- [修改] `brd_ui_dirty_presenter.cpp/.h`：提供既有 RGB565 framebuffer 供原地合成，保留 8x16 Tile Dirty SPI 更新。
- [新增] host transition test + C++ 原始場景渲染 GIF 工具。
- [保留] 9 種風格、BLE 圖示、低電紅格閃爍、原有 ADC/BLE/OTA/RPM 功能。韌體版本不變。

## BRD BPX PRO V1.2 — Charging UI redesign (2026-10-22)

- [修改] MAIN / IR TEST 使用電池內部流動亮帶和三點循環動畫，不再繪製閃電。
- [新增] MAIN 右上角同時顯示電量百分比及 `CHG` 充電狀態。
- [保留] IR TEST 電池電壓 2 位小數、BLE 圖示、RPM、LOAD/AUTO 與 3 種風格。
- [新增] 240 ms 動畫相位透過 `Model.charge_phase` 傳遞；充電時 MAIN / IR TEST 才在變更時局部刷新。
- [保留] GPIO10 CHRG_DET 原先的 LOW=充電判斷，與 ADC 實際容量；不改充電保護、BLE 與 OTA。
- [修改][測試] 原測試固定假設 8 MHz/50 ms，依此次來源專案的 16 MHz/33 ms 更新。
- [新增][測試] `tests/test_charge_ui.cpp` 驗證 3 風格兩頁、充電相位，且不覆寫主要繪圖。
- [驗證] `tests/run_motion_tests.py` 主機端回歸測試通過；尚未以真實 ESP32-C3/面板實測。
