/* [修改] GPIO1 ADC 3999/4000 原始閾值、取樣與失效安全。 */
#include <cassert>
#include <cstdio>
#include <Arduino.h>
#include "brd_config.h"
#include "brd_load_adc.h"
#include "brd_adc_shared.h"
uint64_t test_clock_us = 100000U;
static int raw_input = 4095;
static int mv = 2650;
static bool fault = false;
namespace brd_adc {
esp_err_t configure(int, adc_atten_t, adc_bitwidth_t) { return fault ? ESP_ERR_INVALID_STATE : ESP_OK; }
esp_err_t calibrate(int) { return ESP_OK; }
esp_err_t read_mv(int, int &raw, int &out) {
    raw = raw_input;
    out = mv;
    return fault ? ESP_ERR_INVALID_RESPONSE : ESP_OK;
}
}
static void sample_at(int value, unsigned ms) {
    raw_input = value;
    test_clock_us += ms * 1000ULL;
    brd_load_adc_update();
}
int main() {
    brd_load_adc_begin();
    brd_load_adc_update();
    assert(brd_load_adc_valid() && !brd_load_adc_active());
    sample_at(4000, 20); assert(!brd_load_adc_active());
    sample_at(3999, 20); assert(brd_load_adc_active());
    sample_at(0, 20); assert(brd_load_adc_active());
    sample_at(4095, 20); assert(!brd_load_adc_active());
    sample_at(3999, 20); assert(brd_load_adc_active());
    fault = true;
    sample_at(3999, 20); assert(brd_load_adc_active());
    sample_at(3999, LOAD_ADC_STALE_TIMEOUT_MS + 20);
    assert(!brd_load_adc_valid() && !brd_load_adc_active());
    fault = false;
    sample_at(3999, 20); assert(brd_load_adc_valid() && brd_load_adc_active());
    puts("PASS: LOAD ADC raw <4000, threshold boundary and stale fail-safe.");
}
