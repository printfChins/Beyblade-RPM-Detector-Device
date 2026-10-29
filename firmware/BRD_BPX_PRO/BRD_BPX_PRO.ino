/* [PRO 新增] 新專案獨立入口；[修改] OLED -> 160x80 橫向深色 ST7735S TFT。
   [保留] BPX BLE、NVS、LOAD/AUTO 與 V1.2 OTA 流程。 */
/*
    檔案位置: BRD_BPX_PRO/BRD_BPX_PRO.ino
    貼放位置: 完整主程式，Arduino IDE 開啟此檔，同資料夾保留全部 .cpp/.h。
    [PRO 保留] BPX V1.2 BattlePass BLE 周邊端。
    [新增] 電池檢查及開機畫面後才初始化 BLE，再啟動量測。
    [修改] 80MHz、GPIO3 CHANGE；正->正與負->負兩組完整一圈 RPM 交錯採樣；不變更主程式流程。
    [保留] LOAD ADC 去抖預設 100 ms、2.5 秒 MAX 自鎖。
    [修改] AUTO / LOAD 共用可設定的 MAX 百分比結束，預設 30%。
    [PRO 刪減] 不帶入原 BPX build 與 bin，避免誤燒舊韌體。
*/
#include <esp_system.h>
#include "brd_battery.h"
#include "brd_bbp.h"
#include "brd_config.h"
#include "brd_io.h"
#include "brd_measurement.h"
#include "brd_tft.h"
#include "brd_shot_capture.h" /* [新增 V1.4] 完成後才產生 TFT/BLE 曲線 */
/* [新增] 主程式設定模組入口。 */
#include "brd_settings.h"
#include "brd_settings_ble.h"
/* [V1.2 新增] 放在 setup() 前的 include 區。 */
#include "brd_ota.h"
void setup(void) {
    /* [新增] 所有硬體與量測啟動前，先載入 Flash 設定。 */
    /* [PRO 修改] 先保持 GPIO6 電源自鎖，再載入可能耗時的 NVS。 */
    setCpuFrequencyMhz(CPU_FIXED_FREQ_MHZ);
    brd_io_begin();
    brd_settings_begin();
    /* [PRO 新增] 系統鎖電腳在 brd_io_begin() 先行拉至維持電源電位。 */
    brd_battery_begin();
    brd_tft_begin();
    brd_battery_update();
    if (brd_battery_measurement_allowed()) {
        brd_bbp_begin();
        brd_battery_update();
    }
    if (brd_battery_measurement_allowed()) {
        brd_io_set_ir_enabled(true);
        brd_measurement_begin();
    } else {
        brd_measurement_stop();
        brd_bbp_stop();
    }
}
void loop(void) {
    brd_battery_update();
    /* [PRO 新增] 低電或 OTA 期間關閉 IR LED 功耗。 */
    brd_io_set_ir_enabled(brd_battery_measurement_allowed() && !brd_ota_is_busy());
    if (!brd_battery_measurement_allowed()) {
        brd_measurement_stop();
        brd_bbp_stop();
        /* [修正] SOC 恢復穩定後先保存 dirty 紀錄，成功才重啟，避免清除被撤回。 */
        if (brd_battery_restart_required() && brd_bbp_prepare_restart()) {
            esp_restart();
        }
        brd_tft_update();
        delay(BATTERY_LOW_LOOP_DELAY_MS);
        return;
    }
    /* [V1.2 修改] OTA 持有量測 pause，不重新處理 GPIO / RPM 事件。 */
    if (!brd_ota_is_busy()) {
        brd_measurement_begin();
        brd_measurement_update();
    }
    /* [新增 V1.4] 僅當發射已完成，處理原始記錄並建立 64/32 點曲線。
       量測期間此函式立即返回，不執行抽樣、TFT 或 BLE 封包產生。 */
    if (!brd_ota_is_busy()) { brd_shot::publish_finished(); }
    /* [V1.2 新增] 量測安全窗口建立後，再處理 OTA 入列請求。 */
    brd_ota_update();
    /* [PRO 新增] OTA request 可能在本次迴圈開始，更新 IR 電源狀態。 */
    brd_io_set_ir_enabled(!brd_ota_is_busy());
    /* [保留] 量測事件優先，其次 BLE，最後 TFT Dirty Region 更新。 */
    bool ota_busy = brd_ota_is_busy();
    bool storage_allowed = !ota_busy && brd_measurement_storage_allowed();
    /* [新增] 充電策略只關閉 BLE，量測與本機歷史仍持續運作。 */
    if (!ota_busy && !brd_settings_bluetooth_allowed(brd_io_is_charging())) {
        brd_bbp_stop();
    } else if (storage_allowed) {
        brd_bbp_begin();
    }
    /* [新增 V1.4] RPM ISR Queue 累積超過 1/4 時先排空，BLE 與 TFT 讓路。
       不能直接丟棄事件、不能為曲線取樣暫停 ISR。 */
    if (!ota_busy && brd_measurement_rpm_backlog() >= RPM_ISR_QUEUE_SIZE / 4U) {
        brd_measurement_update();
        return;
    }
    brd_bbp_update(storage_allowed);
    /* [新增] GET / 通知仍運作；修改指令由設定模組檢查安全窗口。 */
    brd_settings_ble_update();
    /* [新增] 只在量測結束或閒置時保存；保存成功後不需重啟。 */
    /* [V1.2 修改] OTA 期間只回覆設定 GET / BUSY，不執行 NVS 保存。 */
    if (!ota_busy) { brd_settings_update(); }
    /* [新增 V1.4] BLE 操作後再檢查待處理 RPM；必要時延後 LCD 動畫。 */
    if (!ota_busy && brd_measurement_rpm_backlog() >= RPM_ISR_QUEUE_SIZE / 4U) {
        brd_measurement_update();
        return;
    }
    brd_tft_update();
    delay(MAIN_LOOP_DELAY_MS);
}
