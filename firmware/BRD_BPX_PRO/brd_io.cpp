/* 檔案位置: BRD_BPX_PRO/brd_io.cpp
   [PRO 刪減] 原 GPIO8 狀態 LED 和 OLED I2C 初始化。
   [PRO 新增] GPIO6 電源維持、GPIO7 IR 開關、GPIO5 按鈕上拉。
   [保留] GPIO3 RPM / GPIO1 LOAD 無上下拉；GPIO10 充電內部上拉。
*/
#include <driver/gpio.h>
#include "brd_config.h"
#include "brd_io.h"
#include "brd_load_adc.h"
static void input_without_pull(uint8_t pin) {
    pinMode(pin, INPUT);
    (void)gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING);
}
static void output_level(uint8_t pin, uint8_t level) {
    /* [PRO 新增] 先設定鎖存位準，再設成輸出，減少切換脈衝。 */
    (void)gpio_set_level((gpio_num_t)pin, level);
    pinMode(pin, OUTPUT);
    (void)gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING);
    (void)gpio_set_level((gpio_num_t)pin, level);
}
void brd_io_begin(void) {
    /* [PRO 新增] 電源自鎖必須先於其他耗時初始化。 */
    output_level(SYS_POWER_GPIO, SYS_POWER_ACTIVE_LEVEL);
    output_level(IR_SWITCH_GPIO, (IR_SWITCH_ACTIVE_LEVEL == HIGH) ? LOW : HIGH);
    input_without_pull(RPM_IR_GPIO);
    input_without_pull(LOAD_IR_GPIO);
    input_without_pull(BATTERY_ADC_GPIO);
    input_without_pull(IR_RPM_ADC_GPIO); /* [PRO 新增] 預留 IR 模擬量輸入，不啟用 ADC 轉換。 */
    pinMode(CHRG_DET_GPIO, CHRG_DET_INPUT_MODE);
    (void)gpio_set_pull_mode((gpio_num_t)CHRG_DET_GPIO, GPIO_PULLUP_ONLY);
    pinMode(BUTTON_GPIO, BUTTON_INPUT_MODE);
    (void)gpio_set_pull_mode((gpio_num_t)BUTTON_GPIO, GPIO_PULLUP_ONLY);
    /* [PRO 新增] 進入量測流程前啟用 IR 發射器；上電未完成 ADC 檢查時不點亮。 */
}
void brd_io_set_ir_enabled(bool enabled) {
    const uint8_t off_level = (IR_SWITCH_ACTIVE_LEVEL == HIGH) ? LOW : HIGH;
    (void)gpio_set_level((gpio_num_t)IR_SWITCH_GPIO, enabled ? IR_SWITCH_ACTIVE_LEVEL : off_level);
}
bool brd_io_button_pressed(void) {
    /* [PRO 新增] 僅提供硬體鍵狀態；未定義短按/長按功能，不影響原協議。 */
    return gpio_get_level((gpio_num_t)BUTTON_GPIO) == BUTTON_ACTIVE_LEVEL;
}
bool brd_io_is_charging(void) {
    return gpio_get_level((gpio_num_t)CHRG_DET_GPIO) == CHRG_ACTIVE_LEVEL;
}
bool brd_io_read_ir1(void) {
    return gpio_get_level((gpio_num_t)RPM_IR_GPIO) == IR_TEST_TRIGGER_LEVEL;
}
bool brd_io_read_ir2(void) {
    /* [修改] IR2 與 LOAD 模式使用同一個 ADC 判定結果，不再讀取數位腳。 */
    brd_load_adc_update();
    return brd_load_adc_active();
}
