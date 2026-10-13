/* [新增] 僅提供 ADC 硬體邊界；電池換算及保護使用真正模組。 */
#ifndef TEST_ADC_ONESHOT_H
#define TEST_ADC_ONESHOT_H
#include <esp_err.h>
using adc_oneshot_unit_handle_t = void *;
using adc_unit_t = int;
using adc_channel_t = int;
using adc_bitwidth_t = int;
#define ADC_UNIT_1 1
#define ADC_CHANNEL_0 0
#define ADC_ULP_MODE_DISABLE 0
extern esp_err_t test_adc_error;
struct adc_oneshot_unit_init_cfg_t { int unit_id; int ulp_mode; };
struct adc_oneshot_chan_cfg_t { int atten; adc_bitwidth_t bitwidth; };
inline esp_err_t adc_oneshot_io_to_channel(int,adc_unit_t *unit,adc_channel_t *channel) {
    *unit = ADC_UNIT_1; *channel = ADC_CHANNEL_0; return ESP_OK;
}
inline esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *,void **out) {
    *out = reinterpret_cast<void *>(1); return ESP_OK;
}
inline esp_err_t adc_oneshot_config_channel(void *,int,const adc_oneshot_chan_cfg_t *) {
    return ESP_OK;
}
inline esp_err_t adc_oneshot_read(void *,int,int *raw) {
    *raw = 2000; return test_adc_error;
}
#endif
