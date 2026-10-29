/*
 * 檔案位置：BRD_BPX_PRO/brd_shot_capture.cpp
 * [新增 V1.4] 量測期間只寫入所有有效的正/負緣完整單圈原始數值。
 * [新增] 發射結束後才由主迴圈壓縮：TFT 64 點 / BLE 32 點，強制包含 MAX。
 * [保留] BLE BBP 封包、Flash 格式不改；不在 ISR 分配記憶體或執行取樣。
 * [限制] 緩衝固定容量且可檢查 overflow，不宣稱無限筆紀錄。
 */
#include "brd_shot_capture.h"

#include <stddef.h>
#include "brd_config.h"
#include "brd_bbp.h"
#include "brd_bbp_protocol.h"
#include "brd_ui_curve.h"

namespace brd_shot {
namespace {

/* [新增] 每筆 8 bytes：完整週期(us)、當時精確計算之 RPM、相對時間與極性。
 * 時間欄位 bit0 是極性，其餘 15 bits 是距第一點的毫秒數，最多 32.767 秒。
 * 峰值 RPM 由量測原值決定，不會受到 BLE 8us 量化影響。
 */
struct Sample {
    uint32_t period_us;
    uint16_t rpm;
    uint16_t time_edge;
};
static_assert(sizeof(Sample) == 8U, "Unexpected raw RPM sample size.");
static_assert(SHOT_RAW_SAMPLE_CAPACITY <= 65535U, "Sample index must fit uint16_t.");

static Sample g_samples[SHOT_RAW_SAMPLE_CAPACITY];
static uint16_t g_count = 0U;
static uint32_t g_dropped = 0U;
static uint32_t g_start_us = 0U;
static uint16_t g_measured_max = 0U;
static uint16_t g_edge_count[2] = {};
static uint16_t g_edge_peak[2] = {};
static uint16_t g_edge_peak_rank[2] = {};
static uint16_t g_global_peak_index = 0U;
static bool g_truncated = false;
static bool g_started = false;
static bool g_pending = false;
static bool g_finalized = false;

/* [新增] 使用等距序號抽樣以保留曲線頭尾及整段資料，避免高速只取前 32 圈。 */
static uint16_t spaced_index(uint16_t size, uint8_t out_count, uint8_t slot) {
    if (out_count < 2U) { return 0U; }
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(slot) * (size - 1U) + (out_count - 1U) / 2U) /
        (out_count - 1U));
}

/* [新增] 必須包含 MAX；以峰值取代距離最近且不破壞時間順序的代表點。 */
static void pin_max(uint16_t *indices, uint8_t size, uint16_t max_index) {
    if (size == 0U) { return; }
    for (uint8_t i = 0U; i < size; ++i) {
        if (indices[i] == max_index) { return; }
    }
    uint8_t best = 0U;
    uint16_t best_distance = UINT16_MAX;
    for (uint8_t i = 0U; i < size; ++i) {
        if ((i > 0U && indices[i - 1U] >= max_index) ||
            (i + 1U < size && indices[i + 1U] <= max_index)) {
            continue;
        }
        uint16_t distance = indices[i] > max_index ?
            static_cast<uint16_t>(indices[i] - max_index) :
            static_cast<uint16_t>(max_index - indices[i]);
        if (distance < best_distance) {
            best = i;
            best_distance = distance;
        }
    }
    if (best_distance != UINT16_MAX) { indices[best] = max_index; }
}

static void build_tft_curve() {
    const uint8_t n = g_count > UI_CURVE_POINTS ? UI_CURVE_POINTS : static_cast<uint8_t>(g_count);
    uint16_t indices[UI_CURVE_POINTS] = {};
    uint16_t rpm[UI_CURVE_POINTS] = {};
    for (uint8_t i = 0U; i < n; ++i) {
        indices[i] = (n == g_count) ? i : spaced_index(g_count, n, i);
    }
    if (n > 0U) { pin_max(indices, n, g_global_peak_index); }
    for (uint8_t i = 0U; i < n; ++i) {
        rpm[i] = g_samples[indices[i]].rpm;
    }
    brd_ui::curve_publish(rpm, n, g_measured_max);
}

