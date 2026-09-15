/*
    檔案位置: BRD_BLE_OLED/brd_record.h
    [V1.11 修改] 僅保留 Reliable Protocol 真實 RPM 事件曲線。
*/
#ifndef BRD_RECORD_H
#define BRD_RECORD_H

#include <Arduino.h>

struct brd_sample_t {
    uint16_t time_ms;
    uint16_t rpm;
};

struct brd_record_info_t {
    uint32_t epoch;
    uint32_t result_id;
    uint32_t crc32;
    uint16_t count;
    uint16_t duration_ms;
    uint16_t max_rpm;
    uint16_t launch_rpm;
    uint16_t max_at_launch;
    uint16_t launch_time_ms;
    uint16_t launch_sample_index;
    bool active;
    bool ready;
    bool launch_valid;
    bool truncated;
};

struct brd_event_info_t {
    uint16_t count;
    uint16_t duration_ms;
    uint16_t max_time_ms;
    uint16_t launch_sample_index;
    uint32_t crc32;
    bool truncated;
};

void brd_record_reset(void);
void brd_record_begin(uint32_t start_us);
void brd_record_advance(uint32_t time_us, uint16_t rpm, bool include_equal);
void brd_record_launch(uint32_t time_us, uint16_t rpm, uint16_t max_rpm);
void brd_record_finish(uint32_t time_us, uint16_t rpm, uint16_t max_rpm);
void brd_record_mark_gap(void);
brd_record_info_t brd_record_get_info(void);
void brd_record_capture_event(uint32_t time_us, uint16_t rpm, bool is_max);
brd_event_info_t brd_record_get_event_info(void);
bool brd_record_get_event_sample(uint16_t index, brd_sample_t &sample);

#endif
