#pragma once
#include <cstdint>
struct gpio_mock { struct { uint32_t val; } in; };
extern gpio_mock GPIO;
