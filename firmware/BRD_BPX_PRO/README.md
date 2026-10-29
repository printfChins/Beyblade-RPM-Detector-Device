> **V1.4 RPM 優先曲線資料流程**：有效完整單圈的 RPM、週期、時間與極性先保存至固定原始樣本 RAM，直到發射完成才產生 TFT 64 點 / BLE 32 點曲線，不再以 50ms 間隔直接遺棄中途讀數。詳細參閱 [`docs/TORQUE_V1_4_RPM_FIRST_CAPTURE.md`](docs/TORQUE_V1_4_RPM_FIRST_CAPTURE.md)。

# BRD BPX PRO V1.3 — TORQUE V3 韌體（2026-10-29）

> 此段為最新版本說明。下方 V1.2 文件為歷史設計記錄；請以 `brd_config.h` 與 `docs/TORQUE_V1_3_FIRMWARE_CHANGES.md` 為準。

- UI 僅保留風格三 TORQUE；主頁左對齊 SP/MAX（x=60、字體 3 倍），裝載動畫原位置，HISTORY 8 筆兩欄；開機 0→8 格、OTA 文字修正。
- LOAD：GPIO1 12-bit ADC 原始數值 `< 4000` 視為裝載，100ms 去抖。
- RPM：GPIO3 CHANGE，正→正與負→負各自完整一圈計算；各組都更新即時 RPM/MAX。
- BLE：每個極性各自記錄前 32 個完整圈週期，完成後只將包含最高 MAX 的一組存入原 BBP 格式；Flash、UUID、封包長度未改。
- 本機曲線每 50ms 從事件取最新值，最多 64 點，逐點圓點連線；此資料與 BLE 曲線分開。
- 開發相依：Arduino-ESP32 (ESP32-C3)、Adafruit GFX、Adafruit ST7735、U8g2_for_Adafruit_GFX、NimBLE-Arduino 等，請比對原 V1.2 專案。
- `python3 tests/run_motion_tests.py` 可於原專案附帶的主機端模擬環境執行測試；不等同於硬體編譯／測試。

---

# 當前交付版：僅保留 0～2 三風格（2026-10-22）

- [保留] `0 NEBULA`、`1 DIAL`、`2 TORQUE`；預設 `BRD_UI_MOTION_THEME = 2U`。
- [刪除] 第四風格 ORBIT SIGNAL BEACON 對應模組、頁面、測試與預覽。
- [保留] W1 WAIT LOAD、L2 LOADED、單一 SP、DIAL 無 TACHO、充電電池流光、16 MHz SPI 與 40 ms（25 FPS）目標更新間隔。
- 下方描述 9 種等數量的舊版章節為版本發展紀錄，**不代表目前編譯提供的風格**；現在唯一有效索引為 0、1、2。

---

# 本次 V1.2 UI 更新：W1 / L2 / SP

- [新增] 三風格 WAIT LOAD 狀態採 W1 呼吸特效。
- [新增] 三風格 LOADED 狀態採 L2 聚焦鎖定，括號與文字不重疊。
- [修改] 顯示畫面中的 RPM 單位改為 SP；量測、BLE、OTA 傳輸維持原協議數據單位。
- 詳見 `docs/W1_L2_SP_UI_20261022.md` 和 `docs/W1_L2_SP_PREVIEW/`。

---

> [2026-10-22 新增] **LIVE Peak Hold / NEBULA 雙環動畫**：三風格主畫面數值只升不降，原始即時 RPM、BLE 與曲線保留下降趨勢。NEBULA 未裝載雙圈熄滅、裝載黃圈旋轉、LOAD 解除藍圈按峰值延伸。詳見 `docs/NEBULA_LOADED_LAUNCHED_PEAK_UI.md`。

# BRD BPX PRO V1.2 — 3 selected UI styles

[修改] 新版僅保留 NEBULA / DIAL / TORQUE。主題設定 `BRD_UI_MOTION_THEME` 為 0、1、2。
[保留] MAX 在主畫面內顯示；GPIO5 主頁→曲線→歷史→IR TEST 循環。
詳細對照請見 `docs/UI_3_STYLES.md`。

