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
/* [V1.2 新增] 放在 setup() 前的 include 區。 */
#include "brd_ota.h"
/* [新增] 讓三組 BLE 服務共用每輪傳送額度。 */
#include "brd_scheduler.h"
void setup(void) {
    /* [新增] 所有硬體與量測啟動前，先載入 Flash 設定。 */
    brd_settings_begin();
    setCpuFrequencyMhz(CPU_FIXED_FREQ_MHZ);
    brd_io_begin();
    brd_battery_begin();
    brd_oled_begin();
    brd_battery_update();
    if (brd_battery_measurement_allowed()) {
        /* [修改] BLE 初始化前先建立原生 IRAM 擷取；初始化後立即處理事件。 */
        brd_measurement_begin();
        brd_bbp_begin();
        brd_measurement_update();
        brd_battery_update();
    }
    if (brd_battery_measurement_allowed()) {
        brd_measurement_begin();
    } else {
        brd_measurement_stop();
        brd_bbp_stop();
    }
}
/* [新增] 放在 loop() 前。每個背景操作之間優先服務 RPM。 */
static void service_rpm(void) {
    if (!brd_ota_is_busy() && brd_battery_measurement_allowed()) {
        brd_measurement_begin();
        brd_measurement_update();
    }
}
/* [修改] 原 loop() 完整替換：RPM > OLED > OTA/BBP/設定/保存。 */
void loop(void) {
    brd_scheduler_begin_cycle();
    service_rpm();
    brd_battery_update();
    if (!brd_battery_measurement_allowed()) {
        brd_measurement_stop();
        brd_bbp_stop();
        if (brd_battery_restart_required() && brd_bbp_prepare_restart()) { esp_restart(); }
        brd_oled_update();
        delay(BATTERY_LOW_LOOP_DELAY_MS);
        return;
    }
    service_rpm();
    brd_oled_update();
    service_rpm();
    brd_ota_update();
    service_rpm();
    bool ota_busy = brd_ota_is_busy();
    bool storage_allowed = !ota_busy && brd_measurement_storage_allowed();
    if (!ota_busy && !brd_settings_bluetooth_allowed(brd_io_is_charging())) {
        brd_bbp_stop();
    } else if (storage_allowed) {
        brd_bbp_begin();
    }
    service_rpm();
    brd_bbp_update(storage_allowed);
    service_rpm();
    brd_settings_ble_update();
    service_rpm();
    if (!ota_busy) { brd_settings_update(); }
    service_rpm();
    delay(MAIN_LOOP_DELAY_MS);
}
