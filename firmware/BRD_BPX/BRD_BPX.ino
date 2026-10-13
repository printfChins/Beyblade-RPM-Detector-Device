/*
    檔案位置: BRD_BPX/BRD_BPX.ino
    貼放位置: 完整主程式，Arduino IDE 開啟此檔，同資料夾保留全部 .cpp/.h。
    [BRD_BBP 修改] 由 BRD_OLED V0.12 整合 BattlePass BLE 周邊端。
    [新增] 電池檢查及開機畫面後才初始化 BLE，再啟動量測。
    [修改] 80MHz、GPIO3 CHANGE；每次量測由先觸發的正/負緣固定為同極性單圈參考。
    [保留] LOAD 去抖預設 50 ms、2.5 秒 MAX 自鎖。
    [修改] AUTO / LOAD 共用可設定的 MAX 百分比結束，預設 30%。
    [刪減] 不帶入原 OLED build 與 bin，避免誤燒舊韌體。
*/
#include <esp_system.h>
#include "brd_battery.h"
#include "brd_bbp.h"
#include "brd_config.h"
#include "brd_io.h"
#include "brd_measurement.h"
#include "brd_oled.h"
/* [新增] 主程式設定模組入口。 */
#include "brd_settings.h"
#include "brd_settings_ble.h"
void setup(void) {
    /* [新增] 所有硬體與量測啟動前，先載入 Flash 設定。 */
    brd_settings_begin();
    setCpuFrequencyMhz(CPU_FIXED_FREQ_MHZ);
    brd_io_begin();
    brd_battery_begin();
    brd_oled_begin();
    brd_battery_update();
    if (brd_battery_measurement_allowed()) {
        brd_bbp_begin();
        brd_battery_update();
    }
    if (brd_battery_measurement_allowed()) {
        brd_measurement_begin();
    } else {
        brd_measurement_stop();
        brd_bbp_stop();
    }
}
void loop(void) {
    brd_battery_update();
    if (!brd_battery_measurement_allowed()) {
        brd_measurement_stop();
        brd_bbp_stop();
        /* [修正] SOC 恢復穩定後先保存 dirty 紀錄，成功才重啟，避免清除被撤回。 */
        if (brd_battery_restart_required() && brd_bbp_prepare_restart()) {
            esp_restart();
        }
        brd_oled_update();
        delay(BATTERY_LOW_LOOP_DELAY_MS);
        return;
    }
    brd_measurement_begin();
    /* [保留] 量測事件優先，其次 BLE，最後 OLED 一個區塊。 */
    brd_measurement_update();
    bool storage_allowed = brd_measurement_storage_allowed();
    /* [新增] 充電策略只關閉 BLE，量測與本機歷史仍持續運作。 */
    if (!brd_settings_bluetooth_allowed(brd_io_is_charging())) {
        brd_bbp_stop();
    } else if (storage_allowed) {
        brd_bbp_begin();
    }
    brd_bbp_update(storage_allowed);
    /* [新增] GET / 通知仍運作；修改指令由設定模組檢查安全窗口。 */
    brd_settings_ble_update();
    /* [新增] 只在量測結束或閒置時保存；保存成功後不需重啟。 */
    brd_settings_update();
    brd_oled_update();
    delay(MAIN_LOOP_DELAY_MS);
}