---

# 最新修改：V1.2 MAX SP 直接顯示於原本主畫面

- [修改] 測量完成後不再跳轉獨立 MAX 頁；九套主畫面改顯示 `MAX SP` 與峰值轉速，保留主題構圖。
- [修改] MAX 轉態於原主畫面局部更新，短暫動畫後停止，避免每次最高轉速都重新切頁。
- [保留] GPIO5 四頁切換：主畫面（LIVE／MAX）→ 曲線 → 歷史 → IR TEST；BLE／OTA 與電池警示不變。
- 詳細說明：`docs/MAX_ON_MAIN_V1.2.md`。

---

# 最新修改：主頁右下角狀態列移除、BLE 圖示重新設計（9 Styles）

- [刪除] 右下角 WAIT LOAD／LOADED／READY／HOLD／SAVED 狀態列，僅取消顯示，量測邏輯不變。
- [修改] 電池左側的 BLE 連線圓點改為 Bluetooth 符號：已連線亮色、未連線灰色、BLE 停用紅色斜線。
- [修正] BLE 停用狀態也觸發 Dirty Tile 重新更新。
- [保留] 9 種 UI、GPIO5 四頁循環、Adafruit ST7735、8×16 Dirty Tile、BLE／OTA／ADC。
- 完整位置與預覽：`docs/UI_BLE_NoBottomStatus/README.md`。

---

# 最新修改：BRD BPX PRO V1.2 僅保留指定 9 種 UI 風格

- [刪除] 其餘 11 套主題的顏色表、主頁場景與警告頁背景圖樣。
- [保留] 原始編號 01 NEBULA、02 DIAL、03 WAVE、06 TORQUE、07 ORBIT、11 VECTOR、13 RAPTOR、17 ECLIPSE、18 FLUX。
- [修改] `BRD_UI_MOTION_THEME` 使用新索引 0..8；舊編號與新索引對照見 `docs/UI_9_STYLES.md`。
- [保留] 七個頁面、GPIO5 四頁短按、LOAD/RPM ADC、BLE、OTA、Dirty Tile 與低電紅色格閃爍。
- [新增] `docs/selected_9_styles/`：從正式場景 C++ 輸出的 160x80 九套預覽。
- [驗證] 63 張頁面與原版被保留的相應畫面逐像素一致；主機端測試通過。

---

# 最新修改：GPIO5 四頁短按導覽（V1.2）

- [修改] `GPIO5 INPUT_PULLUP`，按下 LOW，短按放開後切換一頁。
- [修改] 導覽順序：主畫面／MAX → 曲線 → 歷史紀錄 → IR TEST → 主畫面。
- [新增] 40 ms 去抖及 800 ms 短按上限；長按不切頁，不連續觸發。
- [新增] `brd_ui_navigation.h` 集中管理頁面路由優先權。
- [修改] 本地第四頁 IR TEST 會同步讀取 GPIO1 LOAD ADC、GPIO4 RPM ADC。
- [保留] 9 種指定風格、Dirty Tile、BLE、OTA、電池警告、RPM ISR。
- 詳見 `docs/GPIO5_FOUR_PAGE_NAVIGATION.md`。

---

# 歷史修改：獨立低電與 ADC 異常警告頁（現已精簡為 9 種）

- [新增] `brd_ui_warning_pages.cpp/.h`，讓兩種警告頁保留指定 9 種原始主畫面視覺元素。
- [保留] 160×80、Adafruit ST7735 + GFX、單 RGB565 GFXcanvas16 + 8×16 Dirty Tile。
- [保留] LOAD ADC、RPM ADC、BLE、OTA、NVS、GPIO5 切頁、動畫排程與主程式。
- 詳見 `docs/WARNING_THEMES_V1.2_20261020.md`。

# TFT 動畫版交付說明

