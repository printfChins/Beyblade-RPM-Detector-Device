/* [V1.4 新增] battery host test only, mock ESP-IDF ADC API */
#pragma once
#include <stdint.h>
#include <esp_err.h>
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_INVALID_RESPONSE 0x107
using adc_unit_t = int;
using adc_channel_t = int;
using adc_bitwidth_t = int;
#define ADC_UNIT_1 1
#define ADC_CHANNEL_0 0
#define ADC_ULP_MODE_DISABLE 0
using adc_oneshot_unit_handle_t = void *;
struct adc_oneshot_unit_init_cfg_t { adc_unit_t unit_id; int ulp_mode; };
struct adc_oneshot_chan_cfg_t { int atten; adc_bitwidth_t bitwidth; };
extern int test_battery_mv;
extern bool test_adc_fail;
inline esp_err_t adc_oneshot_io_to_channel(int, adc_unit_t *u, adc_channel_t *c) { *u=ADC_UNIT_1; *c=ADC_CHANNEL_0; return ESP_OK; }
inline esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *, adc_oneshot_unit_handle_t *h) { *h=(void*)0x1; return ESP_OK; }
inline esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t, adc_channel_t, const adc_oneshot_chan_cfg_t *) { return ESP_OK; }
inline esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t, adc_channel_t, int *raw) { if(test_adc_fail) return 1; *raw=test_battery_mv / 2; return ESP_OK; }
