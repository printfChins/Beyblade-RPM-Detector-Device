#include <stdint.h>
#include <esp_err.h>
inline int esp_efuse_mac_get_default(uint8_t *mac) {
    for(int i=0;i<6;i++) { mac[i]=static_cast<uint8_t>(i+1); }
    return ESP_OK;
}
