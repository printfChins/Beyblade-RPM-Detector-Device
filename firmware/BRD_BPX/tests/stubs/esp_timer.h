#ifndef TEST_ESP_TIMER_H
#define TEST_ESP_TIMER_H
#include <Arduino.h>
inline int64_t esp_timer_get_time() { return static_cast<int64_t>(test_clock_us); }
#endif
