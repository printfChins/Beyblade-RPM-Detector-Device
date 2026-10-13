#ifndef TEST_ADC_CALI_H
#define TEST_ADC_CALI_H
#include <esp_err.h>
using adc_cali_handle_t = void *;
extern int test_adc_voltage_mv;
inline esp_err_t adc_cali_raw_to_voltage(void *,int,int *mv) {
    *mv = test_adc_voltage_mv; return ESP_OK;
}
#endif
