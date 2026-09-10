/*
    檔案位置: BRD_OLED_V0.10/brd_battery.h
    [V0.10 恢復] GPIO0 電池量測與 OLED 電量資料介面。
    所有函式均由 setup / 主 loop 呼叫，不在 ISR 內執行。
*/
#ifndef BRD_BATTERY_H
#define BRD_BATTERY_H

#include <Arduino.h>

void brd_battery_begin(void);
void brd_battery_update(void);
uint16_t brd_battery_get_voltage_mv(void);
uint8_t brd_battery_get_percent(void);
/* [V0.10 新增] 本次運行中維持低電鎖定；達到恢復門檻後由主程式重新開機。 */
bool brd_battery_is_low_locked(void);
bool brd_battery_restart_required(void);

#endif
