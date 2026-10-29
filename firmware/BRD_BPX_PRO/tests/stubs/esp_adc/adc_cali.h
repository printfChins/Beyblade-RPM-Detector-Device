#ifndef TEST_ADC_CALI_H
#define TEST_ADC_CALI_H
#include <esp_adc/adc_oneshot.h>
using adc_cali_handle_t = void*;
esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t,int raw,int *out_mv);
#endif
