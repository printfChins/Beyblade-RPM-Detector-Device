/*
    檔案位置: BRD_OLED_V0.10/BRD_OLED_V0.10.ino
    [V0.10 修改] 以附件 BLE_RPM_V1.9 為來源，建立獨立單機 OLED 版本。
    貼放位置: 本檔為新專案完整主程式，從第 1 行使用，不貼入舊版 .ino。

    [V0.10 刪減]
    BLE/NimBLE、曲線封包、ACK、低功耗與 Serial 初始化。

    [V0.10 保留]
    CPU 80 MHz、GPIO3 FALLING RPM、GPIO1 CHANGE LOAD、OLED 即時 RPM/MAX。
    [V0.10 恢復] GPIO0 電池量測、OLED 電量與 GPIO10 充電圖示。
    [V0.10 新增] 電量 < 5% 停止量測並警示，恢復至 >= 10% 後重新啟動。
    GPIO20/21 專供 OLED I2C；不啟動可能占用相同腳位的 UART。
*/
/* [V0.10 新增] 電量恢復後以 ESP-IDF 的重啟 API 重新執行開機流程。 */
#include <esp_system.h>

/* [V0.10 新增] 電量功能的標頭檔。 */
#include "brd_battery.h"
#include "brd_config.h"
#include "brd_io.h"
#include "brd_measurement.h"
#include "brd_oled.h"

void setup(void) {
    setCpuFrequencyMhz(CPU_FIXED_FREQ_MHZ);
    brd_io_begin();
    /* [V0.10 新增] 放在 GPIO 初始化之後、OLED 初始化之前，取得第一筆電量。 */
    brd_battery_begin();

    /* [修改] 開機即低電時直接顯示警示，不播放版本畫面、不啟動量測中斷。 */
    brd_oled_begin();

    /* [新增] 版本畫面等待結束後再次更新 ADC，避免低電時仍啟動量測。 */
    brd_battery_update();
    if (brd_battery_is_low_locked()) {
        brd_measurement_stop();
    } else {
        brd_measurement_begin();
    }
}

void loop(void) {
    /* [修改] 每秒單次 ADC；低電判斷優先於 RPM、LOAD 與 MAX 自鎖。 */
    brd_battery_update();

    if (brd_battery_is_low_locked()) {
        /* [新增] 只在首次進入時清資料及解除中斷，後續呼叫保持停用狀態。 */
        brd_measurement_stop();
        if (brd_battery_restart_required()) {
            /* [新增] 電量 >= 10% 時重新開機，不從原量測接續執行。 */
            esp_restart();
        }
        brd_oled_update();
        delay(BATTERY_LOW_LOOP_DELAY_MS);
        return;
    }

    /* [保留] 正常模式先量測，OLED 一次最多送出一個一般畫面封包。 */
    brd_measurement_update();
    brd_oled_update();
    delay(MAIN_LOOP_DELAY_MS);
}
