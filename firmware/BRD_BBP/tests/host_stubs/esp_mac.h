/* 檔案位置: BRD_BBP/tests/host_stubs/esp_mac.h
   [BRD_BBP 新增] 宿主驗證，不參與Arduino韌體編譯。 */
#pragma once
#include "esp_err.h"
#include <cstdint>
inline esp_err_t esp_efuse_mac_get_default(uint8_t *p){for(int i=0;i<6;i++)p[i]=uint8_t(i+1);return ESP_OK;}
