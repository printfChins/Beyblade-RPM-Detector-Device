/* 檔案: BRD_BPX_PRO/brd_rpm_adc.cpp
 * [新增] GPIO4 12-bit ADC 原始值，50 ms 週期，僅供 IR TEST 頁面使用。
 * [保留] ADC1 oneshot unit 由 brd_adc_shared 唯一管理。
 * [安全] 取樣失敗/資料逾時顯示 ----，不使用無效值填條狀圖。
 */
#include <Arduino.h>
#include "brd_config.h"
#include "brd_adc_shared.h"
#include "brd_rpm_adc.h"
namespace {
bool attempted = false;
bool valid = false;
uint16_t last_raw = 0;
uint32_t last_attempt_ms = 0;
uint32_t last_success_ms = 0;
}
void brd_rpm_adc_begin(void) {
    attempted = false;
    valid = false;
    last_raw = 0;
    last_attempt_ms = 0;
    last_success_ms = 0;
}
void brd_rpm_adc_update(void) {
    const uint32_t now = millis();
    if (attempted && (uint32_t)(now - last_attempt_ms) < IR_RPM_ADC_SAMPLE_INTERVAL_MS) { return; }
    attempted = true;
    last_attempt_ms = now;
    const esp_err_t err = brd_adc::configure(IR_RPM_ADC_GPIO, IR_RPM_ADC_ATTENUATION,
                                              (adc_bitwidth_t)IR_RPM_ADC_RESOLUTION_BITS);
    int raw = 0;
    if (err == ESP_OK && brd_adc::read_raw(IR_RPM_ADC_GPIO, raw) == ESP_OK &&
        raw >= 0 && raw <= 4095) {
        valid = true;
        last_raw = (uint16_t)raw;
        last_success_ms = now;
        return;
    }
    if ((uint32_t)(now - last_success_ms) >= IR_RPM_ADC_STALE_TIMEOUT_MS) {
        valid = false;
    }
}
bool brd_rpm_adc_valid(void) { return valid; }
uint16_t brd_rpm_adc_raw(void) { return last_raw; }
