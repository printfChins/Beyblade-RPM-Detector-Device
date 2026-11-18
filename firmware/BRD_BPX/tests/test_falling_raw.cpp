/*
    檔案: BRD_BPX/tests/test_falling_raw.cpp
    [新增] 驗證 GPIO3 僅在裝載後裝上 FALLING ISR；無濾波即時 RPM/MAX；停止後才進 BBP。
    [新增] 驗證完整原始筆數、固定 RAM 上限診斷、micros 回繞及 32 點曲線。
*/
#include <cassert>
#include <cstdio>
#include <cstring>
#include <Arduino.h>
#include <soc/gpio_struct.h>
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_settings.h"
#include "brd_bbp_session.h"

uint64_t test_clock_us = 1000000ULL;
void (*test_interrupts[32])(void) = {};
TestGPIO GPIO = {};
static brd_settings::Values g_values = {BRD_MEASUREMENT_MODE_LOAD, 0U, 30U, 2000U,
                                       BATTERY_CAPACITY_300_MAH, OLED_MAX_HOLD_MS};
static brd_bbp::Session g_session;
static uint32_t g_session_period_calls = 0U;
static uint32_t g_finish_calls = 0U;

const brd_settings::Values &brd_settings_get() { return g_values; }
void brd_bbp_capture_reset() { g_session.reset_capture(); }
void brd_bbp_capture_period(uint32_t us, uint32_t event_us, bool profile) {
    ++g_session_period_calls;
    g_session.period(us, event_us, profile);
}
void brd_bbp_capture_launch(uint32_t event_us) { g_session.launched(event_us); }
void brd_bbp_capture_finish() { ++g_finish_calls; g_session.finish(); }
void brd_bbp_capture_abort() { g_session.abort(); }

