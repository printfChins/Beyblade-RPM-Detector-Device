/*
    檔案名稱: brd_measurement.h
    檔案位置: BLE_RPM_V1.0/brd_measurement.h

    GPIO0 RPM、GPIO1 裝載/發射、量測狀態機與曲線記錄模組介面。
*/

#ifndef BRD_MEASUREMENT_H
#define BRD_MEASUREMENT_H

#include <Arduino.h>

void brd_measurement_begin(void);

void brd_load_poll_update(void);
void brd_rpm_poll_update(void);
void brd_rpm_zero_update_task(void);
void brd_curve_record_update_task(void);
void brd_measurement_stop_update_task(void);

bool brd_load_is_active(void);
bool brd_measurement_is_active(void);
uint16_t brd_measurement_elapsed_ms(void);

void brd_enter_wait_load_state(void);
void brd_enter_loaded_ready_state(void);
void brd_measurement_reset_after_result_send(void);

#endif
