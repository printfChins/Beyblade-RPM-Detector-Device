/* 檔案: BRD_BPX_PRO/brd_rpm_adc.h
 * [新增] GPIO4 ADC1_CH4 取樣介面，僅提供 IR TEST UI 顯示。
 * [注意] 與 GPIO3 RPM 脈衝輸入完全獨立，不改量測演算法。
 */
#ifndef BRD_RPM_ADC_H
#define BRD_RPM_ADC_H
#include <stdint.h>
void brd_rpm_adc_begin(void);
void brd_rpm_adc_update(void);
bool brd_rpm_adc_valid(void);
uint16_t brd_rpm_adc_raw(void);
#endif
