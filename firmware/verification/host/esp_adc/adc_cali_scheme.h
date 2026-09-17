#pragma once
#include <hal/adc_types.h>
#include <esp_adc/adc_cali.h>
struct adc_cali_curve_fitting_config_t { adc_unit_t unit_id; adc_channel_t chan; adc_atten_t atten; adc_bitwidth_t bitwidth; };
extern int test_cali_create_error;
inline esp_err_t adc_cali_create_scheme_curve_fitting(const adc_cali_curve_fitting_config_t*, adc_cali_handle_t *cali) {
    if (test_cali_create_error) return test_cali_create_error;
    *cali=reinterpret_cast<void*>(1); return ESP_OK;
}
