/* [新增] 實際執行量測模組，GPIO 與時間僅由主機測試環境提供。 */
#include <cassert>
#include <cstdio>
#include <Arduino.h>
#include <soc/gpio_struct.h>
#include "brd_config.h"
#include "brd_measurement.h"

uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
TestGPIO GPIO = {};

static void tick(uint32_t us) {
    test_clock_us += us;
    brd_measurement_update();
}

static void edge(uint32_t delta, int level = HIGH, bool update = true) {
    test_clock_us += delta;
    GPIO.in.val = (GPIO.in.val & ~(1U << RPM_IR_GPIO)) |
                  (static_cast<uint32_t>(level) << RPM_IR_GPIO);
    if (test_interrupts[RPM_IR_GPIO] != nullptr) {
        test_interrupts[RPM_IR_GPIO]();
    }
    if (update) {
        brd_measurement_update();
    }
}

static void load_level(int level) {
    GPIO.in.val = (GPIO.in.val & ~(1U << LOAD_IR_GPIO)) |
                  (static_cast<uint32_t>(level) << LOAD_IR_GPIO);
    if (test_interrupts[LOAD_IR_GPIO] != nullptr) {
        test_interrupts[LOAD_IR_GPIO]();
    }
    brd_measurement_update();
}

static void fresh(bool ready = true) {
    brd_measurement_stop();
    test_clock_us = 100000U;
    GPIO.in.val = 0U;
    brd_measurement_begin();
    assert(test_interrupts[RPM_IR_GPIO] != nullptr);
    if (BRD_MEASUREMENT_MODE == BRD_MEASUREMENT_MODE_LOAD) {
        assert(test_interrupts[LOAD_IR_GPIO] != nullptr);
        if (ready) {
            load_level(HIGH);
            tick(LOAD_IR_DEBOUNCE_US);
            assert(brd_measurement_get_display().loaded);
        }
    } else {
        assert(test_interrupts[LOAD_IR_GPIO] == nullptr);
    }
}

static void check_ratio_and_hold(void) {
    fresh();
    edge(100U);
    edge(6000U);   /* 10000 RPM。 */
    edge(20000U);  /* 3000 RPM，等於 MAX 的 30%，不可結束。 */
    assert(!brd_measurement_get_display().show_max);
    edge(25000U);  /* 2400 RPM，嚴格低於 30% 才結束。 */
    brd_display_t display = brd_measurement_get_display();
    assert(display.show_max && display.hold_active && display.value == 10000U);
    const uint32_t generation = display.generation;
    tick(500000U);
    brd_measurement_max_frame_presented(generation + 1U);
    brd_measurement_max_frame_presented(generation);
    /* [修正] HOLD 使用 millis()；以實際毫秒期限測試前一微秒。 */
    const uint64_t hold_deadline_us =
        (test_clock_us / 1000U + OLED_MAX_HOLD_MS) * 1000U;
    tick(static_cast<uint32_t>(hold_deadline_us - test_clock_us - 1U));
    assert(brd_measurement_get_display().hold_active);
    edge(100U);
    assert(brd_measurement_get_display().show_max);
    assert(brd_measurement_get_display().value == 10000U);
    assert(!brd_measurement_get_display().hold_active);
    if (BRD_MEASUREMENT_MODE == BRD_MEASUREMENT_MODE_LOAD) {
        tick(LOAD_IR_DEBOUNCE_US);
    }
    edge(100U);
    edge(5000U);
    assert(!brd_measurement_get_display().show_max);
    assert(brd_measurement_get_display().value == 12000U);
}

static void check_no_pulse_and_repeated_begin(void) {
    fresh();
    edge(100U);
    edge(5000U);
    brd_measurement_begin();
    assert(brd_measurement_get_display().value == 12000U);
    tick(RPM_ZERO_TIMEOUT_MS * 1000U - 1U);
    assert(!brd_measurement_get_display().show_max);
    tick(1U);
    assert(brd_measurement_get_display().show_max);
    assert(brd_measurement_get_display().value == 12000U);
    brd_measurement_stop();
    assert(test_interrupts[RPM_IR_GPIO] == nullptr);
    assert(test_interrupts[LOAD_IR_GPIO] == nullptr);
    assert(brd_measurement_get_display().value == 0U);
}

