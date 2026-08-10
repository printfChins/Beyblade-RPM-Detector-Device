/*
    檔案名稱: brd_context.h
    檔案位置: BLE_RPM_V1.0/brd_context.h

    集中管理跨模組共用的執行狀態，避免在多個 .cpp 使用大量獨立 extern 變數。
*/

#ifndef BRD_CONTEXT_H
#define BRD_CONTEXT_H

#include <Arduino.h>

#include "brd_config.h"
#include "brd_types.h"

typedef struct {
    volatile bool ble_connected;
    char ble_device_name[BLE_DEVICE_NAME_MAX_LEN];
    uint32_t last_live_notify_ms;
    bool launch_event_pending;

    uint32_t rpm_last_poll_us;
    uint32_t rpm_last_raw_change_us;
    int rpm_raw_level;
    int rpm_stable_level;

    uint32_t load_last_poll_us;
    uint32_t load_last_raw_change_us;
    int load_raw_level;
    int load_stable_level;

    brd_state_t brd_state;

    uint32_t last_trigger_us;
    uint32_t last_accepted_edge_ms;
    uint32_t valid_edge_count;

    uint16_t current_rpm;
    uint16_t max_rpm;
    bool has_valid_rpm;

    rpm_curve_sample_t curve_buffer[MAX_RECORD_SAMPLES];
    uint16_t curve_count;

    uint32_t record_start_ms;
    uint32_t last_sample_ms;

    bool launch_mark_valid;
    uint16_t launch_rpm;
    uint16_t launch_time_ms;
    uint16_t launch_sample_index;
    uint32_t launch_absolute_ms;

    bool stop_hold_active;
    uint32_t stop_hold_start_ms;

    bool charge_state_initialized;
    bool charging;

    bool led_trigger_output_on;
    uint32_t led_last_trigger_ms;

    uint32_t log_last_ms;
    uint32_t log_last_edge_count;
} brd_context_t;

brd_context_t &brd_context_get(void);
void brd_context_reset(void);

#endif
