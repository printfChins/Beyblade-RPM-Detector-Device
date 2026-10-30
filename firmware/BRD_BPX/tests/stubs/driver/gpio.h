#ifndef TEST_GPIO_H
#define TEST_GPIO_H
#include <esp_err.h>
using gpio_num_t = int;
#define GPIO_FLOATING 0
#define GPIO_PULLUP_ONLY 1
inline int gpio_set_pull_mode(int,int) { return ESP_OK; }
inline int gpio_set_level(int,int) { return ESP_OK; }
inline int gpio_get_level(int) { return 1; }
#endif
