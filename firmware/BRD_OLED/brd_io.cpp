/*
    檔案位置: BRD_OLED_V0.10/brd_io.cpp
    [V0.10 修正] 充電 DET 使用內部上拉，其餘專案輸入腳不啟用內部上下拉。
    [V0.10 恢復] 充電 DET 讀取，提供 OLED 充電圖示狀態。
    [V0.10 刪減] LED 跟隨功能；GPIO8 固定關燈。
*/
#include <driver/gpio.h>

#include "brd_config.h"
#include "brd_io.h"

static void input_without_pull(uint8_t pin) {
    pinMode(pin, INPUT);
    (void)gpio_set_pull_mode((gpio_num_t)pin, GPIO_FLOATING);
}

void brd_io_begin(void) {
    input_without_pull(RPM_IR_GPIO);
    input_without_pull(LOAD_IR_GPIO);

    /* [V0.10 恢復] GPIO0 作電池 ADC；初始化時不啟用內部上下拉。 */
    input_without_pull(BATTERY_ADC_GPIO);

    /* [V0.10 修正] GPIO10 使用內部上拉，關閉內部下拉。 */
    pinMode(CHRG_DET_GPIO, CHRG_DET_INPUT_MODE);
    (void)gpio_set_pull_mode((gpio_num_t)CHRG_DET_GPIO, GPIO_PULLUP_ONLY);

    /* [V0.10 新增] I2C 啟動前先清除 SDA/SCL 可能殘留的內部上下拉。 */
    input_without_pull(I2C_SDA_GPIO);
    input_without_pull(I2C_SCL_GPIO);

    /* [V0.10 修改] 先設定輸出鎖存電位，再切換為 OUTPUT，固定關閉 LED。 */
    (void)gpio_set_level((gpio_num_t)STATUS_LED_GPIO, STATUS_LED_INACTIVE_LEVEL);
    pinMode(STATUS_LED_GPIO, OUTPUT);
    (void)gpio_set_pull_mode((gpio_num_t)STATUS_LED_GPIO, GPIO_FLOATING);
    (void)gpio_set_level((gpio_num_t)STATUS_LED_GPIO, STATUS_LED_INACTIVE_LEVEL);
}

bool brd_io_is_charging(void) {
    /* [V0.10 恢復] 僅讀取 DET 電位，不更動上拉模式或 LED 狀態。 */
    return gpio_get_level((gpio_num_t)CHRG_DET_GPIO) == CHRG_ACTIVE_LEVEL;
}
