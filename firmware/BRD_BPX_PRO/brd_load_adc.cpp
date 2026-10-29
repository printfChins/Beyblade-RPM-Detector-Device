/* 檔案: BRD_BPX_PRO/brd_load_adc.cpp
 * [新增] ADC1_CH1 GPIO1 LOAD 類比偵測。
 * [修改] raw ADC < 4000 為 ACTIVE；不再依 mV 分類。
 * [保留] 20 ms 取樣、100 ms 狀態去抖；超時視為未裝載。
 * [刪減] 不讀 GPIO 數位電位，不在 ISR 中執行 ADC。
 */
#include "brd_config.h"
#include "brd_adc_shared.h"
#include "brd_load_adc.h"
namespace {
bool attempted = false;
bool valid = false;
bool active = false;
bool ever_valid = false;
uint16_t voltage_mv = 0;
uint16_t raw_value = 0;
uint32_t last_attempt_ms = 0;
uint32_t last_success_ms = 0;
esp_err_t error = ESP_OK;
}
void brd_load_adc_begin(void) {
    attempted = false;
    valid = false;
    active = false;
    ever_valid = false;
    voltage_mv = 0;
    raw_value = 0;
    last_attempt_ms = 0;
    last_success_ms = 0;
    error = ESP_OK;
}
void brd_load_adc_update(void) {
    const uint32_t now = millis();
    if (attempted && (uint32_t)(now - last_attempt_ms) < LOAD_ADC_SAMPLE_INTERVAL_MS) {
        return;
    }
    attempted = true;
    last_attempt_ms = now;
    error = brd_adc::configure(LOAD_IR_GPIO, LOAD_ADC_ATTENUATION,
                               (adc_bitwidth_t)LOAD_ADC_RESOLUTION_BITS);
    if (error == ESP_OK) { error = brd_adc::calibrate(LOAD_IR_GPIO); }
    int raw = 0;
    int mv = 0;
    if (error == ESP_OK) { error = brd_adc::read_mv(LOAD_IR_GPIO, raw, mv); }
    if (error == ESP_OK && raw >= 0 && raw <= 4095 && mv >= 0) {
        voltage_mv = (uint16_t)mv;
        raw_value = (uint16_t)raw;
        valid = true;
        ever_valid = true;
        last_success_ms = now;
        /* [修改] 原始 ADC <4000 為裝載；等於 4000 必須是未裝載。 */
        active = raw_value < LOAD_ADC_ACTIVE_RAW_THRESHOLD;
    } else {
        if (error == ESP_OK) { error = ESP_ERR_INVALID_RESPONSE; }
        if (!ever_valid || (uint32_t)(now - last_success_ms) >= LOAD_ADC_STALE_TIMEOUT_MS) {
            valid = false;
            active = false; /* [安全] 故障時不觸發新裝載啟動。 */
        }
    }
}
bool brd_load_adc_valid(void) { return valid; }
bool brd_load_adc_active(void) { return valid && active; }
uint16_t brd_load_adc_voltage_mv(void) { return voltage_mv; }
uint16_t brd_load_adc_raw(void) { return raw_value; }
esp_err_t brd_load_adc_last_error(void) { return error; }
