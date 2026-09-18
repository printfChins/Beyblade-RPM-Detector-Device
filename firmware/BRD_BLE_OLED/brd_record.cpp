/*
    檔案位置: BRD_BLE_OLED/brd_record.cpp
    [V1.11 修改] 刪除舊 Web 50 ms 曲線 buffer，只保留 Reliable Protocol 的真實 RPM 事件曲線。
    [V1.11 新增] 每筆曲線固定以 t=0 / 0 RPM 起始，並納入 A3 CRC32。
    固定 RAM，不在 ISR、NimBLE callback 或量測期間配置 heap。
*/
#include "brd_config.h"
#include "brd_record.h"

static brd_record_info_t g_record = {};
static uint32_t g_record_start_us = 0UL;
static uint32_t g_record_sequence = 0UL;
static bool g_record_time_capped = false;
static brd_sample_t g_event_samples[BLE_EVENT_MAX_SAMPLES];
static brd_event_info_t g_event_info = {};

static void record_put_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static uint32_t record_crc_byte(uint32_t crc, uint8_t value) {
    crc ^= value;
    for (uint8_t bit = 0U; bit < 8U; bit++) {
        crc = (crc >> 1) ^ ((crc & 1UL) ? 0xEDB88320UL : 0UL);
    }
    return crc;
}

void brd_record_reset(void) {
    uint32_t next_epoch = g_record.epoch + 1UL;
    g_record = {};
    g_record.epoch = next_epoch;
    g_record.launch_sample_index = CURVE_INVALID_INDEX;
    g_record_time_capped = false;
    g_event_info = {};
    g_event_info.launch_sample_index = CURVE_INVALID_INDEX;
}

void brd_record_begin(uint32_t start_us) {
    brd_record_reset();
    g_record_sequence++;
    if (g_record_sequence == 0UL) {
        g_record_sequence = 1UL;
    }
    g_record.result_id = g_record_sequence;
    g_record.active = true;
    g_record_start_us = start_us;

    /* [V1.11 新增] Reliable 曲線第一筆固定為起始 0 RPM。 */
    g_event_samples[0] = {0U, 0U};
    g_event_info.count = 1U;
    g_record.count = 1U;
}

void brd_record_advance(uint32_t time_us, uint16_t rpm, bool include_equal) {
    (void)rpm;
    (void)include_equal;
    if (!g_record.active || g_record_time_capped) {
        return;
    }
    uint32_t elapsed_us = (uint32_t)(time_us - g_record_start_us);
    uint32_t elapsed_ms = elapsed_us / 1000UL;
    if (elapsed_ms > CURVE_MAX_DURATION_MS) {
        g_record.truncated = true;
        g_event_info.truncated = true;
        g_record_time_capped = true;
        elapsed_ms = CURVE_MAX_DURATION_MS;
    }
    g_record.duration_ms = (uint16_t)elapsed_ms;
    g_event_info.duration_ms = (uint16_t)elapsed_ms;
}

void brd_record_launch(uint32_t time_us, uint16_t rpm, uint16_t max_rpm) {
    if (!g_record.active) {
        return;
    }
    uint32_t elapsed_ms = (uint32_t)(time_us - g_record_start_us) / 1000UL;
    g_record.launch_rpm = rpm;
    g_record.max_at_launch = max_rpm;
    if (g_record_time_capped || elapsed_ms > CURVE_MAX_DURATION_MS) {
        g_record.truncated = true;
        g_event_info.truncated = true;
        return;
    }
    g_record.launch_time_ms = (uint16_t)elapsed_ms;
    g_record.launch_valid = true;
    /* [V1.11 修改] 發射當下立即鎖定最接近原 LOW 時間的既有事件索引。 */
    uint32_t best_distance = UINT32_MAX;
    for (uint16_t i = 0U; i < g_event_info.count; i++) {
        uint32_t sample_time = g_event_samples[i].time_ms;
        uint32_t distance = sample_time > elapsed_ms ? sample_time - elapsed_ms : elapsed_ms - sample_time;
        if (distance < best_distance) {
            best_distance = distance;
            g_event_info.launch_sample_index = i;
        }
    }
    g_record.launch_sample_index = g_event_info.launch_sample_index;
}

void brd_record_capture_event(uint32_t time_us, uint16_t rpm, bool is_max) {
    if (!g_record.active || g_record_time_capped) {
        return;
    }
    uint32_t elapsed = (uint32_t)(time_us - g_record_start_us) / 1000UL;
    if (elapsed > CURVE_MAX_DURATION_MS || g_event_info.count >= BLE_EVENT_MAX_SAMPLES) {
        g_event_info.truncated = true;
        g_record.truncated = true;
        return;
    }
    g_event_samples[g_event_info.count++] = {(uint16_t)elapsed, rpm};
    g_record.count = g_event_info.count;
    g_event_info.duration_ms = (uint16_t)elapsed;
    g_record.duration_ms = (uint16_t)elapsed;
    if (is_max) {
        g_event_info.max_time_ms = (uint16_t)elapsed;
    }
}

brd_event_info_t brd_record_get_event_info(void) {
    return g_event_info;
}

bool brd_record_get_event_sample(uint16_t index, brd_sample_t &sample) {
    if (index >= g_event_info.count) {
        return false;
    }
    sample = g_event_samples[index];
    return true;
}

void brd_record_finish(uint32_t time_us, uint16_t rpm, uint16_t max_rpm) {
    if (!g_record.active) {
        return;
    }
    brd_record_advance(time_us, rpm, true);
    uint32_t elapsed_ms;
    if (g_record_time_capped) {
        /* [V1.11 修正] 超過 60 秒後即使 micros 後續回繞，也維持封頂時間。 */
        elapsed_ms = CURVE_MAX_DURATION_MS;
    } else {
        elapsed_ms = (uint32_t)(time_us - g_record_start_us) / 1000UL;
        if (elapsed_ms > CURVE_MAX_DURATION_MS) {
            elapsed_ms = CURVE_MAX_DURATION_MS;
            g_record.truncated = true;
            g_event_info.truncated = true;
        }
    }
    g_record.duration_ms = (uint16_t)elapsed_ms;
    g_event_info.duration_ms = (uint16_t)elapsed_ms;
    g_record.max_rpm = max_rpm;
    g_record.active = false;
    g_record.ready = true;

    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t best_distance = UINT32_MAX;
    for (uint16_t i = 0U; i < g_event_info.count; i++) {
        uint8_t bytes[4];
        record_put_u16(bytes, g_event_samples[i].time_ms);
        record_put_u16(bytes + 2U, g_event_samples[i].rpm);
        for (uint8_t byte : bytes) {
            crc = record_crc_byte(crc, byte);
        }
        if (g_record.launch_valid) {
            uint32_t sample_time = g_event_samples[i].time_ms;
            uint32_t launch_time = g_record.launch_time_ms;
            uint32_t distance = sample_time > launch_time ?
                                sample_time - launch_time : launch_time - sample_time;
            if (distance < best_distance) {
                best_distance = distance;
                g_event_info.launch_sample_index = i;
            }
        }
    }
    g_event_info.crc32 = crc ^ 0xFFFFFFFFUL;
    g_record.crc32 = g_event_info.crc32;
    g_record.count = g_event_info.count;
    g_record.launch_sample_index = g_event_info.launch_sample_index;
    g_record.truncated = g_record.truncated || g_event_info.truncated;
}

void brd_record_mark_gap(void) {
    if (g_record.active) {
        g_record.truncated = true;
        g_event_info.truncated = true;
    }
}

brd_record_info_t brd_record_get_info(void) {
    return g_record;
}
