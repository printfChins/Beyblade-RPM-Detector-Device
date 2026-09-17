/* [V1.10 新增] 主機睡眠替身；僅驗證呼叫順序與喚醒腳位。 */
#pragma once
#include <Arduino.h>
#include <esp_err.h>
#define ESP_SLEEP_WAKEUP_ALL 0
#define ESP_GPIO_WAKEUP_GPIO_HIGH 1
extern int mock_sleep_error;
extern int mock_sleep_calls;
extern uint64_t mock_sleep_wake_mask;
inline esp_err_t esp_sleep_disable_wakeup_source(int) { return ESP_OK; }
inline esp_err_t esp_deep_sleep_enable_gpio_wakeup(uint64_t mask, int mode) {
    assert(mode == ESP_GPIO_WAKEUP_GPIO_HIGH);
    mock_sleep_wake_mask = mask;
    return mock_sleep_error;
}
inline void esp_deep_sleep_start(void) { mock_sleep_calls++; }
