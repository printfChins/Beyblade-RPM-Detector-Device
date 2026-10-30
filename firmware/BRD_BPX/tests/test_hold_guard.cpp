/*
    檔案位置：BRD_BPX/tests/test_hold_guard.cpp
    [V1.2.2 新增] 以真實 brd_measurement.cpp + Session 驗證 HOLD 與 LOAD。
    [目標] MAX 自鎖期間不得清零；自鎖解除後必須真正 LOW 50ms 再 HIGH 50ms。
    [不修改] RPM 換算、BBP 協定與電池演算法。
*/
#include <cassert>
#include <cstdio>
#include <Arduino.h>
#include <soc/gpio_struct.h>
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_settings.h"
#include "brd_bbp_session.h"

uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
TestGPIO GPIO = {};
static brd_bbp::Session g_session;
static uint32_t g_finished = 0U;

const brd_settings::Values &brd_settings_get() {
    static const brd_settings::Values values = {BRD_MEASUREMENT_MODE_LOAD, 0U, 30U, 2000U};
    return values;
}
void brd_bbp_capture_reset() { g_session.reset_capture(); }
void brd_bbp_capture_period(uint32_t period_us, uint32_t event_us, bool record_profile) {
    g_session.period(period_us, event_us, record_profile);
}
void brd_bbp_capture_launch(uint32_t time_us) { g_session.launched(time_us); }
void brd_bbp_capture_finish() { ++g_finished; g_session.finish(); }
void brd_bbp_capture_abort() { g_session.abort(); }

static void tick(uint32_t delta_us) {
    test_clock_us += delta_us;
    brd_measurement_update();
}
static void load_level(int level, uint32_t delta_us) {
    test_clock_us += delta_us;
    GPIO.in.val = (GPIO.in.val & ~(1U << LOAD_IR_GPIO)) | (static_cast<uint32_t>(level) << LOAD_IR_GPIO);
    if (test_interrupts[LOAD_IR_GPIO]) { test_interrupts[LOAD_IR_GPIO](); }
    brd_measurement_update();
}
static void rpm_edge(uint32_t delta_us) {
    test_clock_us += delta_us;
    GPIO.in.val |= (1U << RPM_IR_GPIO);
    if (test_interrupts[RPM_IR_GPIO]) { test_interrupts[RPM_IR_GPIO](); }
    brd_measurement_update();
}
static void shot() {
    rpm_edge(100U);
    rpm_edge(5000U);   // 12000 RPM
    rpm_edge(20000U);  // 3000 RPM < 12000*30%; finish
}
static void start_loaded() {
    brd_measurement_stop();
    GPIO.in.val &= ~((1U << RPM_IR_GPIO) | (1U << LOAD_IR_GPIO));
    brd_measurement_begin();
    load_level(HIGH, 1000U);
    tick(LOAD_IR_DEBOUNCE_US);
    assert(brd_measurement_get_display().loaded);
}
static void verify_second_shot_when_genuinely_reloaded() {
    const uint32_t original_finish = g_finished;
    const uint32_t original_launch = brd_measurement_get_diagnostics().launch_events;
    shot();
    assert(g_finished == original_finish+1U);
    assert(brd_measurement_get_diagnostics().launch_events == original_launch+1U);
}
static void test_held_high_and_bounce() {
    start_loaded();
    verify_second_shot_when_genuinely_reloaded();
    assert(brd_measurement_get_display().hold_active);
    const brd_display_t frame=brd_measurement_get_display();
    brd_measurement_max_frame_presented(frame.generation);
    // 原始 LOAD 彈跳：HOLD 尚未完成，任何 GPIO1 事件不得歸零。
    load_level(LOW, 10000U);
    load_level(HIGH, 10000U);
    rpm_edge(100U);
    tick(1000000U);
    load_level(LOW, 10000U);
    load_level(HIGH, 10000U);
    assert(brd_measurement_get_display().hold_active);
    assert(brd_measurement_get_display().show_max);
    assert(brd_measurement_get_display().value == 12000U);
    assert(g_finished == 1U);

    tick(LOAD_MAX_HOLD_MS * 1000U + 1U);  // [V1.4.2 修改] LOAD HOLD 2.5 秒到期
    /* [V1.2.2 測試] 正常 BLE loop 已發布第一發；下一發不得覆寫尚未發布的 Session。 */
    g_session.update(micros(), BBP_PUBLISH_MIN_DELAY_US);
    assert(g_session.state().total == 1U);
    assert(!brd_measurement_get_display().hold_active);
    assert(brd_measurement_get_display().show_max);
    // 未解除裝載（一直 HIGH）：必須始終保留 MAX。
    tick(LOAD_IR_DEBOUNCE_US + 50000U);
    rpm_edge(100U); rpm_edge(5000U); rpm_edge(20000U);
    assert(brd_measurement_get_display().show_max);
    assert(g_finished == 1U);
    // LOW 短暫彈跳不足 50ms，再回 HIGH，仍不允許下一發。
    load_level(LOW, 100U);
    tick(LOAD_IR_DEBOUNCE_US - 1000U);
    load_level(HIGH, 100U);
    tick(LOAD_IR_DEBOUNCE_US + 1000U);
    assert(brd_measurement_get_display().show_max);
    assert(g_finished == 1U);
    // 真正 LOW 穩定 50ms：只解除下一發資格鎖，不能立刻清除 MAX。
    load_level(LOW, 100U);
    tick(LOAD_IR_DEBOUNCE_US);
    assert(brd_measurement_get_display().show_max);
    assert(!brd_measurement_get_display().hold_active);
    assert(g_finished == 1U);
    // 新 HIGH 連續 50ms 代表真正新裝載，才允許歸零。
    load_level(HIGH, 100U);
    tick(LOAD_IR_DEBOUNCE_US);
    assert(!brd_measurement_get_display().show_max);
    verify_second_shot_when_genuinely_reloaded();
    assert(g_finished == 2U);
    g_session.update(micros() + BBP_PUBLISH_MIN_DELAY_US, BBP_PUBLISH_MIN_DELAY_US);
    assert(g_session.state().total == 2U);
    puts("PASS: HOLD guards load bounce, held HIGH cannot rearm, real LOW->HIGH permits only one new shot.");
}
static void test_load_already_low_after_hold() {
    const uint32_t base = g_finished;
    start_loaded();
    verify_second_shot_when_genuinely_reloaded();
    brd_display_t frame=brd_measurement_get_display();
    brd_measurement_max_frame_presented(frame.generation);
    // 在 HOLD 期間已經卸載，HOLD 到期時 LOW 保持不變，不會再產生下降沿。
    load_level(LOW, 10000U);
    tick(LOAD_MAX_HOLD_MS*1000U + 1U);
    assert(brd_measurement_get_display().show_max);
    tick(LOAD_IR_DEBOUNCE_US + 1U);
    assert(brd_measurement_get_display().show_max);
    // LOW 期間必須維持 MAX，直到新的 HIGH 穩定。
    load_level(HIGH, 200U);
    tick(LOAD_IR_DEBOUNCE_US + 1U);
    assert(!brd_measurement_get_display().show_max);
    shot();
    assert(g_finished == base + 2U);
    puts("PASS: LOW already present when HOLD expires is confirmed without requiring a new falling edge.");
}
int main() {
    test_held_high_and_bounce();
    test_load_already_low_after_hold();
    puts("PASS: V1.2.2 HOLD regression / MAX-history / new-load tests.");
}
