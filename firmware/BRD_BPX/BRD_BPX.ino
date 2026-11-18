/*
    檔案位置: BRD_BPX/BRD_BPX.ino
    貼放位置: 完整主程式，Arduino IDE 開啟此檔，同資料夾保留全部 .cpp/.h。
    [BRD_BBP 修改] 由 BRD_OLED V0.12 整合 BattlePass BLE 周邊端。
    [新增] 電池檢查及開機畫面後才初始化 BLE，再啟動量測。
    [修改] 80MHz、GPIO3 FALLING；Falling-to-Falling 原始 RPM 即時計算與完整 RAM 緩存。
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
/* [V1.4.4 新增] BLE 名稱 NVS 啟動與安全窗口存檔。 */
#include "brd_ble_identity.h"
void setup(void) {
    /* [新增] 所有硬體與量測啟動前，先載入 Flash 設定。 */
    brd_settings_begin();
    brd_ble_identity_begin(); /* [V1.4.4 新增] 首次使用預設 BEYBLADE_TOOL01。 */
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
    /* [V1.2 修改] OTA 持有量測 pause，不重新處理 GPIO / RPM 事件。 */
    if (!brd_ota_is_busy()) {
        brd_measurement_begin();
        brd_measurement_update();
    }
    /*
        [修改] RPM 量測為最高優先：進行中完全略過主 loop 的 OTA/BBP Notify/設定傳輸/NVS。
        不強制 BLE 斷線，僅延後應用層傳輸；電池保護與 OLED 仍維持運作。
    */
    if (!brd_measurement_capture_active()) {
        brd_ota_update();
        const bool ota_busy = brd_ota_is_busy();
        const bool storage_allowed = !ota_busy && brd_measurement_storage_allowed();
        if (!ota_busy && !brd_settings_bluetooth_allowed(brd_io_is_charging())) {
            brd_bbp_stop();
        } else if (storage_allowed) {
            brd_bbp_begin();
        }
        brd_bbp_update(storage_allowed);
        brd_settings_ble_update();
        if (!ota_busy) {
            brd_settings_update();
            if (!brd_settings_model().pending()) { brd_ble_identity_update(); }
        }
    }
    brd_oled_update();
    delay(MAIN_LOOP_DELAY_MS);
}
