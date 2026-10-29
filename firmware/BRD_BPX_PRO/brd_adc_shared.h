/* 檔案: BRD_BPX_PRO/brd_adc_shared.h
 * [新增] ADC1 共用控制器：電池 GPIO0、LOAD GPIO1 與 RPM ADC GPIO4 不可各自建立 ADC1 oneshot unit。
 */
#ifndef BRD_ADC_SHARED_H
#define BRD_ADC_SHARED_H
#include <esp_err.h>
#include <hal/adc_types.h>
namespace brd_adc {
/* 先註冊 ADC 通道，再建立該通道的獨立校正實例。 */
esp_err_t configure(int gpio, adc_atten_t attenuation, adc_bitwidth_t bitwidth);
esp_err_t calibrate(int gpio);
/* [新增] 只讀 ADC 原始值，不要求校正；IR TEST 條狀圖專用。 */
esp_err_t read_raw(int gpio, int &raw);
esp_err_t read_mv(int gpio, int &raw, int &millivolts);
}
#endif
