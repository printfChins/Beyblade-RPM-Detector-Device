/*
    檔案位置: BRD_BBPX/BRD_BBPX.ino
    貼放位置: 完整主程式，Arduino IDE 開啟此檔，同資料夾保留全部 .cpp/.h。
    [BRD_BBPX 修改] 由 BRD_OLED V0.12 整合 BattlePass BLE 周邊端。
    [新增] 電池檢查及開機畫面後才初始化 BLE，再啟動量測。
    [保留] 80MHz、GPIO3 FALLING 一圈、原 LOAD 去抖、20% 結束、2.5秒 MAX 自鎖。
    [刪減] 不帶入原 OLED build 與 bin，避免誤燒舊韌體。
*/
#include <esp_system.h>
#include "brd_battery.h"
#include "brd_bbp.h"
#include "brd_config.h"
#include "brd_io.h"
#include "brd_measurement.h"
#include "brd_oled.h"
void setup(void) {
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
    if (storage_allowed) {
        brd_bbp_begin();
    }
    brd_bbp_update(storage_allowed);
    brd_oled_update();
    delay(MAIN_LOOP_DELAY_MS);
}