此版保留 9 組深色主題、七個可在 ESP32-C3 執行的 Adafruit ST7735/GFX 非阻塞動畫頁面。
**主要整合說明：`docs/TFT_ANIMATION_MODULE.md`**。
主題切換請修改 `brd_config.h` 的 `BRD_UI_MOTION_THEME`（0～8），GPIO5 現在為四頁循環（Main/Max → Graph → History → IR TEST）。

## 最新 UI（160x80 橫向，SP 滿版與 Dirty Region）

- [修改] 主畫面頂部改顯示裝載狀態 `WAIT LOAD` / `LOADED` / `READY` / `HOLD` / `SAVED`。
- [修改] 藍牙符號顯示在電池左側，充電閃電顯示在電池右側。
- [修改] 中間 SP 數字完整橫向排列，MODE 位於底部；GPIO5 切換曲線頁。
- [修改] 曲線顯示在 x=3..157 範圍；LAST SHOT 自動展寬至全幅。
- [保留] 各 Widget Dirty 更新、Adafruit GFX，頁面切換才全清螢幕。
- [新增] 版面座標與新增頁面開發規範：`docs/UI_MAIN_GRAPH_LAYOUT.md`。

---

## BRD BPX PRO LOAD ADC 更新（GPIO1 / ADC1_CH1）

- [新增] GPIO1 LOAD 現在使用校正 ADC 毫伏值判定，原數位 GPIO 中斷已移除。
- [新增] GPIO0 電池與 GPIO1 LOAD 共用 brd_adc_shared.cpp 的 ADC1 oneshot handle；各自校正。
- [新增] brd_load_adc.cpp 實作 20ms 取樣、1.60V / 1.95V 雙門檻遲滯，失效保護。
- [修改] LOAD 去抖為連續 100ms，AUTO 模式不使用 LOAD 觸發量測。
- [修改] IR TEST 的 IR2 顯示 GPIO1 LOAD ADC 判定結果。
- [保留] 160x80 橫向 TFT、Adafruit ST7735、Dirty Region UI、BLE/OTA 與 GPIO3 RPM 中斷。

**注意：** 0.89V/2.65V 目前只確認是輸入量測範圍，尚未確認裝載極性。
預設暫定 0.89V=已裝載、2.65V=未裝載（`LOAD_ADC_ACTIVE_LOW=1`）。
如實機相反請改為 `LOAD_ADC_ACTIVE_LOW=0` 並重新燒錄。

詳見 `docs/LOAD_ADC_GPIO1.md`。

---

# BRD BPX PRO — ESP32-C3-MINI-1-N4 / ST7735S / 160x80 TFT

[PRO 新增] 以 `BRD_BPX(10).zip` (BPX V1.2) 為基礎複製新專案 `BRD_BPX_PRO`，保留原版專案，不覆寫舊版。
[PRO 修改] SSD1306 128x32 I2C 顯示器改為 GMT096-04-WG 0.96-inch 160x80 TFT；驅動 IC 依規格書為 ST7735S，4-wire SPI。

## 新版畫面更新框架（Dirty Region，模組化）

**[修改]** UI 已拆分為 `brd_ui_screen`（LCD HAL）、`brd_ui_core`（頁面路由）、
`brd_ui_widgets`（共用元件）、`brd_ui_page_main/ir/ota/alert`（獨立頁面）。
只有換頁時清除全螢幕；**同頁更新只繪製發生變化的元件區域**，
RPM 同位數時僅重畫變動字元，OTA 進度只補畫新增扇形。
原主程式與 `brd_tft_begin/update()` 呼叫方式不變。

後續新增/調整 UI，請先閱讀 `docs/UI_DIRTY_REGION_ARCHITECTURE.md`；
新增頁面不需要改動 `BRD_BPX_PRO.ino`。

## Arduino IDE 開啟方式