static void advance(uint32_t us) {
    test_clock_us += us;
    brd_measurement_update();
}
static void falling(uint32_t us, bool update = true) {
    test_clock_us += us;
    GPIO.in.val &= ~(1U << RPM_IR_GPIO);
    assert(test_interrupts[RPM_IR_GPIO] != nullptr);
    test_interrupts[RPM_IR_GPIO]();
    /* [驗證] 上升緣實際不觸發 ISR；只改腳位回復 HIGH。 */
    GPIO.in.val |= (1U << RPM_IR_GPIO);
    if (update) { brd_measurement_update(); }
}
static void set_load(int level) {
    GPIO.in.val = (GPIO.in.val & ~(1U << LOAD_IR_GPIO)) |
                  (static_cast<uint32_t>(level) << LOAD_IR_GPIO);
    assert(test_interrupts[LOAD_IR_GPIO] != nullptr);
    test_interrupts[LOAD_IR_GPIO]();
    brd_measurement_update();
}
static void reset_mode(uint8_t mode) {
    brd_measurement_stop();
    g_values.mode = mode;
    GPIO.in.val = (1U << RPM_IR_GPIO);
    brd_measurement_begin();
}
static void load_and_arm() {
    assert(test_interrupts[RPM_IR_GPIO] == nullptr);
    assert(test_interrupts[LOAD_IR_GPIO] != nullptr);
    set_load(HIGH);
    advance(LOAD_IR_DEBOUNCE_US);
    assert(brd_measurement_get_display().loaded);
    assert(test_interrupts[RPM_IR_GPIO] != nullptr);
}
static void check_load_direct_finish() {
    reset_mode(BRD_MEASUREMENT_MODE_LOAD);
    load_and_arm();
    const uint32_t saved_before = g_session_period_calls;
    const uint32_t periods_before = brd_measurement_get_diagnostics().raw_periods_seen;
    falling(100U);                    // 第一下降緣：僅起點
    assert(brd_measurement_capture_active());
    assert(test_interrupts[LOAD_IR_GPIO] == nullptr);
    assert(brd_measurement_get_display().value == 0U);
    falling(5000U);                  // 12000 RPM
    assert(brd_measurement_get_display().value == 12000U);
    falling(4000U);                  // 15000 RPM，立即更新
    assert(brd_measurement_get_display().value == 15000U);
    falling(5000U);                  // 12000 RPM，MAX 不下降
    assert(brd_measurement_get_display().value == 12000U);
    assert(g_session_period_calls == saved_before); // 採樣中完全不進 BBP Session
    assert(brd_measurement_get_diagnostics().raw_periods_seen == periods_before + 3U);
    falling(16000U);                 // 3750 RPM <= MAX*30%，首次下降即停
    assert(brd_measurement_get_display().hold_active);
    assert(brd_measurement_get_display().value == 15000U);
    assert(test_interrupts[RPM_IR_GPIO] == nullptr);
    assert(g_session_period_calls == saved_before + 5U); // MAX + 四個真實週期
    assert(g_finish_calls > 0U);
    assert(g_session.pending());
    assert(g_session.state().total == 0U); // 尚未發布，BLE 主迴圈才處理
    g_session.update(micros(), 0U);
    assert(g_session.state().total == 1U);
    assert(g_session.state().lifetime_max == 15000U);
    /* MAX 15000 對應 4000us/8=500；原始四筆曲線按發射時序保存。 */
    assert(g_session.state().curve[0] == 625U);
    assert(g_session.state().curve[1] == 500U);
    assert(g_session.state().curve[2] == 625U);
    assert(g_session.state().curve[3] == 2000U);
}
static void check_raw_unfiltered_and_wrap() {
    reset_mode(BRD_MEASUREMENT_MODE_AUTO);
    assert(test_interrupts[RPM_IR_GPIO] != nullptr);
    assert(test_interrupts[LOAD_IR_GPIO] == nullptr);
    test_clock_us = (1ULL << 32U) - 2500U; // 使後續 Falling Edge 跨越 32-bit micros()
    falling(100U);
    falling(5000U);    // 12000RPM
    falling(1000U);    // 60000RPM：不做 5 點中位數、直接變更 MAX
    assert(brd_measurement_get_display().value == 60000U);
    falling(20000U);   // 3000RPM < 60000*30%，立即結束
    assert(brd_measurement_get_display().hold_active);
    assert(brd_measurement_get_display().value == 60000U);
    g_session.update(micros(), 0U);
    assert(g_session.state().lifetime_max == 60000U);
}
static void check_capture_capacity_and_queue_overflow() {
    const uint32_t previous_total = g_session.state().total;
    reset_mode(BRD_MEASUREMENT_MODE_AUTO);
    const uint32_t overflows = brd_measurement_get_diagnostics().raw_capture_overflows;
    const uint32_t queue_overflows = brd_measurement_get_diagnostics().rpm_queue_overflows;
    falling(100U);
    /* [驗證] 主迴圈阻塞造成 Queue Overflow 時僅重建 Falling 基準，不偽造 RPM。 */
    for (uint32_t index = 0U; index < RPM_ISR_QUEUE_SIZE + 7U; ++index) {
        falling(2000U, false);
    }
    brd_measurement_update();
    assert(brd_measurement_get_diagnostics().rpm_queue_overflows == queue_overflows + 1U);
    assert(!brd_measurement_get_display().hold_active);
    falling(2000U); // 溢位後重新建立週期時間點
    falling(2000U); // 恢復每圈一次即時值
    assert(brd_measurement_get_display().value == 30000U);
    const uint32_t initial = brd_measurement_get_diagnostics().raw_periods_seen;
    /* [驗證] 超出可用 RAM 上限仍然持續更新 MAX / RPM，資料不會越界。 */
    for (uint32_t index = 0U; index < RPM_RAW_CAPTURE_CAPACITY + 5U; ++index) {
        falling(2000U);
    }
    assert(brd_measurement_get_diagnostics().raw_periods_seen ==
           initial + RPM_RAW_CAPTURE_CAPACITY + 5U);
    assert(brd_measurement_get_diagnostics().raw_capture_overflows > overflows);
    assert(brd_measurement_get_display().value == 30000U);
    falling(10000U); // 6000RPM < 30000*30%，發射完成
    assert(brd_measurement_get_display().hold_active);
    assert(brd_measurement_get_display().value == 30000U);
    g_session.update(micros(), 0U);
    assert(g_session.state().total == previous_total + 1U);
    const auto &saved = g_session.state();
    assert(saved.records[(saved.next + brd_bbp::HISTORY_SIZE - 1U) % brd_bbp::HISTORY_SIZE] == 30000U);
    for (uint8_t index = 0U; index < brd_bbp::CURVE_SIZE; ++index) {
        assert(g_session.state().curve[index] != 0U);
    }
}
int main() {
    static_assert(RPM_IR_TRIGGER_EDGE == FALLING, "RPM must configure FALLING only");
    check_load_direct_finish();
    check_raw_unfiltered_and_wrap();
    check_capture_capacity_and_queue_overflow();
    puts("PASS: FALLING-only, load arming, raw RPM/MAX, immediate stop, deferred curve, wrap, RAM cap and queue overflow.");
}
