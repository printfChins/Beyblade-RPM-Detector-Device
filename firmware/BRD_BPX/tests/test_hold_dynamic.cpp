/* [V1.4.4 新增] 真實 AUTO 量測狀態機確認 BLE 設定 4000ms HOLD。 */
#include <cassert>
#include <cstdio>
#include <Arduino.h>
#include <soc/gpio_struct.h>
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_settings.h"
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
TestGPIO GPIO = {};
const brd_settings::Values &brd_settings_get() {
    static const brd_settings::Values value = {BRD_MEASUREMENT_MODE_AUTO,0U,30U,2000U,
                                               BATTERY_CAPACITY_250_MAH,4000U};
    return value;
}
void brd_bbp_capture_reset() {}
void brd_bbp_capture_period(uint32_t, uint32_t, bool) {}
void brd_bbp_capture_launch(uint32_t) {}
void brd_bbp_capture_finish() {}
void brd_bbp_capture_abort() {}
static void pulse(uint32_t us) {
    test_clock_us += us;
    GPIO.in.val &= ~(1U << RPM_IR_GPIO);
    assert(test_interrupts[RPM_IR_GPIO] != nullptr);
    test_interrupts[RPM_IR_GPIO]();
    brd_measurement_update();
}
int main() {
    brd_measurement_begin();
    pulse(100U);
    // [V1.4.5 修改] 使用持續高轉／下降，HOLD 時間斷言保留。
    for (unsigned i = 0U; i < 5U; ++i) { pulse(5000U); }
    for (unsigned i = 0U; i < 4U; ++i) { pulse(20000U); }
    assert(brd_measurement_get_display().hold_active);
    test_clock_us += 3999ULL * 1000ULL;
    brd_measurement_update();
    assert(brd_measurement_get_display().hold_active);
    test_clock_us += 2ULL * 1000ULL;
    brd_measurement_update();
    assert(!brd_measurement_get_display().hold_active);
    assert(brd_measurement_get_display().show_max);
    puts("PASS: saved 4000ms runtime HOLD controls AUTO without changing MAX history.");
}
