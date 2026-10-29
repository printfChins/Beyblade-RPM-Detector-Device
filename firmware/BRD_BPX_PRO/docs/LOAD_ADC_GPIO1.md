# BRD BPX PRO GPIO1 LOAD ADC

## 模組與檔案路徑

- `BRD_BPX_PRO/brd_config.h`：GPIO1、門檻、極性、取樣週期與去抖時間。
- `BRD_BPX_PRO/brd_adc_shared.cpp/.h`：共用的 ADC1 handle，電池 GPIO0/CH0、LOAD GPIO1/CH1 個別設定與校正。
- `BRD_BPX_PRO/brd_load_adc.cpp/.h`：ADC 取樣、電壓狀態分類、遲滯、讀值與錯誤狀態。
- `BRD_BPX_PRO/brd_measurement.cpp`：只接收經分類的 LOAD ACTIVE/INACTIVE 事件，100ms 去抖。
- `BRD_BPX_PRO/brd_io.cpp`：IR TEST 中 IR2 讀取 LOAD ADC ACTIVE 結果。

## 預設值（需確認裝載極性）

| 選項 | 設定 |
| --- | --- |
| 輸入電壓範圍 | 0.89V ~ 2.65V（使用者提供） |
| GPIO | GPIO1 = ADC1_CH1 |
| 初步判定極性 | `LOAD_ADC_ACTIVE_LOW=1`（0.89V=已裝載，暫定） |
| 低門檻 | 1.60V |
| 高門檻 | 1.95V |
| 低頻率取樣 | 20ms/次 |
| 穩定去抖 | 100ms |
| 連續 ADC 讀取失敗超時 | 300ms，回到 INACTIVE |

`LOAD_ADC_ACTIVE_LOW=1` 時：低於或等於 1.60V 變 ACTIVE；高於或等於 1.95V 變 INACTIVE；中間區段維持上次狀態。

`LOAD_ADC_ACTIVE_LOW=0` 時：大於或等於 1.95V 變 ACTIVE；低於或等於 1.60V 變 INACTIVE；中間區段維持上次狀態。

若超過 300ms 無有效 ADC 讀值，分類器將設成 INACTIVE，不會啟動新量測；已進行的轉速量測不會因此被清掉。

## 重要硬體限制

Espressif 官方文件說明 ESP32-C3 在 12 dB 衰減設定的量測電壓範圍約 0~2500mV。
使用者的最高端 2.65V 高於這個範圍，讀值可能不再有可保證的準確度；本實作門檻設於 1.60/1.95V，兩個門檻均低於 2.5V，不依賴高端精確電壓。

若需要準確測出 2.65V 而不只是判斷高低狀態，應先調整類比前端分壓後再實測校正。

資料來源：https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32c3/api-reference/peripherals/adc.html

## 實機驗證項目（尚未完成）

1. 確認 0.89V 對應已裝載或未裝載，必要時調整 `LOAD_ADC_ACTIVE_LOW`。
2. 確認 IR 發射器 GPIO7 開啟後，GPIO1 的 ADC 高低電壓符合預期。
3. 測試 1.60/1.95V 附近反覆晃動時不會誤觸發裝載。
4. 確認 LOAD ACTIVE 穩定 100ms 後，才開始偵測 RPM。
5. 確認 GPIO0 電池 ADC 與 GPIO1 LOAD ADC 在長時間運作中沒有 ADC1 衝突。
6. 確認 AUTO 模式不會以 LOAD 作為量測啟動依據。

