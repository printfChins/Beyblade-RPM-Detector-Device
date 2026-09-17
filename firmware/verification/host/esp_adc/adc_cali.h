#pragma once
#include <esp_err.h>
using adc_cali_handle_t = void*;
extern int test_cali_error, test_adc_mv;
inline esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t, int, int *mv) { *mv=test_adc_mv; return test_cali_error; }
