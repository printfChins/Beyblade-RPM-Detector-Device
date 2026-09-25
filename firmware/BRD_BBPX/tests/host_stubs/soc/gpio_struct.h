/* 檔案位置: BRD_BBP/tests/host_stubs/soc/gpio_struct.h
   [BRD_BBP 新增] 宿主驗證，不參與Arduino韌體編譯。 */
#pragma once
#include <cstdint>
struct host_gpio_in_t { uint32_t val; };
struct host_gpio_t { host_gpio_in_t in; };
extern host_gpio_t GPIO;
