/* [新增] 真正執行量測模組；抓出 AUTO 未在下降門檻立即結束。 */
#include <cassert>
#include <Arduino.h>
#include <soc/gpio_struct.h>
#include "brd_config.h"
#include "brd_measurement.h"
#include "brd_settings.h"
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
TestGPIO GPIO = {};
const brd_settings::Values &brd_settings_get() {
    static const brd_settings::Values value = {1U,1U,30U,2000U,BATTERY_CAPACITY_200_MAH, OLED_MAX_HOLD_MS};
    return value;
}
static unsigned launches = 0U;
void brd_bbp_capture_reset() {}
void brd_bbp_capture_period(uint32_t, uint32_t, bool) {}
void brd_bbp_capture_launch(uint32_t) { launches++; }
void brd_bbp_capture_finish() {}
void brd_bbp_capture_abort() {}
static void pulse(uint32_t delta) {
    test_clock_us += delta;
    GPIO.in.val |= 1U << RPM_IR_GPIO;
    test_interrupts[RPM_IR_GPIO]();
    brd_measurement_update();
}
int main() {
    brd_measurement_begin();
    pulse(100U);
    // [V1.4.5 修改] 單點低值不得結束，持續低於百分比仍須完成。
    for (unsigned i = 0U; i < 5U; ++i) { pulse(5000U); }
    pulse(20000U);
    assert(!brd_measurement_get_display().show_max);
    for (unsigned i = 0U; i < 3U; ++i) { pulse(20000U); }
    const brd_display_t display = brd_measurement_get_display();
    if (!display.show_max || display.value != 12000U || launches != 1U) {
        fprintf(stderr, "FAIL: AUTO must finish after confirmed low periods; show_max=%u value=%u launches=%u\n",
                display.show_max, display.value, launches);
        return 1;
    }
    puts("PASS: AUTO finishes on common MAX ratio.");
}
