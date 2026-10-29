#ifndef TEST_GPIO_H
#define TEST_GPIO_H
#include <esp_err.h>
#include <Arduino.h>
using gpio_num_t = int;
#define GPIO_FLOATING 0
#define GPIO_PULLUP_ONLY 1
inline int gpio_set_pull_mode(int,int) { return ESP_OK; }
inline int gpio_set_level(int,int) { return ESP_OK; }
inline int gpio_get_level(int) { return 1; }
using gpio_isr_t = void (*)(void *);
#define GPIO_INTR_ANYEDGE 3
inline gpio_isr_t test_gpio_handlers[32] = {};
inline void *test_gpio_args[32] = {};
inline int test_gpio_install_flags = 0;
inline int test_gpio_install_result = ESP_OK;
inline void test_gpio_rpm_dispatch() { if(test_gpio_handlers[3]) { test_gpio_handlers[3](test_gpio_args[3]); } }
inline void test_gpio_load_dispatch() { if(test_gpio_handlers[1]) { test_gpio_handlers[1](test_gpio_args[1]); } }
inline int gpio_install_isr_service(int flags) { test_gpio_install_flags=flags; return test_gpio_install_result; }
inline int gpio_set_intr_type(int,int) { return ESP_OK; }
inline int gpio_intr_enable(int pin) { test_interrupts[pin]=pin==3 ? test_gpio_rpm_dispatch : test_gpio_load_dispatch; return ESP_OK; }
inline int gpio_intr_disable(int pin) { test_interrupts[pin]=nullptr; return ESP_OK; }
inline int gpio_isr_handler_add(int pin,gpio_isr_t handler,void *arg) { test_gpio_handlers[pin]=handler; test_gpio_args[pin]=arg; return ESP_OK; }
inline int gpio_isr_handler_remove(int pin) { test_gpio_handlers[pin]=nullptr; test_interrupts[pin]=nullptr; return ESP_OK; }
#endif
