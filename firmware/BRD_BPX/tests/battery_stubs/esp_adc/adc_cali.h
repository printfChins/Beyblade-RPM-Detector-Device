/* [V1.4 新增] battery host test only, calibrated ADC is simulated at VBAT / 2 */
#pragma once
#include <esp_adc/adc_oneshot.h>
using adc_cali_handle_t = void *;
struct adc_cali_curve_fitting_config_t { adc_unit_t unit_id; adc_channel_t chan; int atten; adc_bitwidth_t bitwidth; };
inline esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t,int raw,int *mv) { *mv=raw; return ESP_OK; }
