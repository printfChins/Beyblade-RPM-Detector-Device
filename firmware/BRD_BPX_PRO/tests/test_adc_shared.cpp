/* [新增] 驗證 GPIO0、GPIO1 與 GPIO4 僅建立一份 ADC1 handle，各自校正與取樣。 */
#include <cassert>
#include <cstdio>
#include <stdint.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali_scheme.h>
#include "brd_config.h"
#include "brd_adc_shared.h"
static int unit_count = 0, channel_count = 0, calibration_count = 0;
esp_err_t adc_oneshot_io_to_channel(int gpio, adc_unit_t *unit, adc_channel_t *channel) {
    if (gpio != 0 && gpio != 1 && gpio != 4) { return ESP_ERR_INVALID_ARG; }
    *unit = ADC_UNIT_1; *channel = gpio;
    return ESP_OK;
}
esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t*, adc_oneshot_unit_handle_t *unit) {
    unit_count++;
    *unit = (void*)0x1;
    return ESP_OK;
}
esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t, adc_channel_t ch,
                                     const adc_oneshot_chan_cfg_t*) {
    assert(ch == 0 || ch == 1 || ch == 4);
    channel_count++;
    return ESP_OK;
}
esp_err_t adc_cali_create_scheme_curve_fitting(const adc_cali_curve_fitting_config_t *cfg,
                                               adc_cali_handle_t *cal) {
    calibration_count++;
    *cal = (void*)(uintptr_t)(cfg->chan+1);
    return ESP_OK;
}
esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t,adc_channel_t ch,int *raw) {
    *raw = ch == 0 ? 2000 : 1000;
    return ESP_OK;
}
esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t cal, int, int *mv) {
    *mv = (uintptr_t)cal == 1 ? 1900 : 890;
    return ESP_OK;
}
int main() {
    assert(brd_adc::configure(BATTERY_ADC_GPIO,BATTERY_ADC_ATTENUATION,12) == ESP_OK);
    assert(brd_adc::configure(LOAD_IR_GPIO,LOAD_ADC_ATTENUATION,12) == ESP_OK);
    assert(brd_adc::configure(LOAD_IR_GPIO,LOAD_ADC_ATTENUATION,12) == ESP_OK);
    assert(brd_adc::configure(IR_RPM_ADC_GPIO, IR_RPM_ADC_ATTENUATION, 12) == ESP_OK);
    assert(unit_count == 1 && channel_count == 3);
    assert(brd_adc::calibrate(BATTERY_ADC_GPIO) == ESP_OK);
    assert(brd_adc::calibrate(LOAD_IR_GPIO) == ESP_OK);
    assert(brd_adc::calibrate(BATTERY_ADC_GPIO) == ESP_OK);
    assert(calibration_count == 2);
    int raw, mv;
    assert(brd_adc::read_mv(BATTERY_ADC_GPIO,raw,mv) == ESP_OK && mv == 1900);
    assert(brd_adc::read_mv(LOAD_IR_GPIO,raw,mv) == ESP_OK && mv == 890);
    assert(brd_adc::read_raw(IR_RPM_ADC_GPIO,raw) == ESP_OK && raw == 1000);
    assert(brd_adc::configure(LOAD_IR_GPIO,LOAD_ADC_ATTENUATION,10) == ESP_ERR_INVALID_STATE);
    puts("PASS: shared ADC1 owner, GPIO0/GPIO1/GPIO4 configuration and channel isolation.");
}
