/* 檔案: BRD_BPX_PRO/brd_adc_shared.cpp
 * [新增] 唯一 ADC1 oneshot owner；三個通道在主 loop 依序取樣。
 * 不在中斷中執行 ADC read，也不解除他方通道的設定。
 */
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include "brd_config.h"
#include "brd_adc_shared.h"
namespace brd_adc {
namespace {
struct Channel {
    int gpio;
    adc_channel_t channel;
    adc_atten_t attenuation;
    adc_bitwidth_t bitwidth;
    bool configured;
    adc_cali_handle_t cali;
};
/* [新增] 專用通道：GPIO0 電池、GPIO1 LOAD、GPIO4 RPM 光學強度。 */
Channel channels[3] = {
    {BATTERY_ADC_GPIO, ADC_CHANNEL_0, BATTERY_ADC_ATTENUATION,
     (adc_bitwidth_t)BATTERY_ADC_RESOLUTION_BITS, false, nullptr},
    {LOAD_IR_GPIO, ADC_CHANNEL_1, LOAD_ADC_ATTENUATION,
     (adc_bitwidth_t)LOAD_ADC_RESOLUTION_BITS, false, nullptr},
    {IR_RPM_ADC_GPIO, ADC_CHANNEL_4, IR_RPM_ADC_ATTENUATION,
     (adc_bitwidth_t)IR_RPM_ADC_RESOLUTION_BITS, false, nullptr}
};
adc_oneshot_unit_handle_t adc1 = nullptr;
Channel *lookup(int gpio) {
    for (Channel &channel : channels) {
        if (channel.gpio == gpio) { return &channel; }
    }
    return nullptr;
}
}
esp_err_t configure(int gpio, adc_atten_t attenuation, adc_bitwidth_t bitwidth) {
    Channel *slot = lookup(gpio);
    if (!slot) { return ESP_ERR_INVALID_ARG; }
    if (slot->configured) {
        return (slot->attenuation == attenuation && slot->bitwidth == bitwidth) ?
            ESP_OK : ESP_ERR_INVALID_STATE;
    }
    adc_unit_t unit;
    adc_channel_t channel;
    esp_err_t err = adc_oneshot_io_to_channel(gpio, &unit, &channel);
    if (err != ESP_OK) { return err; }
    if (unit != ADC_UNIT_1) { return ESP_ERR_NOT_SUPPORTED; }
    if (!adc1) {
        adc_oneshot_unit_init_cfg_t cfg = {};
        cfg.unit_id = ADC_UNIT_1;
        cfg.ulp_mode = ADC_ULP_MODE_DISABLE;
        err = adc_oneshot_new_unit(&cfg, &adc1);
        if (err != ESP_OK) { return err; }
    }
    adc_oneshot_chan_cfg_t cfg = {};
    cfg.atten = attenuation;
    cfg.bitwidth = bitwidth;
    err = adc_oneshot_config_channel(adc1, channel, &cfg);
    if (err != ESP_OK) { return err; }
    err = gpio_set_pull_mode((gpio_num_t)gpio, GPIO_FLOATING);
    if (err != ESP_OK) { return err; }
    slot->channel = channel;
    slot->attenuation = attenuation;
    slot->bitwidth = bitwidth;
    slot->configured = true;
    return ESP_OK;
}
esp_err_t calibrate(int gpio) {
    Channel *slot = lookup(gpio);
    if (!slot || !slot->configured) { return ESP_ERR_INVALID_STATE; }
    if (slot->cali) { return ESP_OK; }
    adc_cali_curve_fitting_config_t cfg = {};
    cfg.unit_id = ADC_UNIT_1;
    cfg.chan = slot->channel;
    cfg.atten = slot->attenuation;
    cfg.bitwidth = slot->bitwidth;
    return adc_cali_create_scheme_curve_fitting(&cfg, &slot->cali);
}
esp_err_t read_raw(int gpio, int &raw) {
    Channel *slot = lookup(gpio);
    if (!slot || !slot->configured) { return ESP_ERR_INVALID_STATE; }
    esp_err_t err = adc_oneshot_read(adc1, slot->channel, &raw);
    if (err != ESP_OK) { return err; }
    if (raw < 0 || raw > ((1 << slot->bitwidth) - 1)) { return ESP_ERR_INVALID_RESPONSE; }
    return ESP_OK;
}
esp_err_t read_mv(int gpio, int &raw, int &millivolts) {
    Channel *slot = lookup(gpio);
    if (!slot || !slot->configured || !slot->cali) { return ESP_ERR_INVALID_STATE; }
    esp_err_t err = read_raw(gpio, raw);
    if (err != ESP_OK) { return err; }
    err = adc_cali_raw_to_voltage(slot->cali, raw, &millivolts);
    if (err != ESP_OK) { return err; }
    return millivolts >= 0 ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}
}
