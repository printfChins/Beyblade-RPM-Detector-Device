/*
    檔案：BRD_BPX/tests/test_load_hold_v123.cpp
    [V1.2.3 新增] 使用真實 LOAD/RPM 量測 + BBP Session 驗證五階段流程。
    [驗證] 30% 等號立即結束、HOLD 先停 ISR、3 秒不檢查 LOAD、不清 MAX；
           HOLD 才封存 MAX/曲線；結束後 WAIT LOAD -> LOW -> HIGH 才歸零。
*/
#include <cassert>
#include <cstdio>
#include <Arduino.h>
#include <soc/gpio_struct.h>
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_settings.h"
#include "brd_bbp_session.h"

uint64_t test_clock_us = 100000ULL;
void (*test_interrupts[32])(void) = {};
TestGPIO GPIO = {};
static brd_bbp::Session session;
static unsigned finish_calls = 0U;

const brd_settings::Values &brd_settings_get() {
    static const brd_settings::Values value = {BRD_MEASUREMENT_MODE_LOAD, 0U, 30U, 2000U};
    return value;
}
void brd_bbp_capture_reset() { session.reset_capture(); }
void brd_bbp_capture_period(uint32_t period, uint32_t time, bool record) {
    session.period(period, time, record);
}
void brd_bbp_capture_launch(uint32_t time) { session.launched(time); }
void brd_bbp_capture_finish() {
    /* [V1.2.3 驗證] 進入 HOLD 並禁止 ISR 後，才能凍結結果。 */
    assert(brd_measurement_get_display().hold_active);
    ++finish_calls;
    session.finish();
}
void brd_bbp_capture_abort() { session.abort(); }

static void tick(uint32_t us) {
    test_clock_us += us;
    brd_measurement_update();
}
static void load(int level, uint32_t us) {
    test_clock_us += us;
    GPIO.in.val = (GPIO.in.val & ~(1U << LOAD_IR_GPIO)) |
                  (static_cast<uint32_t>(level) << LOAD_IR_GPIO);
    if (test_interrupts[LOAD_IR_GPIO]) { test_interrupts[LOAD_IR_GPIO](); }
    brd_measurement_update();
}
static void rpm(uint32_t us) {
    test_clock_us += us;
    GPIO.in.val |= 1U << RPM_IR_GPIO;
    if (test_interrupts[RPM_IR_GPIO]) { test_interrupts[RPM_IR_GPIO](); }
    brd_measurement_update();
}
static void shot_10000_then_3000() {
    rpm(100U);
    rpm(6000U); // 10000 SP
    assert(!brd_measurement_get_display().show_max);
    assert(!session.pending());
    assert(session.state().total == 0U);
    rpm(20000U); // exactly 10000 x 30% = 3000 SP
    assert(brd_measurement_get_display().show_max);
    assert(brd_measurement_get_display().value == 10000U);
    assert(brd_measurement_get_display().hold_active);
}
int main() {
    static_assert(LOAD_MAX_HOLD_MS == 2500UL, "LOAD HOLD must match original V1.2 2.5 seconds");
    brd_measurement_begin();
    assert(!brd_measurement_get_display().loaded);
    assert(!brd_measurement_get_display().show_max);
    rpm(100U); rpm(6000U); // WAIT LOAD ignores RPM
    assert(!brd_measurement_get_display().show_max);
    load(HIGH, 1000U);
    tick(LOAD_IR_DEBOUNCE_US-1U);
    assert(!brd_measurement_get_display().loaded);
    tick(1U);
    assert(brd_measurement_get_display().loaded);
    shot_10000_then_3000();
    assert(finish_calls == 1U);
    assert(session.pending());
    brd_display_t frame = brd_measurement_get_display();
    brd_measurement_max_frame_presented(frame.generation);
    /* [驗證] HOLD 期間的 LOAD LOW/HIGH 彈跳及 RPM 不可清 MAX/增加歷史。 */
    const uint32_t load_changes_before_hold_bounce =
        brd_measurement_get_diagnostics().load_stable_transitions;
    load(LOW, 10000U);
    load(HIGH, 10000U);
    rpm(5000U); rpm(6000U); rpm(20000U);
    assert(brd_measurement_get_diagnostics().load_stable_transitions ==
           load_changes_before_hold_bounce);
    tick(600000U);
    session.update(micros(), BBP_PUBLISH_MIN_DELAY_US); // HOLD 內才發布
    assert(session.state().total == 1U);
    assert(session.state().lifetime_max == 10000U);
    assert(session.state().curve[0] != 0U);
    assert(finish_calls == 1U);
    assert(brd_measurement_get_display().hold_active);
    /* [驗證] HOLD 鎖定時長相對 MAX 畫面完成，不是從 IR 事件起算。 */
    tick(1800000U); // [V1.4.2 修改] 約 2.45 秒，仍在 HOLD
    assert(brd_measurement_get_display().hold_active);
    tick(100000U); // [V1.4.2 修改] 超過 2.5 秒，HOLD 結束
    assert(!brd_measurement_get_display().hold_active);
    assert(!brd_measurement_get_display().loaded); // WAIT LOAD despite physical HIGH
    assert(brd_measurement_get_display().show_max);
    assert(brd_measurement_get_display().value == 10000U);
    rpm(100U); rpm(6000U); rpm(20000U);
    assert(finish_calls == 1U);
    assert(brd_measurement_get_display().show_max);
    load(LOW, 100U); tick(LOAD_IR_DEBOUNCE_US-1000U);
    load(HIGH, 100U); tick(LOAD_IR_DEBOUNCE_US+1000U);
    assert(!brd_measurement_get_display().loaded);
    assert(brd_measurement_get_display().show_max);
    load(LOW, 100U); tick(LOAD_IR_DEBOUNCE_US);
    assert(!brd_measurement_get_display().loaded);
    assert(brd_measurement_get_display().show_max);
    load(HIGH, 100U); tick(LOAD_IR_DEBOUNCE_US);
    assert(brd_measurement_get_display().loaded);
    assert(!brd_measurement_get_display().show_max); // only real LOADED clears old MAX
    assert(brd_measurement_get_display().value == 0U);
    rpm(100U); rpm(6000U); rpm(20000U);
    assert(finish_calls == 2U);
    assert(session.state().total == 1U); // second record is not published until HOLD
    assert(brd_measurement_get_display().hold_active);
    tick(600000U);
    session.update(micros(), BBP_PUBLISH_MIN_DELAY_US);
    assert(session.state().total == 2U);
    puts("PASS: LOAD -> SP -> MAX threshold equality -> HOLD 2.5s / BLE record -> WAIT LOAD -> LOADED reset.");
}
