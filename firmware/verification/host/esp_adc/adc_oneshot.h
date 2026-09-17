#pragma once
#include <hal/adc_types.h>
#include <esp_err.h>
using adc_oneshot_unit_handle_t = void*;
struct adc_oneshot_unit_init_cfg_t { adc_unit_t unit_id; adc_ulp_mode_t ulp_mode; };
struct adc_oneshot_chan_cfg_t { adc_atten_t atten; adc_bitwidth_t bitwidth; };
extern int test_map_error, test_new_error, test_config_error, test_read_error;
extern int test_adc_raw, test_adc_reads, test_adc_units, test_adc_configs;
inline esp_err_t adc_oneshot_io_to_channel(int pin, adc_unit_t *unit, adc_channel_t *channel) {
    if (test_map_error) return test_map_error;
    if (pin!=0) return ESP_ERR_INVALID_ARG;
    *unit=ADC_UNIT_1; *channel=ADC_CHANNEL_0; return ESP_OK;
}
inline esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t*, adc_oneshot_unit_handle_t *unit) {
    if (test_new_error) return test_new_error;
    *unit=reinterpret_cast<void*>(1); test_adc_units++; return ESP_OK;
}
inline esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t, adc_channel_t, const adc_oneshot_chan_cfg_t *cfg) {
    test_adc_configs++;
    if (test_config_error) return test_config_error;
    return cfg->atten==ADC_ATTEN_DB_12 && cfg->bitwidth==ADC_BITWIDTH_12 ? ESP_OK : ESP_ERR_INVALID_ARG;
}
inline esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t, adc_channel_t, int *raw) {
    test_adc_reads++; *raw=test_adc_raw; return test_read_error;
}
