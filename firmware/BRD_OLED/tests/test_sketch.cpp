/* [新增] 連結完整主程式與所有模組，確認低電/ADC 故障停機及恢復。 */
#include <cassert>
#include <cstdio>
#include <Arduino.h>
#include <esp_err.h>
#include <soc/gpio_struct.h>
uint64_t test_clock_us = 100000U;
void (*test_interrupts[32])(void) = {};
TestGPIO GPIO = {};
unsigned test_restart_count = 0U;
esp_err_t test_adc_error = ESP_OK;
int test_adc_voltage_mv = 2000;
#include "../BRD_OLED.ino"

static void check_boot(void) {
    setup();
    assert(brd_battery_get_voltage_mv() == 4000U);
    assert(brd_battery_get_percent() == 80U);
    assert(brd_battery_measurement_allowed());
    assert(test_interrupts[RPM_IR_GPIO] != nullptr);
    assert((test_interrupts[LOAD_IR_GPIO] != nullptr) ==
           (BRD_MEASUREMENT_MODE == BRD_MEASUREMENT_MODE_LOAD));
}

static void check_adc_fault_recovery(void) {
    test_adc_error = ESP_FAIL;
    test_clock_us += BATTERY_ADC_STALE_TIMEOUT_MS * 1000U;
    loop();
    assert(brd_battery_is_adc_fault());
    assert(test_interrupts[RPM_IR_GPIO] == nullptr);
    assert(test_interrupts[LOAD_IR_GPIO] == nullptr);
    test_adc_error = ESP_OK;
    test_clock_us += BATTERY_SAMPLE_INTERVAL_MS * 1000U;
    loop();
    assert(brd_battery_measurement_allowed());
    assert(test_interrupts[RPM_IR_GPIO] != nullptr);
}

static void check_low_battery_and_restart(void) {
    test_adc_voltage_mv = 1650;
    test_clock_us += BATTERY_SAMPLE_INTERVAL_MS * 1000U;
    loop();
    assert(brd_battery_is_low_locked());
    assert(!brd_battery_measurement_allowed());
    assert(test_interrupts[RPM_IR_GPIO] == nullptr);
    test_adc_voltage_mv = 1800;
    for (unsigned i = 0U; i < 4U; i++) {
        test_clock_us += BATTERY_SAMPLE_INTERVAL_MS * 1000U;
        loop();
    }
    assert(brd_battery_restart_required());
    assert(test_restart_count == 1U);
}

static void check_low_battery_boot(void) {
    test_adc_voltage_mv = 1700;
    uint64_t before = test_clock_us;
    setup();
    assert(brd_battery_is_low_locked());
    assert(test_interrupts[RPM_IR_GPIO] == nullptr);
    assert(test_interrupts[LOAD_IR_GPIO] == nullptr);
    assert(test_clock_us == before); /* 低電開機略過開機等待。 */
}

int main(void) {
    check_boot();
    check_adc_fault_recovery();
    check_low_battery_and_restart();
    check_low_battery_boot();
    printf("PASS: %s complete sketch link, battery-first boot, ADC recovery and low-battery restart.\n",
           BRD_MEASUREMENT_MODE == BRD_MEASUREMENT_MODE_LOAD ? "LOAD" : "AUTO");
}
