/*
    檔案位置: BRD_BLE_OLED/BRD_BLE_OLED.ino
    貼放位置: 本檔是新專案完整主程式，從第 1 行使用，不貼入原 BRD_OLED。
    [V0.12 新增] BLE 即時 RPM、發射摘要、曲線與可選 ACK / 重傳。
    [V0.12 修改] 低電 / ADC 故障同時停止量測與 BLE。
    [V1.11 修改] 保留 GPIO / 去抖 / ADC / OLED HOLD；停止門檻改為 MAX 35%。
    [V1.15 修改] 移除 0x81/C5/state_seq；B1 LIVE 每 200 ms 持續同步狀態，無 protocol 主動斷線。
    不啟用 Serial / UART，GPIO20/21 保留給 OLED I2C。
*/
#include <esp_system.h>

#include "brd_battery.h"
#include "brd_ble.h"
#include "brd_config.h"
#include "brd_io.h"
#include "brd_measurement.h"
#include "brd_oled.h"
// [V1.15 修改] 待機休眠已移除；保留 power 介面只確保 OLED 常亮。
#include "brd_power.h"

void setup(void) {
    setCpuFrequencyMhz(CPU_FIXED_FREQ_MHZ);
    brd_io_begin();
    brd_battery_begin();
    /* [V1.15 保留] 只有真正上電才播放開機版本畫面；其他 reset 直接進主畫面。 */
    brd_oled_begin(esp_reset_reason() == ESP_RST_POWERON);
    brd_battery_update();
    if (!brd_battery_measurement_allowed()) {
        brd_measurement_stop();
        brd_ble_set_enabled(false);
    } else {
        /* [新增] BLE 初始化完成後才啟動量測捕捉，避免初始化時堆積事件。 */
        brd_ble_set_enabled(true);
        brd_battery_update();
        if (brd_battery_measurement_allowed()) {
            brd_measurement_begin();
        } else {
            brd_ble_set_enabled(false);
            brd_measurement_stop();
        }
    }
    // [V1.15 修改] 不建立待機計時；確保 OLED 維持啟用。
    brd_power_begin();
}

void loop(void) {
    brd_battery_update();
    if (!brd_battery_measurement_allowed()) {
        // [V1.15 修改] 低電流程不再涉及待機休眠。
        brd_power_begin();
        brd_measurement_stop();
        brd_ble_set_enabled(false);
        if (brd_battery_restart_required()) {
            esp_restart();
        }
        brd_oled_update();
        delay(BATTERY_LOW_LOOP_DELAY_MS);
        return;
    }

    brd_ble_set_enabled(true);
    /* [新增] BLE 故障重試／恢復初始化可能耗時，量測前再次核對電量新鮮度。 */
    brd_battery_update();
    if (!brd_battery_measurement_allowed()) {
        brd_power_begin();
        brd_measurement_stop();
        brd_ble_set_enabled(false);
        brd_oled_update();
        delay(BATTERY_LOW_LOOP_DELAY_MS);
        return;
    }
    brd_measurement_begin();
    /* [修改] 每輪先處理 RPM / LOAD 及曲線，再各送一個 BLE / OLED 封包。 */
    brd_measurement_update();
    brd_ble_update();
    // [V1.15 修改] 待機休眠已移除；此介面只維持 OLED 啟用。
    brd_power_update();
    brd_oled_update();
    delay(MAIN_LOOP_DELAY_MS);
}