static void build_ble_profile() {
    /* [保留] 選擇包含全程最大 RPM 的單圈極性，平手時取點較多者，再取正緣。 */
    const uint8_t winning = g_edge_peak[0] > g_edge_peak[1] ? 0U :
        (g_edge_peak[1] > g_edge_peak[0] ? 1U :
         (g_edge_count[0] > g_edge_count[1] ? 0U : 1U));
    const uint16_t count = g_edge_count[winning];
    const uint8_t n = count > brd_bbp::CURVE_SIZE ? brd_bbp::CURVE_SIZE : static_cast<uint8_t>(count);
    uint16_t ranks[brd_bbp::CURVE_SIZE] = {};
    for (uint8_t i = 0U; i < n; ++i) {
        ranks[i] = n == count ? i : spaced_index(count, n, i);
    }
    if (n > 0U) { pin_max(ranks, n, g_edge_peak_rank[winning]); }

    uint16_t rank = 0U;
    uint8_t written = 0U;
    for (uint16_t i = 0U; i < g_count && written < n; ++i) {
        const Sample &s = g_samples[i];
        if ((s.time_edge & 1U) != winning) { continue; }
        if (rank == ranks[written]) {
            /* [新增] 原始完整週期，不將半圈誤當一圈。 */
            const uint32_t event_us = g_start_us + (static_cast<uint32_t>(s.time_edge >> 1U) * 1000UL);
            brd_bbp_capture_period(s.period_us, event_us, winning);
            ++written;
        }
        ++rank;
    }
    /* [新增] 保留量測計算的真正最高值，不受縮點與 BBP 量化影響。 */
    brd_bbp_capture_set_max(g_measured_max);
    brd_bbp_capture_finish();
}

} // namespace

void reset() {
    g_count = 0U;
    g_dropped = 0U;
    g_start_us = 0U;
    g_measured_max = 0U;
    g_edge_count[0] = g_edge_count[1] = 0U;
    g_edge_peak[0] = g_edge_peak[1] = 0U;
    g_edge_peak_rank[0] = g_edge_peak_rank[1] = 0U;
    g_global_peak_index = 0U;
    g_truncated = false;
    g_started = false;
    g_pending = false;
    g_finalized = false;
}

void record(uint32_t period_us, uint32_t event_us, uint8_t edge_level, uint16_t rpm) {
    if (g_finalized || g_pending || edge_level > 1U || period_us == 0U || rpm == 0U) {
        return;
    }
    if (!g_started) {
        g_start_us = event_us;
        g_started = true;
    }
    const uint32_t elapsed_ms = static_cast<uint32_t>(event_us - g_start_us) / 1000UL;
    if (g_count >= SHOT_RAW_SAMPLE_CAPACITY || elapsed_ms > 32767UL) {
        /* [防護] 記憶體滿不覆寫已記錄資料；獨立統計溢位而不影響 RPM/MAX。 */
        g_truncated = true;
        if (g_dropped != UINT32_MAX) { ++g_dropped; }
        return;
    }
    const uint16_t edge_rank = g_edge_count[edge_level];
    if (rpm > g_edge_peak[edge_level]) {
        g_edge_peak[edge_level] = rpm;
        g_edge_peak_rank[edge_level] = edge_rank;
    }
    if (rpm > g_measured_max) {
        g_measured_max = rpm;
        g_global_peak_index = g_count;
    }
    g_samples[g_count++] = {period_us, rpm,
        static_cast<uint16_t>((elapsed_ms << 1U) | edge_level)};
    ++g_edge_count[edge_level];
}

void finish(uint16_t measured_max_rpm) {
    if (!g_started || g_count == 0U || g_finalized) { return; }
    /* [新增] 測速 MAX 為唯一權威，取樣壓縮不能覆蓋它。 */
    g_measured_max = measured_max_rpm;
    g_pending = true;
}

void abort() {
    reset();
}

bool publish_finished() {
    if (!g_pending || g_finalized) { return false; }
    g_pending = false;
    g_finalized = true;
    build_tft_curve();
    build_ble_profile();
    return true;
}

Diagnostics diagnostics() {
    Diagnostics d;
    d.recorded = g_count;
    d.dropped = g_dropped;
    d.max_rpm = g_measured_max;
    d.truncated = g_truncated;
    d.pending = g_pending;
    return d;
}

} // namespace brd_shot