1. 解壓縮 `BRD_BPX_PRO.zip`，所有 `.ino/.cpp/.h` 檔案置於 **同一層** `BRD_BPX_PRO/`。
2. 開啟 `BRD_BPX_PRO/BRD_BPX_PRO.ino`，板卡選擇 ESP32C3 Dev Module (4 MB Flash, 80 MHz)。
3. 依來源專案使用 Arduino-ESP32 3.3.11、NimBLE-Arduino 2.5.1，需 ESP-IDF >= 5.3。
4. **[PRO 新增] Arduino IDE 函式庫管理員安裝 `Adafruit ST7735 and ST7789 Library`、`Adafruit GFX Library`、`Adafruit BusIO`**，由 Adafruit 函式庫管理 ST7735S 初始化與 SPI 傳輸。
5. **[修改] 額外安裝 `U8g2_for_Adafruit_GFX`**（Arduino 程式庫管理員搜尋此名稱，或使用 [官方 GitHub](https://github.com/olikraus/U8g2_for_Adafruit_GFX) 的 ZIP 安裝）。正式程式使用現成的 `u8g2_font_7x13_tr`／`u8g2_font_6x10_tr`，已移除自行編寫的字型點陣表，詳見 `docs/UI_LEGIBLE_TEXT_V12.md`。
6. [保留] BLE OTA 專用 1984 KiB x 2 APP 自訂分割表 `partitions.csv`。原 BPX V1.2 的 OTA、設定與歷史資料 NVS 格式未變。
7. [重要] 本交付物是**原始碼**，尚未使用 ESP32 真正工具鏈編譯、尚未燒錄，因此未附 `.bin`。**不得將 BPX 的既有 bin 燒錄到 PRO 板**。

## GPIO 設定

| ESP32-C3 GPIO | PRO 用途 | 型態 / 程式處理 |
|---|---|---|
| GPIO0 | BATT_ADC | ADC 量測，原分壓換算暫沿用 470k/470k |
| GPIO1 | IR_LOAD | ADC1_CH1 類比電壓偵測，共用 ADC1 校正與雙門檻遲滯；不使用 GPIO1 中斷 |
| GPIO2 | DC_LCD | TFT D/C，OUTPUT，資料 HIGH / 命令 LOW，**Strapping 腳** |
| GPIO3 | IR_RPM | RPM IRQ CHANGE，無上下拉 |
| GPIO4 | IR_RPM_ADC | INPUT 無上下拉，保留 ADC 通道，**未新增 ADC 演算法** |
| GPIO5 | BUTTON | INPUT_PULLUP，按下 LOW；40 ms 去抖，短按 4 頁循環（Main/Max → Graph → History → IR TEST） |
| GPIO6 | SYS_POWER | OUTPUT，預設 HIGH 維持電源自鎖，`SYS_POWER_ACTIVE_LEVEL` 可修改 |
| GPIO7 | IR_SWITCH | OUTPUT，預設 HIGH 開啟 IR，低電／OTA 期間關閉，`IR_SWITCH_ACTIVE_LEVEL` 可修改 |
| GPIO8 | BACKLIGHT_LCD / LEDA | OUTPUT，預設 HIGH 亮，**Strapping 腳**；不再作充電 LED |
| GPIO9 | BOOT | 保留原生下載模式控制；專案未重配置 |
| GPIO10 | CHRG_DET | INPUT_PULLUP，LOW 為充電中 |
| GPIO18 | USB D- | 原生 USB |
| GPIO19 | USB D+ | 原生 USB |
| GPIO20 | TFT SDA | SPI MOSI（不再是 I2C SDA） |
| GPIO21 | TFT SCL | SPI SCLK（不再是 I2C SCL） |

### TFT 接線

GMT096-04-WG 8-pin FPC: 1 LEDA → GPIO8 控制之限流/開關電路；2 GND → GND；3 RESET → **10kΩ 上拉至 3.3V，1µF 對 GND 的 RC 復位電路（已由使用者確認）**；4 D/C → GPIO2；5 SDA → GPIO20 (MOSI)；6 SCL → GPIO21 (SCLK)；7 VDD → 3.3V；8 CS → GND（本版 SPI 僅 TFT 一個從裝置，允許一直選取）。

**注意硬體尚待確認**：

- [PRO 注意] 由於 GPIO2 與 GPIO8 是 ESP32-C3 的 strapping 腳，**上電/RESET 採樣階段**就必須滿足開機條件，程式 `pinMode()` 只能在開機後執行，不能修復不正確的開機偏壓。電路圖 R13 為 GPIO2 / D/C 的 10 kΩ 上拉；GPIO8/LEDA 必須額外檢查背光 LED 負載是否影響啟動，不可僅靠軟體確保。
- [PRO 注意] GPIO8 控制 LEDA 時，請依 TFT 規格書設定背光限流，並確認總電流與 ESP32-C3 輸出能力。若使用 NMOS/P-MOS 轉接，`TFT_BACKLIGHT_ON_LEVEL` 的極性須對應外部電路。
- [PRO 注意] GPIO6 `SYS_POWER` 預設高有效、GPIO7 `IR_SWITCH` 預設高有效，**這是尚未看到自鎖/MOS 開關完整電路時的假設**。製作板之前務必確認實際高低電位，否則可能無法維持供電或讓 IR 恆亮。
- [PRO 注意] TFT RESET 已使用 10kΩ 上拉與 1µF 下拉（時間常數約 10ms）；開機等候 180ms 後進行兩次 ST7735S 軟體初始化，兩次間隔 80ms。軟體復位不能救援外部 RESET 持續拉低，也無法保證快速關機再開機時電容完全放電。
- [PRO 注意] GPIO20/21 從 UART0 複用為 SPI，避免程式同時以這兩腳使用 UART0。
- [PRO 注意] ADC 換算與先前 BPX 相同，請依新 PCB 電阻值修改 `BATTERY_DIVIDER_R_TOP_OHM` 和 `BATTERY_DIVIDER_R_BOTTOM_OHM`。

## TFT 驅動顯示：官方 Adafruit 版本（2026-10-18）

- **[重寫] `brd_tft.cpp`**：直接使用 Adafruit 官方 [`Adafruit-ST7735-Library`](https://github.com/adafruit/Adafruit-ST7735-Library) 與 `Adafruit GFX Library`。硬體 SPI 與 ST7735 的內部命令由函式庫管理。
- **[刪除] 自製顯示驅動／像素傳輸**：舊 `g_frame` 25,600-byte RGB565 framebuffer、字型 `tft_get_glyph()`、手動 byte-swap、`setAddrWindow()` / `writePixels()` / `flush()` 全數移除。
- **[修改] 圖形 API**：官方 `fillScreen()`、`fillRect()`、`drawRect()`、`drawLine()`、`drawCircle()`、`fillCircle()`、`fillTriangle()`；文字使用 `setCursor()` / `setTextSize()` / `print()`。
- ST7735S 80×160 原生面板設定為 **160×80 橫向**（`setRotation(1)`），**深色模式**；預設採 `INITR_MINI160x80_PLUGIN`，由官方函式庫處理 GRAM 地址偏移（非自行處理）。
- CS 焊死 **GND**、RESET 由 **10 kΩ 上拉至 3.3V + 1 µF 對地**控制；官方類別傳入 CS=-1、RST=-1，DC=GPIO2；SCLK=GPIO21、MOSI=GPIO20、背光=GPIO8。
- SPI 預設 8 MHz。`invertDisplay(true)` 對應 PLUGIN profile 的反相映射，沿用已經確認黑底的實板設定；**若面板批次不同，仍需實測調整**。
- **[保留]** 電池／BLE、AUTO/LOAD、主畫面、IR TEST、MAX SP、低電與 OTA 圓形填充及歷史通知流程。
- **[新增] 最小 TFT 單獨測試**：Arduino IDE 獨立開啟 `examples/Adafruit_ST7735_PanelCheck/Adafruit_ST7735_PanelCheck.ino`。在不啟動 BRD 功能的情況下測試四色角塊、文字、畫面邊界及初始化穩定性。詳見 `docs/TFT_ADAFRUIT_PANEL_TEST.md`。
- **[測試]** `python3 tests/run_host_tests.py` 包含官方 API 介面靜態檢查與 TFT 渲染替身檢查。`docs/TFT_*.png` 是主機端**模擬圖片**，**不是** ESP32-C3 實機拍攝或 Adafruit 官方函式庫的 pixel-for-pixel 輸出。
- **[未驗證]** 尚未使用 ESP32-C3 真正工具鏈編譯，未在實體 LCD 上驗證新的函式庫繪圖。因此不能保證先前文字斜移/錯亂已完全修復。

## 原有 BPX 功能不變

- [保留] LOAD/AUTO、RPM 取樣、MAX HOLD 2.5 秒、50 ms LOAD 去抖、原預設 MAX 30% 結束（已存 NVS 設定優先）。
- [保留] BLE `BEYBLADE_TOOL_BRD_XXXX` 前綴及原 BBP service/characteristic UUID，確保舊上位機搜尋邏輯不因名稱變化而失效。
- [保留] 設定 BLE API、IR TEST 頁面 characteristic、OTA BLE service、NVS 格式、Flash 分割與 Flash 保護流程。
- [PRO 刪減] SSD1306 I2C 驅動 `brd_oled.cpp/.h`，不再編譯原 OLED 程式；原 bin / build 全數排除。

## 驗證

執行：`python3 tests/run_host_tests.py`。主機工具鏈測試包含量測/模式、設定、BLE 通道、OTA 協議、歷史發布閘門、ST7735S + GFX 公開 API 語法、模擬 TFT 畫面、OTA GFX 圓形填充、DXF 點座標。預覽放在 `docs/TFT_*.png`，它們是渲染產物而非實機拍攝結果。

[未驗證] ESP32-C3 實機編譯、實際 LCD 橫向旋轉方向/色序/偏移、背光電流/啟動 Strapping、電源自鎖、IR SWITCH 極性、實際 BLE OTA 吞吐與螢幕對刷新時間的影響。

## [2026-10-18 修改] 160×80 橫向深色模式 / CS 永久接地開機策略

- `[修改] brd_config.h`：`TFT_WIDTH=160`、`TFT_HEIGHT=80`、`TFT_ROTATION=1`、`TFT_POWER_STABLE_WAIT_MS=180`、`TFT_REINIT_GAP_MS=80`。
- `[修改] brd_tft.cpp`：深灰底色 / 淺色資訊 / 電池與 BLE 狀態，重排 Boot / CURRENT SP / MAX SP / IR TEST / OTA / LOW POWER / ADC ERR。
- `[修改] brd_tft.cpp`：先關背光、等待 LCD 電源與 RC RESET 穩定，呼叫兩次 `Adafruit_ST7735::initR()`，再次設定旋轉和顏色，送完首幅圖才開背光。
- `[確認] 硬體 RESET`：10kΩ 上拉 3.3V + 1µF 對地，RC 約 10ms。保持原電路不變；實測仍需檢查 3.3V、RESET、SCLK 和 D/C 時序。
- `[注意]` GPIO21 是 ROM UART0 TX 預設輸出腳，CS 永久接地時不具備片選隔離能力。若電源與 RESET 波形正常但異常率仍高，優先做 RTC/USB 冷開機與快速重啟對照；下次硬體改版建議控制 CS 或使用外部 RESET supervisor。
- `[測試]` 可執行 `python3 tests/run_host_tests.py`；所有畫面預覽在 `docs/TFT_*.png`。僅為模擬，不能替代 ESP32 真實編譯及開機可靠度測試。

- [新增] 開機異常硬體診斷步驟：`docs/TFT_CS_GND_STARTUP.md`。


## 2026-10-18 實機畫面修正

- 修正 Adafruit MINI160x80_PLUGIN 反相映射造成的白底。
- SPI 8 MHz；RGB565 寫入分成每筆 8 行。
- 具體診斷與實機 A/B 步驟參見 `docs/TFT_PHOTO_DIAGNOSIS.md`。


## 2026-10-19 BPX PRO 新 UI / 曲線頁

- [修改] 160x80 橫向深色主畫面，放大 BPX PRO 與 SP 標示文字。
- [新增] GPIO5 (`INPUT_PULLUP`, LOW 有效) 按下穩定 40 ms 後切換 Main / SP GRAPH；按住不連發，放開後再按才可再次切換。
- [新增] `SP GRAPH` 第二頁，使用 UI 專屬 RAM 快照以 100 ms 為最小間隔取樣即時 RPM，最多 50 筆（約 5 秒），展示 PEAK 及 LIVE / LAST SHOT。
- [保留] 量測完成保留最後曲線，直到下一次有效 RPM 開始。不修改原 BBP BLE 協議的 32 點曲線或 Flash 格式。
- [保留] BLE IR TEST、OTA、低電畫面優先於 GPIO5 本地頁面切換；忙碌時按鍵不切頁。
- [效能] 頁面切換一次全螢幕背景清除；曲線連續新增點時只畫新增線段；峰值提高時僅清除並縮放曲線區域。
- 模組設計與頁面註冊參考 `docs/UI_BUTTON_GRAPH_ARCHITECTURE.md`。
- `docs/TFT_main_BUTTON_UI.png`、`docs/TFT_graph_BUTTON_UI.png` 是主機端 Adafruit GFX 模擬預覽。

**限制：** 尚未使用 ESP32-C3 Arduino 工具鏈完整編譯、燒錄及實板量測。


## IR TEST 原始 ADC 顯示 (2026-10-19)

- 進入 BLE 指定的 IR TEST 頁面時，左側為 GPIO1 LOAD ADC 原始值、右側為 GPIO4 RPM ADC 原始值。
- 12-bit 顯示範圍 0–4095，無效顯示 `----`，右上角顯示電池圖示與實際電池電壓。
- RGB565/Adafruit ST7735 與 Dirty Region UI 架構均保留，沒有自行實作顯示驅動。
- `brd_rpm_adc.cpp/.h` 是新檔；Arduino IDE 自動編譯與 `.ino` 同層的 `.cpp`。
- 詳見 `docs/IR_TEST_ADC_UI_INTEGRATION.md` 與 `docs/TFT_IR_TEST_ADC_Integrated.png`。


### 2026-10-19 UI 版面整合（V1.2）

主畫面、曲線頁、IR TEST、OTA、開機與警告頁的最新版 UI 已整合。
曲線頁使用左靠齊 `MAX SP XXXXX`；字體設定為標題 2、SP 數值 3、MAX 標籤 2、輔助文字 1。
全部細節請參閱 `docs/UI_FIRMWARE_INTEGRATION_20261019.md`。

注意：此專案含 Arduino/ESP32-C3 相關依賴，主機端模擬測試通過不代表完成實機編譯。

## 2026-10-20 更新 — 原版 01~20 風格、局部傳輸

詳細內容與風格設定：`docs/TFT_ANIMATION_MODULE.md`。新增 `brd_ui_style_scene.cpp/.h` 與 `brd_ui_dirty_presenter.cpp/.h`。保留官方 ST7735/GFX 驅動，改用單 GFXcanvas16 + 8x16 Dirty Tile 合併傳輸。


### V1.2 - 3 styles RPM 指示條上限（設定檔）

在 `brd_config.h` 修改 `BRD_UI_INDICATOR_FULL_SCALE_RPM`，預設 `20000UL`。
風格 01 NEBULA 青藍外圈／黃色內圈保留底環，未裝載也看得見；亮色特效依 LOAD/LIVE/MAX 控制。
風格 02 DIAL 的刻度弧及風格 06 TORQUE 的 16 段條，也在 20000 RPM 滿格。
當實際轉速超過設定滿格值，**指示條保持滿格，數字依然顯示真實最高轉速**。
詳見 `docs/NEBULA_PERSISTENT_RINGS_20K_INDICATOR.md`。
