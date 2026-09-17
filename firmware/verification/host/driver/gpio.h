#pragma once
#include <soc/gpio_struct.h>
#include <esp_err.h>
using gpio_num_t = int;
#define GPIO_FLOATING 0
#define GPIO_PULLUP_ONLY 1
extern int test_pull_modes[32];
extern int test_gpio_error;
inline esp_err_t gpio_set_pull_mode(gpio_num_t pin, int mode) { test_pull_modes[pin]=mode; return test_gpio_error; }
inline esp_err_t gpio_set_level(gpio_num_t, int) { return 0; }
inline int gpio_get_level(gpio_num_t pin) { return (GPIO.in.val >> pin) & 1; }
