#ifndef TEST_ESP_SYSTEM_H
#define TEST_ESP_SYSTEM_H
extern unsigned test_restart_count;
inline void esp_restart() { test_restart_count++; }
#endif
