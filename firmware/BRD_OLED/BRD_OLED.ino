/*
    檔案位置: BRD_OLED/BRD_OLED.ino
    貼放位置: 獨立新增主程式；Arduino IDE 開啟本檔，同資料夾保留所有 .cpp/.h。
    [新增] BRD_OLED V1.1 單機 OLED 轉速版本。
    [刪減] 無線初始化、廣播、連線、通訊更新、紀錄及 Flash 設定流程。
    [保留] 電池優先檢查、OLED 開機畫面、80MHz、LOAD/AUTO 量測及低電恢復。
*/
#include <esp_system.h>
#include "brd_battery.h"
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
        brd_measurement_begin();
    } else {
        brd_measurement_stop();
    }
}

void loop(void) {
    brd_battery_update();
    if (!brd_battery_measurement_allowed()) {
        brd_measurement_stop();
        /* [修改] 單機版沒有待保存通訊資料，電量恢復穩定後可直接重啟。 */
        if (brd_battery_restart_required()) {
            esp_restart();
        }
        brd_oled_update();
        delay(BATTERY_LOW_LOOP_DELAY_MS);
        return;
    }
    brd_measurement_begin();
    /* [保留] 先處理量測事件，再進行一個 OLED I2C 傳送。 */
    brd_measurement_update();
    brd_oled_update();
    delay(MAIN_LOOP_DELAY_MS);
}
