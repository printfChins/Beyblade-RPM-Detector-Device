/*
    檔案名稱: brd_types.h
    檔案位置: BLE_RPM_V1.0/brd_types.h

    共用資料型別。
*/

#ifndef BRD_TYPES_H
#define BRD_TYPES_H

#include <Arduino.h>

typedef struct {
    uint16_t time_ms;
    uint16_t rpm;
} rpm_curve_sample_t;

typedef enum {
    BRD_STATE_WAIT_LOAD = 0,
    BRD_STATE_LOADED_READY,
    BRD_STATE_SPINNING_LOADED,
    BRD_STATE_SPINNING_LAUNCHED,
    BRD_STATE_RESULT_PENDING
} brd_state_t;

#endif
