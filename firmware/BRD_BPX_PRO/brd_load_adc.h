/* 檔案: BRD_BPX_PRO/brd_load_adc.h
 * [修改] GPIO1 原始 ADC < 4000 分類；100ms 去抖由 brd_measurement 處理。
 */
#ifndef BRD_LOAD_ADC_H
#define BRD_LOAD_ADC_H
#include <Arduino.h>
#include <esp_err.h>
void brd_load_adc_begin(void);
void brd_load_adc_update(void);
bool brd_load_adc_valid(void);
bool brd_load_adc_active(void);
uint16_t brd_load_adc_voltage_mv(void);
/* [新增] 最近一次有效 GPIO1 ADC 原始值；使用前檢查 brd_load_adc_valid()。 */
uint16_t brd_load_adc_raw(void);
esp_err_t brd_load_adc_last_error(void);
#endif
