/* BRD_BPX_PRO/tests/test_rpm_adc.cpp
 * [新增] GPIO4 ADC 原始值快取、讀取錯誤與逾時行為測試。
 */
#include <cassert>
#include <cstdio>
#include <Arduino.h>
#include "brd_config.h"
#include "brd_rpm_adc.h"
#include "brd_adc_shared.h"
uint64_t test_clock_us = 100000U;
static int raw_value = 2012;
static bool fault = false;
static int reads = 0;
namespace brd_adc {
esp_err_t configure(int gpio, adc_atten_t, adc_bitwidth_t) {
    assert(gpio == IR_RPM_ADC_GPIO);
    return fault ? ESP_ERR_INVALID_STATE : ESP_OK;
}
esp_err_t read_raw(int gpio, int &raw) {
    assert(gpio == IR_RPM_ADC_GPIO);
    ++reads;
    raw = raw_value;
    return fault ? ESP_ERR_INVALID_RESPONSE : ESP_OK;
}
}
int main() {
    brd_rpm_adc_begin();
    brd_rpm_adc_update();
    assert(brd_rpm_adc_valid() && brd_rpm_adc_raw() == 2012U && reads == 1);
    raw_value = 3200;
    brd_rpm_adc_update();
    assert(brd_rpm_adc_raw() == 2012U && reads == 1);
    test_clock_us += IR_RPM_ADC_SAMPLE_INTERVAL_MS * 1000ULL;
    brd_rpm_adc_update();
    assert(brd_rpm_adc_raw() == 3200U && reads == 2);
    fault = true;
    test_clock_us += IR_RPM_ADC_SAMPLE_INTERVAL_MS * 1000ULL;
    brd_rpm_adc_update();
    assert(brd_rpm_adc_valid());
    test_clock_us += IR_RPM_ADC_STALE_TIMEOUT_MS * 1000ULL;
    brd_rpm_adc_update();
    assert(!brd_rpm_adc_valid());
    fault = false;
    raw_value = 4095;
    test_clock_us += IR_RPM_ADC_SAMPLE_INTERVAL_MS * 1000ULL;
    brd_rpm_adc_update();
    assert(brd_rpm_adc_valid() && brd_rpm_adc_raw() == 4095U);
    puts("PASS: GPIO4 RPM ADC sampling and stale validity");
}
