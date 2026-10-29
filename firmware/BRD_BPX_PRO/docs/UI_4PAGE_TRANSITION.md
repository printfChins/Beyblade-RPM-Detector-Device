# BRD BPX PRO V1.2 - GPIO5 四頁滑動切換模組

## 適用環境

- ESP32-C3-MINI-1-N4 / ESP32C3 Dev Module
- TFT：160 × 80，ST7735S，橫向，Adafruit ST7735 + Adafruit GFX
- GPIO5：`INPUT_PULLUP`，短按放開後切換一頁，40 ms 去抖
- 風格共 9 種：NEBULA、DIAL、WAVE、TORQUE、ORBIT、VECTOR、RAPTOR、ECLIPSE、FLUX
- 韌體版本維持 V1.2

## 四頁切換順序

```
Main  -> Graph -> History -> IR TEST -> Main
```

下列頁面不套用動畫：MAX、LOW BATTERY、ADC FAULT、OTA、BOOT。
BLE 指定 IR TEST 也不套用動畫，具有優先權。
系統頁在動畫進行中觸發時立即中止動畫並顯示系統頁。

## 新增與修改檔案

```
BRD_BPX_PRO/
├── BRD_BPX_PRO.ino                 [不修改]
├── brd_ui_transition.h            [新增] 切頁動畫 API
├── brd_ui_transition.cpp          [新增] RGB565 橫向推移合成
├── brd_ui_core.cpp                [修改] 四頁動畫啟動、中斷及完成路由
├── brd_ui_motion.h                [修改] 已完成切頁畫面接管 API
├── brd_ui_motion.cpp              [修改] 接管後不重送全螢幕
├── brd_ui_dirty_presenter.h      [修改] 公開 Framebuffer 指標
├── brd_ui_dirty_presenter.cpp    [修改] 實作 Framebuffer 指標 API
├── tests/
│   ├── test_ui_transition.cpp         [新增] 九風格像素相等測試
│   ├── export_transition_frames.cpp  [新增] 與正式韌體共用 C++ 動畫碼
│   ├── export_4page_transition_gif.py [新增] 從 160x80 RGB565 幀輸出 GIF
│   └── run_motion_tests.py           [修改] 加入動畫測試
└── docs/
    ├── UI_4PAGE_TRANSITION.md
    └── ui_transition_previews/
        ├── ALL_9_4PAGE_TRANSITIONS.gif
        └── 01~09_..._4PAGE_TRANSITION.gif
```

## 動畫策略

1. 舊畫面使用 Dirty Presenter 原本的 160x80 RGB565 framebuffer。
2. 進入新頁前，將目標頁繪製至另一張 160x80 RGB565 暫存畫布。
3. `brd_ui_transition` 每次移動 32 px，一共 5 步，理想間隔 80 ms。
4. 每行以 `memmove` 左移原 framebuffer，再複製新畫面的一條 32 px 窄帶。
5. 呼叫原本的 `dirty_presenter::flush()`，只傳輸像素變動區域。
6. 最後一步完成後將新頁交還 `motion::adopt_rendered()`，不做全畫面清除。

### SRAM 增量

- 只增加目標畫面 160 × 80 × 2 = **25,600 bytes**（於第一次切頁時配置，之後重用）。
- 不額外配置舊畫面備份；利用現有 framebuffer 原地推移。
- 沒有使用 GIF 解碼器、畫面轉場全幀圖片或額外 SPI 驅動。
- 若暫存畫布配置失敗，自動退回原本無動畫換頁，不影響量測。

### 效能注意

滑動特效涉及整頁像素位移，每一步最多可能更新完整 25,600 bytes。
5 步最壞約 128,000 bytes，不是 5 步合計只更新 25,600 bytes。
目前 TFT 傳輸時脈 4 MHz，純傳輸 25,600 bytes 理論下限 51.2 ms／步，
另須加計 GFX 和 SPI 指令／資料處理時間，因此 80 ms 為排程目標，不保證實際幀率。

一般靜態頁面、轉速與電池數值等仍使用 Dirty Tile；只有 GPIO5 的四頁切換需要上述滑動合成。
所有畫面操作都在主程式 loop；GPIO3 RPM ISR 不增加任何動畫運算。

## 驗證

- 主機端：九套風格 × 四段切頁 × 五步，每一步都使用正式 C++ 模組。
- 最終 framebuffer 與目標 C++ 場景逐像素相同。
- Dirty Presenter 對 LCD 測試替身的資料逐像素相同。
- 低電、ADC 異常、OTA 不會套用動畫。
- 單次動畫進度由 `millis()` 排程，沒有新增阻塞式 `delay()`。
- 原有量測、BLE、OTA、ADC、九風格測試已回歸。

**限制：尚未以 Arduino ESP32-C3 Core 3.3.11 完成實機編譯與真正 TFT SPI FPS、RAM 碎片化驗證。**