static void check_polarity_wrap_and_late_peak(void) {
    fresh();
    test_clock_us = UINT32_MAX - 4095ULL;
    edge(100U,LOW);
    edge(5000U,HIGH);
    assert(brd_measurement_get_display().value == 0U);
    edge(5000U,LOW); /* 跨 micros 回繞後仍為 6000 RPM。 */
    for (unsigned i = 0U; i < 40U; i++) {
        edge(5000U,HIGH);
        edge(5000U,LOW);
        assert(brd_measurement_get_display().value == 6000U);
    }
    edge(2000U,LOW);
    assert(brd_measurement_get_display().value == 30000U);
    edge(10000U,LOW);
    assert(brd_measurement_get_display().show_max);
    assert(brd_measurement_get_display().value == 30000U);
}

static void check_queue_overflow(void) {
    fresh();
    edge(100U,LOW);
    edge(10000U,LOW);
    uint32_t before = brd_measurement_get_diagnostics().rpm_queue_overflows;
    for (unsigned i = 0U; i < RPM_ISR_QUEUE_SIZE + 5U; i++) {
        edge(2000U,LOW,false);
    }
    brd_measurement_update();
    assert(brd_measurement_get_diagnostics().rpm_queue_overflows == before + 1U);
    assert(!brd_measurement_get_display().show_max);
    assert(brd_measurement_get_display().value == 30000U);
    edge(1000U,HIGH);
    edge(1000U,LOW);
    edge(2000U,LOW);
    assert(brd_measurement_get_display().value == 30000U);
    edge(10000U,LOW);
    assert(brd_measurement_get_display().show_max);
    assert(brd_measurement_get_display().value == 30000U);
}

static void check_mode_specific_conditions(void) {
    fresh(false);
    if (BRD_MEASUREMENT_MODE == BRD_MEASUREMENT_MODE_LOAD) {
        edge(100U);
        edge(5000U);
        assert(brd_measurement_get_display().value == 0U);
        load_level(HIGH);
        tick(LOAD_IR_DEBOUNCE_US - 1U);
        assert(!brd_measurement_get_display().loaded);
        tick(1U);
        assert(brd_measurement_get_display().loaded);
        edge(100U);
        edge(5000U);
        load_level(LOW);
        for (unsigned i = 0U; i < 10U; i++) {
            edge(5000U);
        }
        assert(!brd_measurement_get_display().loaded);
        assert(brd_measurement_get_display().value == 12000U);
        edge(4000U);
        assert(brd_measurement_get_display().value == 15000U);
        edge(20000U);
        assert(brd_measurement_get_display().show_max);
        assert(brd_measurement_get_display().value == 15000U);
    } else {
        edge(100U);
        edge(40000U); /* 1500 RPM，未達 AUTO 門檻。 */
        load_level(HIGH);
        tick(AUTO_RESET_ZERO_MS * 1000U);
        assert(!brd_measurement_get_display().show_max);
        assert(brd_measurement_get_display().value == 0U);
        edge(100U);
        edge(30000U); /* 2000 RPM，達 AUTO 門檻。 */
        load_level(LOW);
        tick(RPM_ZERO_TIMEOUT_MS * 1000U);
        assert(brd_measurement_get_display().show_max);
        assert(brd_measurement_get_display().value == 2000U);
    }
}

int main(void) {
    check_ratio_and_hold();
    check_no_pulse_and_repeated_begin();
    check_polarity_wrap_and_late_peak();
    check_queue_overflow();
    check_mode_specific_conditions();
    printf("PASS: %s ratio, MAX hold/rearm, no-pulse, 30000 RPM, polarity, wrap and overflow.\n",
           BRD_MEASUREMENT_MODE == BRD_MEASUREMENT_MODE_LOAD ? "LOAD" : "AUTO");
}
