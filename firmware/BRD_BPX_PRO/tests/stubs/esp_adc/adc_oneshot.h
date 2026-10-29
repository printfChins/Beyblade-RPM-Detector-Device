#ifndef TEST_ADC_ONESHOT_H
#define TEST_ADC_ONESHOT_H
#include <esp_err.h>
#include <hal/adc_types.h>
using adc_oneshot_unit_handle_t = void*;
using adc_unit_t = int;
using adc_channel_t = int;
#define ADC_UNIT_1 1
#define ADC_CHANNEL_0 0
#define ADC_CHANNEL_1 1
#define ADC_CHANNEL_4 4
#define ADC_ULP_MODE_DISABLE 0
struct adc_oneshot_unit_init_cfg_t { adc_unit_t unit_id; int ulp_mode; };
struct adc_oneshot_chan_cfg_t { adc_atten_t atten; adc_bitwidth_t bitwidth; };
esp_err_t adc_oneshot_io_to_channel(int gpio, adc_unit_t*, adc_channel_t*);
esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t*, adc_oneshot_unit_handle_t*);
esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t,adc_channel_t,const adc_oneshot_chan_cfg_t*);
esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t,adc_channel_t,int*);
#endif
