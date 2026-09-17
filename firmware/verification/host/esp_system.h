#pragma once
#include <stdexcept>
extern int test_restarts;
extern int mock_reset_reason;
typedef enum {
    ESP_RST_UNKNOWN = 0,
    ESP_RST_POWERON = 1,
    ESP_RST_SW = 3,
    ESP_RST_DEEPSLEEP = 5,
    ESP_RST_WDT = 7
} esp_reset_reason_t;
inline esp_reset_reason_t esp_reset_reason() { return static_cast<esp_reset_reason_t>(mock_reset_reason); }
[[noreturn]] inline void esp_restart() { test_restarts++; throw std::runtime_error("mock_reset"); }
